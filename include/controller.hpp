#pragma once
#include <variant>
#include <map>

#include <pistache/http.h>
#include <pistache/router.h>
#include "pistache/endpoint.h"

#include <nlohmann/json.hpp>

#include "exceptions.hpp"
#include "config_parser.hpp"
#include "utilities.hpp"
#include "post_body.hpp"
#include "detect.hpp"

namespace Controller {
  using std::string, std::string_view, std::optional, std::nullopt, std::map, 
    std::vector, std::make_optional;
  using Pistache::Rest::Request;
  using namespace Pistache;

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

  template <typename T>
  using to_json_t = decltype(std::declval<T>().to_json());

  template <typename T>
  using has_to_json = detect<T, to_json_t>;

  template <class T>
  nlohmann::json ModelToJson(T &model) {
    nlohmann::json json;
    if constexpr (has_to_json<T>{}) { 
      json = model.to_json(); 
    } else {
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
    }

    return json;
  }

  class Response {
    public:
      Response(unsigned int code, const string &content_type, 
        const string &body) : code_(code), content_type_(content_type), 
        body_(body) {};
      Response(unsigned int code, const string &content_type, 
        const string &body, 
        const vector<std::shared_ptr<Http::Header::Header>> &headers) :
        code_(code), content_type_(content_type), body_(body), headers_(headers) {};
      explicit Response(nlohmann::json body, unsigned int code = 200) :
        code_(code), content_type_("application/json; charset=utf8"), 
        body_(body.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore)) {};

      unsigned int code() { return code_; };
      string content_type() { return content_type_; };
      string body() { return body_; };

      void addHeader(std::shared_ptr<Http::Header::Header> header) {
        headers_.push_back(header);
      }
      vector<std::shared_ptr<Http::Header::Header>> headers() {
        return headers_; 
      };

      void send(Http::ResponseWriter &response) {
        if ( !Controller::GetConfig().cors_allow().empty() )
          response.headers().add(
            std::make_shared<Http::Header::AccessControlAllowOrigin>(
              Controller::GetConfig().cors_allow()));

        for (const auto & header : headers()) response.headers().add(header);

        response.send(static_cast<Http::Code>(code()), body(), 
          Http::Mime::MediaType::fromString(content_type())
        );
      };

    protected:
      unsigned int code_;
      string content_type_;
      string body_;
      vector<std::shared_ptr<Http::Header::Header>> headers_;
  };

  class CorsOkResponse : public Response{
    inline static const vector<string> DefaultMethods = {
      "GET", "POST", "PUT", "DELETE"};
    public:
      CorsOkResponse(const vector<string> &whichMethods = DefaultMethods);
  };

  class AuthorizeAll {
    public:
      bool is_authorized(const string &, const string &) {return true;}
      string authorizer_instance_label() { return "AuthorizeAll"; };
      static optional<AuthorizeAll> FromHeader(optional<string>) {
        return make_optional<AuthorizeAll>();
      }
  };

  class AuthorizeNone {
    public:
      bool is_authorized(const string &, const string &) { return false;}
      string authorizer_instance_label() { return "AuthorizeNone"; };
      static optional<AuthorizeNone> FromHeader(optional<string>) {
        return make_optional<AuthorizeNone>();
      }
  };

  class Instance { 
    public:
      typedef std::function<Response(const Request&)> Action;

      string controller_name, views_path;

      explicit Instance(const string &controller_name, const string &views_path) : 
        controller_name(controller_name), views_path(views_path), 
        logger(spdlog::get("server")) { 
        if (logger == nullptr)
          throw std::runtime_error("Unable to acquire controller logger");
      }

      virtual void route_action(string, const Request&, Http::ResponseWriter);

      template <typename... Args> static void static_checks() {
        static_assert(sizeof...(Args) == 1, "Function should take 1 parameter");
      }

      template <typename Result, typename Cls, typename... Args, typename Obj>
      static Rest::Route::Handler \
      bind(string action, Result (Cls::*func)(Args...), std::shared_ptr<Obj> objPtr) {
        static_checks<Args...>();

        if (objPtr->actions.count(action) > 0)
          throw RequestException("A \"{}\" action is being registered, and which "
            "already exists.", action);

        objPtr->actions[action] = [=](const Request &request) { 
          auto castPtr = std::dynamic_pointer_cast<Cls>(objPtr);
          return (castPtr.get()->*func)(request);
        };

        return [=](const Request &request, 
          Http::ResponseWriter response) {
          objPtr->route_action(action, request, std::move(response));
          return Rest::Route::Result::Ok;
        };
      }

      map<string, Action> actions;

    protected:
      std::shared_ptr<spdlog::logger> logger;
      string ensure_view_file(string, string);
      string ensure_view_file(string);
      string ensure_view_folder(string, string);
      string ensure_view_folder(string);
      void ensure_content_type(const Request &, Http::Mime::MediaType);

      template <typename TAuthorizer>
      TAuthorizer ensure_authorization(const Request& req, const string &action) {
        auto auth_header = req.headers().tryGet<Http::Header::Authorization>();

        optional<TAuthorizer> auth = TAuthorizer::FromHeader( 
          (auth_header) ? make_optional<string>(auth_header->value()) : nullopt);

        if (!auth.has_value())
          throw AccessDenied("Unable to fetch authorizer from Provided Header",
            "token_not_provided");
        
        if (!(*auth).is_authorized(controller_name, action))
          throw AccessDenied(
            "Authorization declined for user \"{}\"", "token_not_provided", 
            (*auth).authorizer_instance_label());

        return auth.value();
      }

      Response render_html(string, string);
      Response render_html(string, string, nlohmann::json);
      Response render_js(string, nlohmann::json);
      Response render_js(string);

      template <typename T>
      Response render_model_save_js(T &model, int success_code = 0, 
        int fail_code = -2) {
        auto json_errors = nlohmann::json::object();

        if (model.isValid())
          model.save();
        else {
          for (auto &attr_errors : model.errors() ) {
            string attr = (attr_errors.first) ? *attr_errors.first : "(General) Error";
            json_errors[attr] = nlohmann::json(attr_errors.second);
          }
        }

        return Response( (json_errors.size() > 0) ? 
          nlohmann::json({{"status", fail_code}, {"msg", json_errors }}) : 
          nlohmann::json({{"status", success_code}, {"id", *model.id()}})
        );
      }

      void send_fatal_response(Http::ResponseWriter &, 
        const Request&, const string);
  };
}
