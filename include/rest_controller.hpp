#pragma once
#include "controller.hpp"
#include "controller_factory.hpp"
#include "model.hpp"

#define REST_COLUMN_UPDATE(name, type) \
  if (post.has_scalar(#name)) model.name(post.operator[]<type>(#name));

namespace Controller {

using std::string, std::string_view, std::optional, std::nullopt, std::map, 
  std::vector, std::make_optional;
using ControllerPtr = std::shared_ptr<Controller::Instance>;
using Pistache::Rest::Request;
using Controller::Response;

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

template <class TController, class TModel, class TAuthorizer = AuthorizeAll>
class RestInstance : public Controller::Instance {
  public:
    static constexpr string_view rest_prefix = { "" };
    static constexpr string_view rest_actions[]= { "index", 
      "read", "create", "update", "delete", "multiple_update", 
      "multiple_delete" };

    RestInstance( const string &controller_name, const string &views_path) : 
      Controller::Instance::Instance(controller_name, views_path) { };

    static void Routes(Pistache::Rest::Router& r, ControllerPtr controller) {
      using namespace Pistache::Rest::Routes;
      using namespace Controller;
      using RestInstance_t = RestInstance<TController,TModel,TAuthorizer>;

      map<string, string> action_to_prefix;

      for(const auto &action : TController::actions()) {
        optional<string> p = TController::prefix(action);
        if (p.has_value()) action_to_prefix[action] = p.value();
      }

      if (action_to_prefix.count("index") > 0)
        Get(r, action_to_prefix["index"], 
          bind("index", &TController::index, controller));
      if (action_to_prefix.count("read") > 0)
        Get(r, action_to_prefix["read"]+"/:id", 
          bind("read", &RestInstance_t::read, controller));
      if (action_to_prefix.count("create") > 0)
        Post(r, action_to_prefix["create"],
          bind("create", &RestInstance_t::create_or_update, controller));
      if (action_to_prefix.count("update") > 0)
        Put(r, action_to_prefix["update"]+"/:id",
          bind("update", &RestInstance_t::create_or_update, controller));
      if (action_to_prefix.count("multiple_update") > 0)
        Post(r, action_to_prefix["multiple_update"]+"/multiple-update", 
          bind("multiple_update", &RestInstance_t::multiple_update, controller));
      if (action_to_prefix.count("multiple_delete") > 0)
        Post(r, action_to_prefix["multiple_delete"]+"/multiple-delete", 
          bind("multiple_delete", &RestInstance_t::multiple_delete, controller));
      if (action_to_prefix.count("delete") > 0)
        Delete(r, action_to_prefix["delete"]+"/:id",
          bind("delete", &RestInstance_t::del, controller));
      
      // TODO: Maybe we can just create a more clever bind above...
      //       Like: bind_response("options", CorsOkResponse(), controller)
      if ( !GetConfig().cors_allow().empty() ) {
        vector<string> cors_oks;
        auto offered_cors = [&cors_oks] (string name) -> bool {
          return (std::find(cors_oks.begin(), cors_oks.end(), name) != cors_oks.end());
        };

        // First we declare the non-wildcard paths
        for (const auto &action : vector<string>({"index","create","delete"}))
          if (action_to_prefix.count(action) > 0) {
            string prefix = action_to_prefix[action];
            if (!offered_cors(prefix)) {
              Options(r, prefix,
                bind("options_"+action, &RestInstance_t::options, controller));
              cors_oks.push_back(prefix);
            }
          }

        // Now we declare the sub-prefix paths:
        cors_oks.clear();

        for (const auto &action : vector<string>({"update","read","delete"})) {
          if (action_to_prefix.count(action) > 0) {
            string prefix = action_to_prefix[action];
            if (!offered_cors(prefix)) {
              Options(r, prefix+"/*",
                bind("options_"+action, &RestInstance_t::options, controller));
              cors_oks.push_back(prefix);
            }
          }
        }

        // These aren't * paths, but could have been covered by one of the * paths
        // just above. In any case, if they're not, create an options:
        for (const auto &multiple : vector<string>({"update","delete"})) {
          string action = multiple+"_update";
          if (action_to_prefix.count(action) > 0) {
            string prefix = action_to_prefix[action];
            if (!offered_cors(prefix)) {
              Options(r, prefix+"/multiple-"+multiple,
                bind("options_"+action, &RestInstance_t::options, controller));
              // NOTE: No benefit from pushing this onto cors_ok
            }
          }
        }
      }
    };

    static optional<string> prefix(const string &) { 
      string rp = {TController::rest_prefix.data(), TController::rest_prefix.size()};
      return make_optional<string>(rp);
    }

    static vector<string> actions() { 
      vector<string> ret;
      std::transform(std::begin(TController::rest_actions),
        std::end(TController::rest_actions), 
        std::back_inserter(ret), 
        [](string_view s) -> string {return {s.data(), s.size()};});

      return ret;
    }

    Response options(const Request& request) {
      ensure_authorization(request, "options");
      return Controller::CorsOkResponse();
    }

    Response index(const Request &request) {
      TAuthorizer authorizer = ensure_authorization(request, "index");
      auto ret = nlohmann::json::array();
      for (auto &m: model_index(authorizer)) 
        ret.push_back(Controller::ModelToJson(m));
      return Response(ret);
    }

    Response create_or_update(const Request& request) {
      ensure_content_type(request, MIME(Application, FormUrlEncoded));

      bool is_update = request.hasParam(":id");

      TAuthorizer authorizer = ensure_authorization(request, 
        (is_update) ? "update" : "create");

      auto post = Controller::PostBody(request.body());
      std::tm tm_time = Model::NowUTC();

      TModel model;
      if (is_update)  {
        auto optional_model = TModel::Find(request.param(":id").as<int>());
        if (!optional_model) 
          return Response(404, "text/html", Controller::GetConfig().html_error(404));
        model = *optional_model;
      } else 
        model = model_default(tm_time, authorizer);

      model_update(model, post, tm_time, authorizer);

      return render_model_save_js<TModel>(model);
    }

    Response multiple_update(const Request& request) {
      ensure_content_type(request, MIME(Application, FormUrlEncoded));
      TAuthorizer authorizer = ensure_authorization(request, "multiple_update");

      auto json_errors = nlohmann::json::object();
      auto post = Controller::PostBody(request.body());
      std::tm tm_time = Model::NowUTC();

      // It's possible that they've send a multiple update request... to change nothing:
      if (auto reqsize = post.size("request"); (reqsize && (*reqsize > 0))) {
        PostBody update = *post.postbody("request");

        post.each("ids", 
          [this, &authorizer, &tm_time, &post, &json_errors, &update]
          (const string &v) { 
            if (auto model = TModel::Find(stoi(v)); !model) 
              json_errors[v] = {"Record could not be found"};
            else {
              model_update(*model, update, tm_time, authorizer);

              if ((*model).isValid()) (*model).save();
              else json_errors[v] = {"Record invalid"};
            }
          });
      }

      // NOTE: This error behavior seems to be how the laravel code works, but it
      // doesn't seem to reflect in the gui....
      return Response( (json_errors.size() > 0) ? 
          nlohmann::json({{"status", -2}, {"msg", json_errors }}) : 
          nlohmann::json({{"status", 0}})
      );
    }

    Response multiple_delete(const Request& request) {
      ensure_content_type(request, MIME(Application, FormUrlEncoded));
      TAuthorizer authorizer = ensure_authorization(request, "multiple_delete");

      auto post = Controller::PostBody(request.body());
      post.each("ids", [this, &post, &authorizer](const string &v) { 
        if (auto model = TModel::Find(stoi(v)); model.has_value()) 
          model_delete(*model, authorizer);
      });

      return Response( nlohmann::json({{"status", 0}}) );
    }

    Response read(const Request& request) {
      TAuthorizer authorizer = ensure_authorization(request, "read");
      
      auto model = model_read(request.param(":id").as<int>(), authorizer);
      return (model.has_value()) ?
        Response(Controller::ModelToJson(*model)) : 
        Response(404, "text/html", Controller::GetConfig().html_error(404));
    }

    Response del(const Request& request) {
      TAuthorizer authorizer = ensure_authorization(request, "delete");

      auto model = TModel::Find(request.param(":id").as<int>());

      if (model.has_value() && model_delete(*model, authorizer))
        return Response( nlohmann::json({{"status", 0}}) );
      
      return Response(404, "text/html", Controller::GetConfig().html_error(404));
    }

  protected:
    // These must be overriden by inheriting classes, in order for the 
    // create/update actions to make any sense:
    virtual TModel model_default(std::tm, TAuthorizer &) { return TModel(); };
    virtual optional<TModel> model_read(int id, TAuthorizer &) {
      return TModel::Find(id);
    }
    virtual void model_update(TModel &, Controller::PostBody &, std::tm, TAuthorizer &) {}
    virtual bool model_delete(TModel &model, TAuthorizer &) {
      model.remove();
      return true;
    }
    virtual vector<TModel> model_index(TAuthorizer &) {
      return TModel::Select( fmt::format("select * from {table_name}", 
        fmt::arg("table_name", TModel::Definition.table_name)));
    }

    TAuthorizer ensure_authorization(const Request& req, const string &action) {
      auto auth_header = req.headers().tryGet<Pistache::Http::Header::Authorization>();

      optional<TAuthorizer> auth = TAuthorizer::FromHeader( (!auth_header) ? 
        nullopt : make_optional<string>(auth_header->value()));

      if (!auth.has_value())
        throw AccessDenied("Unabled to fetch authorizer FromHeader",
          "token_not_provided");
      
      if (!(*auth).is_authorized(controller_name, action))
        throw AccessDenied(
          "Authorization declined for user \"{}\"", "token_not_provided", 
          (*auth).authorizer_instance_label());

      return auth.value();
    };

};

}
