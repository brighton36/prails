#include <filesystem>
#include "controller.hpp"
#include "inja.hpp"

using namespace std;
using namespace Pistache;
using namespace prails::utilities;

void Controller::Instance::route_action(string action, const Rest::Request& request, 
  Http::ResponseWriter response) {

  try {
    Action action_fp = actions[action];
    if (actions.count(action) == 0)
      throw RequestException("Missing the requested action \"{}#{}\".", 
        controller_name, action);

    // This was copied out of pistache/src/common/http.cc, and seems to be needed
    // in order to get stringified requests from request.method()
    const vector<string> httpMethods = {             
      #define METHOD(repr, str) {str},
      HTTP_METHODS                              
      #undef METHOD                                 
    };                                            

    logger->debug("Routing: {} {} to {}#{} ({}) ", 
      httpMethods[static_cast<int>(request.method())], request.resource(), 
      controller_name, action, request.address().host() );

    action_fp(request).send(response);

  } catch(const RequestException &e) { 
    logger->error("route_action request exception: {}", e.what());
    response.send(Http::Code::Not_Found, Controller::GetConfig().html_error(404), MIME(Text, Html));
  } catch(const exception& e) { 
    logger->error("route_action general exception: {}", e.what());
    response.send(Http::Code::Internal_Server_Error, Controller::GetConfig().html_error(500), MIME(Text, Html));
  }
}

void Controller::Instance::ensure_content_type(const Rest::Request &request, 
  Http::Mime::MediaType mime) {

  auto content_type = request.headers().get<Http::Header::ContentType>();

  if ((!content_type) || (content_type->mime() != mime))
    throw RequestException("Unrecognized Content Type supplied to request. "
      "Expected \"application/x-www-form-urlencoded\" received {}", 
      content_type->mime().toString());
}

string Controller::Instance::ensure_view_folder(string foldername, string controller_folder) {
  string ret = views_path+"/"+controller_folder+"/"+foldername;

  if (!path_is_readable(ret) && filesystem::is_directory(ret))
    throw RequestException("Attemping to load, but unable to read directory {}.", ret);

  return ret;
}

string Controller::Instance::ensure_view_folder(string foldername) {
  return ensure_view_folder(foldername, controller_name);
}

string Controller::Instance::ensure_view_file(string filename, string controller_folder) {
  string ret = views_path+"/"+controller_folder+"/"+filename;

  if (!path_is_readable(ret) && filesystem::is_regular_file(ret))
    throw RequestException("Attemping to load, but unable to read file {}.", ret);

  return ret;
}

string Controller::Instance::ensure_view_file(string filename) {
  return ensure_view_file(filename, controller_name);
}

Controller::Response Controller::Instance::render_js(string action, nlohmann::json tmpl) {
  string view_file = ensure_view_file(action+".inja.js");

  inja::Environment env;

  env.add_callback("include_as_string", 1, [view_file](inja::Arguments& args) {
    auto filename = args.at(0)->get<string>();
    string include_file_path = string(filesystem::path(view_file).parent_path())+"/"+filename;
    return nlohmann::json{read_file(include_file_path)}[0].dump();
  });

  tmpl["controller"] = controller_name; 
  tmpl["action"] = action; 

  return Controller::Response(200, "text/javascript", env.render_file(view_file, tmpl));
}

Controller::Response Controller::Instance::render_html(string layout, string action, nlohmann::json tmpl) {
  string view_file = ensure_view_file(action+".inja.html");
  string layout_file = ensure_view_file(layout+".inja.html", "layouts");

  inja::Environment env;

  tmpl["controller"] = controller_name; 
  tmpl["action"] = action; 
  tmpl["layout"] = layout; 
  tmpl["content"]  = env.render_file(view_file, tmpl); 

  return Controller::Response(200, "text/html", env.render_file(layout_file, tmpl));
}

Controller::Response Controller::Instance::render_html(string layout, string action) {
  nlohmann::json tmpl;
  return render_html(layout, action, tmpl);
}

Controller::Response Controller::Instance::render_js(string action) {
  nlohmann::json tmpl;
  return render_js(action, tmpl);
}

Controller::CorsOkResponse::CorsOkResponse(const vector<string> &whichMethods) : 
  Response(200, "text/html", join(whichMethods, ", ")) {

  vector<Http::Method> allow;
  for (const auto &method : whichMethods) {
    if (method == "GET")
      allow.push_back(Http::Method::Get);
    else if (method == "POST")
      allow.push_back(Http::Method::Post);
    else if (method == "PUT")
      allow.push_back(Http::Method::Put);
    else if (method == "DELETE")
      allow.push_back(Http::Method::Delete);
    else if (method == "OPTIONS")
      allow.push_back(Http::Method::Options);
    else if (method == "HEAD")
      allow.push_back(Http::Method::Head);
    else if (method == "PATCH")
      allow.push_back(Http::Method::Patch);
    else if (method == "TRACE")
      allow.push_back(Http::Method::Trace);
    else if (method == "CONNECT")
      allow.push_back(Http::Method::Connect);
  }

  headers_ = {
    make_shared<Http::Header::AccessControlAllowHeaders>(
      "Content-Type, Access-Control-Allow-Headers, Authorization, X-Requested-With"), 
    make_shared<Http::Header::AccessControlAllowMethods>(body()), 
    make_shared<Http::Header::Allow>(allow)
  };
}

