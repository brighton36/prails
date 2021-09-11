#include <filesystem>

#include "server.hpp"
#include "controller_factory.hpp"
#include "model.hpp"
#include "model_factory.hpp"
#include "utilities.hpp"
#include "pistache_logger.hpp"
#include "mime_types.hpp"

using namespace std;
using namespace Pistache;
using namespace prails::utilities;

Server::Server(ConfigParser &config) : 
http_endpoint(make_shared<Http::Endpoint>(Address(config.address(), config.port()))) { 
  logger = config.setup_logger("server");
  html_error500 = config.html_error(500);
  html_error404 = config.html_error(404);

  this->path_static = config.static_resource_path();
  this->path_views = config.views_path();
  this->threads = config.threads();
  this->max_request_size = config.max_request_size();

  for (const auto &reg : ModelFactory::getModelNames())
    logger->trace("Found model \"{}\"", reg);

  for (const auto &reg : ControllerFactory::getControllerNames()) {
    logger->trace("Found controller \"{}\"", reg);
    controllers[reg] = shared_ptr<Controller::Instance>(
      ControllerFactory::createInstance(reg, this->path_views));
  }

  auto opts = Http::Endpoint::options()
    .threads(threads)
    .maxRequestSize(max_request_size)
    .flags(Tcp::Options::ReuseAddr)
    .logger(make_shared<StringToSpdLogger>(logger));

  http_endpoint->init(opts);
  setupRoutes();
}

void Server::start() {
  http_endpoint->setHandler(router.handler());
  http_endpoint->serve();
}

void Server::startThreaded() {
  http_endpoint->setHandler(router.handler());
  http_endpoint->serveThreaded();
}

void Server::shutdown() { 
  http_endpoint->shutdown(); 
}

void Server::setupRoutes() {
  using namespace Rest;

  for (auto &[reg, controller] : controllers)
    ControllerFactory::setRoutes(reg, router, controller);

  Routes::NotFound(router, Routes::bind(&Server::doNotFound, this));
}

void Server::doNotFound(const Rest::Request& request, Http::ResponseWriter response) {
  auto valid_path = regex("^[0-9 a-z\\-\\_\\.\\/]+$", regex_constants::icase);

  string resource = request.resource();

  if (!path_is_readable(this->path_static)) {
    logger->error("Static Resource path unreadable upon request: {}", resource);
    response.send(Http::Code::Not_Found, html_error404, MIME(Text, Html));
  } else {
    string local_path = filesystem::weakly_canonical(path_static+resource);
    
    // To Ensure they didn't ../ their way out of public. We test that the begining
    // of local_path matches path_static, and that the path's characters are sane.
    if (!starts_with(local_path,path_static) || (!regex_match(local_path, valid_path))) {
      logger->error("Resource Denied: {}", resource);
      response.send(Http::Code::Internal_Server_Error, html_error500, MIME(Text, Html));
    } else if ((!path_is_readable(local_path)) || (!filesystem::is_regular_file(local_path))) {
      logger->error("Resource Unreadable: {}", resource);
      response.send(Http::Code::Not_Found, html_error404, MIME(Text, Html));
    } else if (local_path != string(filesystem::canonical(local_path))) {
      // This should prevent us from following filesystem links. Note that 
      // canonical requires the path to exist, unlike weakly_canonical.
      logger->error("Resource Path Denied: {}", resource);
      response.send(Http::Code::Internal_Server_Error, html_error500, MIME(Text, Html));
    } else {
      logger->info("Serving: {}", resource);

      smatch matches;
      Http::Mime::MediaType mime_type;
      string res_ext;

      if (regex_search(local_path, matches, regex("([^.]+)$")))
        res_ext = matches[1];

      if (ExtToMime(res_ext).has_value()) {
        try {
          // Unfortunately, pistache has a very limited number of mime types that it
          // includes classes for. But, catch seems seems to work:
          mime_type = Http::Mime::MediaType::fromString(*ExtToMime(res_ext));
        } catch(const Http::HttpError& e) {
          logger->warn("Serving */* for the resource: {}", resource);
          mime_type = MIME(Star, Star);
        }
      } else {
        logger->warn("Unable to find a content type for the resource: {}", resource);
        mime_type = MIME(Text, Plain);
      }
      Http::serveFile(response, local_path, mime_type);
    }
  }
}

optional<string> Server::ExtToMime(const string &ext) {
  string ext_lower = ext;
  transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower); 

  static map<string, string> _extension_to_mime;
  if (_extension_to_mime.size() == 0)
    for (unsigned int i = 1; i < size(_default_mime_types); i+=2) {
      string key = {_default_mime_types[i-1].data(), _default_mime_types[i-1].size()};
      string val = {_default_mime_types[i].data(), _default_mime_types[i].size()};
      _extension_to_mime[key] = val;
    }

  return (_extension_to_mime.count(ext_lower)) ?
    make_optional<string>(_extension_to_mime[ext_lower]) : nullopt;
}
