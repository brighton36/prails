#pragma once
#include <variant>
#include <map>

#include <pistache/http.h>
#include <pistache/router.h>
#include "pistache/endpoint.h"

#include "inja.hpp"

#include "exceptions.hpp"
#include "config_parser.hpp"
#include "utilities.hpp"
#include "post_body.hpp"

namespace Controller {
  class Instance;

  ConfigParser inline GetConfig(ConfigParser *set_config = nullptr) { 
    static ConfigParser config;
    if (set_config != nullptr) config = *set_config;
    return config;
  }

  void inline Initialize(ConfigParser &config) {
    // TODO: I'm not crazy about this interface. But it works for now...
    // Think of something better than GetConfig(). Maybe try a procedure out
    // the way we did in model
    GetConfig((ConfigParser *)&config);
  }

  template <class T>
  nlohmann::json ModelToJson(T &model) {
    auto json = nlohmann::json();
    for (const auto &key : model.recordKeys())
      if (auto value = model.recordGet(key); value.has_value())
        std::visit([&key, &json](auto&& typeA) {
          using U = std::decay_t<decltype(typeA)>;
          if constexpr(std::is_same_v<U, std::tm>)
            json[key] = prails::utilities::tm_to_iso8601(typeA);
          else
            json[key] = typeA;
        }, value.value());
      else
        json[key] = nullptr;

    return json;
  }

  class Response {
    public:
      Response(unsigned int code, const std::string &content_type, 
        const std::string &body) : code_(code), content_type_(content_type), 
        body_(body) {};
      Response(unsigned int code, const std::string &content_type, 
        const std::string &body, 
        const std::vector<std::shared_ptr<Pistache::Http::Header::Header>> &headers) :
        code_(code), content_type_(content_type), body_(body), headers_(headers) {};
      explicit Response(nlohmann::json body, unsigned int code = 200) :
        code_(code), content_type_("application/json; charset=utf8"), 
        body_(body.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore)) {};

      unsigned int code() { return code_; };
      std::string content_type() { return content_type_; };
      std::string body() { return body_; };

      void addHeader(std::shared_ptr<Pistache::Http::Header::Header> header) {
        headers_.push_back(header);
      }
      std::vector<std::shared_ptr<Pistache::Http::Header::Header>> headers() {
        return headers_; 
      };

      void send(Pistache::Http::ResponseWriter &response) {
        if ( !Controller::GetConfig().cors_allow().empty() )
          response.headers().add(
            std::make_shared<Pistache::Http::Header::AccessControlAllowOrigin>(
              Controller::GetConfig().cors_allow()));

        for (const auto & header : headers()) response.headers().add(header);

        response.send(static_cast<Pistache::Http::Code>(code()), body(), 
          Pistache::Http::Mime::MediaType::fromString(content_type())
        );
      };

    protected:
      unsigned int code_;
      std::string content_type_;
      std::string body_;
      std::vector<std::shared_ptr<Pistache::Http::Header::Header>> headers_;
  };

  class CorsOkResponse : public Response{
    inline static const std::vector<std::string> DefaultMethods = {
      "GET", "POST", "PUT", "DELETE"};
    public:
      CorsOkResponse(const std::vector<std::string> &whichMethods = DefaultMethods);
  };

  class Instance { 
    public:
      typedef std::function<Controller::Response(const Pistache::Rest::Request&)> Action;

      std::string controller_name, views_path;

      explicit Instance(const std::string &controller_name, const std::string &views_path) : 
        controller_name(controller_name), views_path(views_path), 
        logger(spdlog::get("server")) { 
        if (logger == nullptr)
          throw std::runtime_error("Unable to acquire controller logger");
      }

      virtual void route_action(std::string, const Pistache::Rest::Request&, 
        Pistache::Http::ResponseWriter);

      template <typename... Args> static void static_checks() {
        static_assert(sizeof...(Args) == 1, "Function should take 1 parameter");
      }

      template <typename Result, typename Cls, typename... Args, typename Obj>
      static Pistache::Rest::Route::Handler \
      bind(std::string action, Result (Cls::*func)(Args...), std::shared_ptr<Obj> objPtr) {
        static_checks<Args...>();

        if (objPtr->actions.count(action) > 0)
          throw RequestException("A \"{}\" action is being registered, and which "
            "already exists.", action);

        objPtr->actions[action] = [=](const Pistache::Rest::Request &request) { 
          auto castPtr = std::dynamic_pointer_cast<Cls>(objPtr);
          return (castPtr.get()->*func)(request);
        };

        return [=](const Pistache::Rest::Request &request, 
          Pistache::Http::ResponseWriter response) {
          objPtr->route_action(action, request, std::move(response));
          return Pistache::Rest::Route::Result::Ok;
        };
      }

      std::map<std::string, Action> actions;

    protected:
      std::shared_ptr<spdlog::logger> logger;
      std::string ensure_view_file(std::string, std::string);
      std::string ensure_view_file(std::string);
      std::string ensure_view_folder(std::string, std::string);
      std::string ensure_view_folder(std::string);
      void ensure_content_type(const Pistache::Rest::Request &, Pistache::Http::Mime::MediaType);
      template <typename T>
      T ensure_authorization(const Pistache::Rest::Request &request) {
        auto authorization = request.headers().get<Pistache::Http::Header::Authorization>();
        std::smatch matches;

        if ((!authorization) || 
          (!authorization->hasMethod<Pistache::Http::Header::Authorization::Method::Bearer>())
        ) throw RequestException("This resource requires a valid authorization header");

        std::string authorization_value = authorization->value();
        if (!std::regex_search(authorization_value, matches, std::regex("^Bearer (.+)$")))
          throw RequestException("This resource requires a valid authorization header");

        auto ret = T::FromToken(matches[1]);
        if (!ret) throw RequestException("Invalid authorization supplied");

        return ret.value();
      }

      Controller::Response render_html(std::string, std::string);
      Controller::Response render_html(std::string, std::string, nlohmann::json);
      Controller::Response render_js(std::string, nlohmann::json);
      Controller::Response render_js(std::string);

      template <typename T>
      Controller::Response render_model_save_js(T &model, int success_code = 0, 
        int fail_code = -2) {
        auto json_errors = nlohmann::json::object();

        if (model.isValid())
          model.save();
        else {
          for (auto &attr_errors : model.errors() ) {
            std::string attr = (attr_errors.first) ? *attr_errors.first : "(General) Error";
            json_errors[attr] = nlohmann::json(attr_errors.second);
          }
        }

        return Controller::Response( (json_errors.size() > 0) ? 
          nlohmann::json({{"status", fail_code}, {"msg", json_errors }}) : 
          nlohmann::json({{"status", success_code}, {"id", *model.id()}})
        );
      }

  };
}
