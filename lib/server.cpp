
#include <filesystem>

#include "server.hpp"
#include "controller_factory.hpp"
#include "model.hpp"
#include "model_factory.hpp"
#include "utilities.hpp"
#include "pistache_logger.hpp"

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

  string mime_type_file = string(config.config_path())+"/mime_types.json"; 

  nlohmann::json mime_type_json = nlohmann::json::parse(ifstream(mime_type_file));
  for (auto& mime_type : mime_type_json.items())
    extension_to_mime[string(mime_type.key())] = mime_type.value();

  for (const auto &reg : ModelFactory::getModelNames()) {
    logger->trace("Found model \"{}\"", reg);
  }

  for (const auto &reg : ControllerFactory::getControllerNames()) {
    logger->trace("Found controller \"{}\"", reg);
    controllers[reg] = shared_ptr<Controller::Instance>(
      ControllerFactory::createInstance(reg, this->path_views));
  }

  auto opts = Http::Endpoint::options()
    .threads(threads)
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
      string extension;

      if (regex_search(local_path, matches, regex("([^.]+)$")))
        extension = matches[1];

      if ((extension.empty()) || (!extension_to_mime.count(extension))) {
        logger->warn("Unable to find a content type for the resource: {}", resource);
        mime_type = MIME(Text, Plain);
      } else {
        try {
          // Unfortunately, pistache has a very limited number of mime types that it
          // includes classes for. But, catch seems seems to work:
          mime_type = Http::Mime::MediaType::fromString(extension_to_mime[extension]);
        } catch(const Http::HttpError& e) {
          logger->warn("Serving */* for the resource: {}", resource);
          mime_type = MIME(Star, Star);
        }
      }
      Http::serveFile(response, local_path, mime_type);
    }
  }
}
