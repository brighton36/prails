#pragma once
#include "controller.hpp"
#include "controller_factory.hpp"
#include "model.hpp"

#define REST_COLUMN_UPDATE(name, type) \
  if (post.has_scalar(#name)) model.name(post.operator[]<type>(#name));

namespace Controller {
  template <class U, class T>
  class RestInstance : public Controller::Instance {
    public:
      static constexpr std::string_view rest_prefix = { "" };
      static constexpr std::string_view rest_actions[]= { "index", 
        "read", "create", "update", "delete", "multiple_update", 
        "multiple_delete" };

      RestInstance(const std::string &, const std::string &);
      static void Routes(Pistache::Rest::Router&, std::shared_ptr<Controller::Instance>);
      static std::optional<std::string> prefix(const std::string &);
      static std::vector<std::string> actions();

      Controller::Response options(const Pistache::Rest::Request&);
      Controller::Response index(const Pistache::Rest::Request&);
      Controller::Response create_or_update(const Pistache::Rest::Request&);
      Controller::Response multiple_update(const Pistache::Rest::Request&);
      Controller::Response multiple_delete(const Pistache::Rest::Request&);
      Controller::Response read(const Pistache::Rest::Request&);
      Controller::Response del(const Pistache::Rest::Request&);
    private:
			// These must be overriden by inheriting classes, in order for the 
	    // create/update actions to make any sense:
      virtual T modelDefault(std::tm) { return T(); };
			virtual void modelUpdate(T &, Controller::PostBody &, std::tm) {}
			virtual std::vector<T> modelSelect(Controller::PostBody &) {
        return T::Select( fmt::format("select * from {table_name}", 
          fmt::arg("table_name", T::Definition.table_name)));
      }
  };
}

template <class U, class T>
Controller::RestInstance<U,T>::RestInstance(
  const std::string &controller_name, const std::string &views_path) : 
  Controller::Instance::Instance(controller_name, views_path) { 

  // TODO: do I care to do this ...
  // This is just a courtesy to some dev (probably myself) in the future. Mostly
  // no trailing slash.
  /*
  if (!std::regex_match(U::prefix(), std::regex("^\\/.+[^\\/]$") ))
    throw RequestException("Error in rest class prefix. "
      "Perhaps there's a trailing slash?", U::prefix());
  */
}

template <class U, class T>
void Controller::RestInstance<U,T>::Routes(
  Pistache::Rest::Router& r, std::shared_ptr<Controller::Instance> controller) {

  using namespace Pistache::Rest::Routes;
  using namespace Controller;

  std::map<std::string, std::string> action_to_prefix;

  for(const auto &action : U::actions()) {
    std::optional<std::string> p = U::prefix(action);
    if (p.has_value()) action_to_prefix[action] = p.value();
  }

  if (action_to_prefix.count("index") > 0)
    Get(r, action_to_prefix["index"], 
      bind("index", &U::index, controller));
  if (action_to_prefix.count("read") > 0)
    Get(r, action_to_prefix["read"]+"/:id", 
      bind("read", &RestInstance<U,T>::read, controller));
  if (action_to_prefix.count("create") > 0)
    Post(r, action_to_prefix["create"],
      bind("create", &RestInstance<U,T>::create_or_update, controller));
  if (action_to_prefix.count("update") > 0)
    Put(r, action_to_prefix["update"]+"/:id",
      bind("update", &RestInstance<U,T>::create_or_update, controller));
  if (action_to_prefix.count("multiple_update") > 0)
    Post(r, action_to_prefix["multiple_update"]+"/multiple-update", 
      bind("multiple_update", &RestInstance<U,T>::multiple_update, controller));
  if (action_to_prefix.count("multiple_delete") > 0)
    Post(r, action_to_prefix["multiple_delete"]+"/multiple-delete", 
      bind("multiple_delete", &RestInstance<U,T>::multiple_delete, controller));
  if (action_to_prefix.count("delete") > 0)
    Delete(r, action_to_prefix["delete"]+"/:id",
      bind("del", &RestInstance<U,T>::del, controller));
  
  // TODO: Maybe we can just create a more clever bind above...
  //       Like: bind_response("options", CorsOkResponse(), controller)
  if ( !GetConfig().cors_allow().empty() ) {
    std::vector<std::string> cors_oks;
    auto offered_cors = [&cors_oks] (std::string name) -> bool {
      return (std::find(cors_oks.begin(), cors_oks.end(), name) != cors_oks.end());
    };

    // First we declare the non-wildcard paths
    for (const auto &action : std::vector<std::string>({"index","create","delete"}))
      if (action_to_prefix.count(action) > 0) {
        std::string prefix = action_to_prefix[action];
        if (!offered_cors(prefix)) {
          Options(r, prefix,
            bind("options_"+action, &RestInstance<U,T>::options, controller));
          cors_oks.push_back(prefix);
        }
      }

    // Now we declare the sub-prefix paths:
    cors_oks.clear();

    for (const auto &action : std::vector<std::string>({"update","read","delete"})) {
      if (action_to_prefix.count(action) > 0) {
        std::string prefix = action_to_prefix[action];
        if (!offered_cors(prefix)) {
          Options(r, prefix+"/*",
            bind("options_"+action, &RestInstance<U,T>::options, controller));
          cors_oks.push_back(prefix);
        }
      }
    }

    // These aren't * paths, but could have been covered by one of the * paths
    // just above. In any case, if they're not, create an options:
    for (const auto &multiple : std::vector<std::string>({"update","delete"})) {
      std::string action = multiple+"_update";
      if (action_to_prefix.count(action) > 0) {
        std::string prefix = action_to_prefix[action];
        if (!offered_cors(prefix)) {
          Options(r, prefix+"/multiple-"+multiple,
            bind("options_"+action, &RestInstance<U,T>::options, controller));
          // NOTE: No benefit from pushing this onto cors_ok
        }
      }
    }

  }
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::options(const Pistache::Rest::Request&) {
  return Controller::CorsOkResponse();
}

template <class U, class T>
Controller::Response 
Controller::RestInstance<U,T>::index(const Pistache::Rest::Request &request) {
  auto post = Controller::PostBody(request.body());
  auto ret = nlohmann::json::array();

  for (auto &m: modelSelect(post)) ret.push_back(Controller::ModelToJson(m));

  return Controller::Response(ret);
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

template <class U, class T>
std::optional<std::string> Controller::RestInstance<U,T>::prefix(const std::string &) { 
  std::string rp = {U::rest_prefix.data(), U::rest_prefix.size()};
  return std::make_optional<std::string>(rp);
}

template <class U, class T>
std::vector<std::string> Controller::RestInstance<U,T>::actions() { 
  std::vector<std::string> ret;

  std::transform(std::begin(U::rest_actions), std::end(U::rest_actions), 
    std::back_inserter(ret), 
    [](std::string_view s) -> std::string {return {s.data(), s.size()};});

  return ret;
}

