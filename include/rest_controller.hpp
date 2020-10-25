#pragma once
#include "controller.hpp"
#include "controller_factory.hpp"
#include "model.hpp"

namespace Controller {

  template <class U, class T>
  class RestInstance : public Controller::Instance {
    public:
      RestInstance(const std::string &, const std::string &);
      static void Routes(Pistache::Rest::Router&, std::shared_ptr<Controller::Instance>);

      Controller::Response options(const Pistache::Rest::Request&);
      Controller::Response index(const Pistache::Rest::Request&);
      Controller::Response create_or_update(const Pistache::Rest::Request&);
      Controller::Response multiple_update(const Pistache::Rest::Request&);
      Controller::Response multiple_delete(const Pistache::Rest::Request&);
      Controller::Response read(const Pistache::Rest::Request&);
      Controller::Response del(const Pistache::Rest::Request&);
    private:
      virtual T modelDefault(std::tm) { return T(); };
      virtual void modelUpdate(T &, Controller::PostBody &, std::tm) = 0;
  };
}

template <class U, class T>
Controller::RestInstance<U,T>::RestInstance(
  const std::string &controller_name, const std::string &views_path) : 
  Controller::Instance::Instance(controller_name, views_path) { 

  // This is just a courtesy to some dev (probably myself) in the future. Mostly
  // no trailing slash.
  if (!std::regex_match(U::route_prefix, std::regex("^\\/.+[^\\/]$") ))
    throw RequestException("Error in rest class prefix. "
      "Perhaps there's a trailing slash?", U::route_prefix);
}


template <class U, class T>
void Controller::RestInstance<U,T>::Routes(
  Pistache::Rest::Router& r, std::shared_ptr<Controller::Instance> controller) {

  using namespace Pistache::Rest::Routes;
  using namespace Controller;

  Get(r, U::route_prefix, bind("index", &RestInstance<U,T>::index, controller));
  Get(r, U::route_prefix+"/:id", bind("read", &RestInstance<U,T>::read, controller));
  Post(r, U::route_prefix, bind("create", &RestInstance<U,T>::create_or_update, controller));
  Put(r, U::route_prefix+"/:id", bind("update", &RestInstance<U,T>::create_or_update, controller));
  Post(r, U::route_prefix+"/multiple-update", 
    bind("multiple_update", &RestInstance<U,T>::multiple_update, controller));
  Post(r, U::route_prefix+"/multiple-delete", 
    bind("multiple_delete", &RestInstance<U,T>::multiple_delete, controller));
  Delete(r, U::route_prefix+"/:id", bind("del", &RestInstance<U,T>::del, controller));
  
  // TODO: Maybe we can just create a more clever bind above...
  //       Like: bind_response("options", CorsOkResponse(), controller)
  if ( !GetConfig().cors_allow().empty() ) {
    Options(r, U::route_prefix+"/*",
      bind("options_actions", &RestInstance<U,T>::options, controller));
    Options(r, U::route_prefix,
      bind("options_index", &RestInstance<U,T>::options, controller));
  }
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::options(const Pistache::Rest::Request&) {
  return Controller::CorsOkResponse();
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::index(const Pistache::Rest::Request&) {
  auto index = nlohmann::json::array();

  // TODO: Introduce a RestController::modelSelect(). pull the json requirement 
  // out of the model class, into this
  for (auto &model: T::Select(fmt::format("select * from {}", T::Definition.table_name)))
    index.push_back(Controller::ModelToJson(model));

  return Controller::Response(index);
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::read(const Pistache::Rest::Request& request) {
  auto id = request.param(":id").as<int>();
  auto model = T::Find(id);

  return (model) ? Controller::Response(Controller::ModelToJson(*model)) : 
    Controller::Response(404, "text/html", Controller::GetConfig().html_error(404));
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::del(const Pistache::Rest::Request& request) {
  if (auto model = T::Find(request.param(":id").as<int>()) ) {
    (*model).remove();
    return Controller::Response( nlohmann::json({{"status", 0}}) );
  }
  
  return Controller::Response(404, "text/html", Controller::GetConfig().html_error(404));
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::create_or_update(const Pistache::Rest::Request& request) {
  ensure_content_type(request, MIME(Application, FormUrlEncoded));

  auto post = Controller::PostBody(request.body());
  std::tm tm_time = Model::NowUTC();

  T model;
  if (request.hasParam(":id"))  {
    auto optional_model = T::Find(request.param(":id").as<int>());
    if (!optional_model) return Controller::Response(404, "text/html", Controller::GetConfig().html_error(404));
    model = *optional_model;
  } else 
    model = modelDefault(tm_time);

  modelUpdate(model, post, tm_time);

  return render_model_save_js<T>(model);
}

template <class U, class T>
Controller::Response
Controller::RestInstance<U,T>::multiple_update(const Pistache::Rest::Request& request) {
  ensure_content_type(request, MIME(Application, FormUrlEncoded));

  auto json_errors = nlohmann::json::object();
  auto post = Controller::PostBody(request.body());
  std::tm tm_time = Model::NowUTC();

  // It's possible that they've send a multiple update request... to change nothing:
  if (auto reqsize = post.size("request"); (reqsize && (*reqsize > 0))) {
    PostBody update = *post.postbody("request");

    post.each("ids", [this, &tm_time, &post, &json_errors, &update](const std::string &v) { 
      if (auto model = T::Find(stoi(v)); !model) 
        json_errors[v] = {"Record could not be found"};
      else {
        modelUpdate(*model, update, tm_time);

        if ((*model).isValid()) (*model).save();
        else json_errors[v] = {"Record invalid"};
      }
    });
}

  // NOTE: This error behavior seems to be how the laravel code works, but it
  // doesn't seem to reflect in the gui....
  return Controller::Response( (json_errors.size() > 0) ? 
      nlohmann::json({{"status", -2}, {"msg", json_errors }}) : 
      nlohmann::json({{"status", 0}})
  );
} 

template <class U, class T>
Controller::Response
Controller::RestInstance<U,T>::multiple_delete(const Pistache::Rest::Request& request) {
  ensure_content_type(request, MIME(Application, FormUrlEncoded));

  auto post = Controller::PostBody(request.body());
  post.each("ids", [&post](const std::string &v) { 
    if (auto model = T::Find(stoi(v)); model) (*model).remove();
  });

  return Controller::Response( nlohmann::json({{"status", 0}}) );
}

