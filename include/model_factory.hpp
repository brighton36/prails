#pragma once
#include "model.hpp"
#include "exceptions.hpp"

// NOTE: This probably needs to be re-worked into a class or struct:
#define INIT_MODEL_REGISTRY() \
  std::shared_ptr<ModelFactory::map_type> ModelFactory::models = std::make_shared<ModelFactory::map_type>(); \
  std::shared_ptr<ModelFactory::dsn_type> ModelFactory::dsns = std::make_shared<ModelFactory::dsn_type>(); \
  ModelFactory::Logger ModelFactory::logger = nullptr;

#define REGISTER_MODEL(name) ModelRegister<name> name::reg(#name);
#define REGISTER_DSN(name, value) ModelFactory::dsn(#name, #value);

template<typename T> 
void migrateT() { 
  T::Migrate(); 
}

struct ModelMapEntry{
  public:
    void (*migrate)();
};

struct ModelFactory {
  typedef std::map<std::string, ModelMapEntry> map_type;
  typedef std::map<std::string, std::shared_ptr<soci::connection_pool>> dsn_type;
  typedef std::function<void (std::string)> Logger;

  public:

    static void migrate(std::string const& s) {
      map_type::iterator it = models->find(s);
      if(it == models->end())
        throw std::runtime_error("Migration "+s+" not found");

      it->second.migrate();
    }

    static std::vector<std::string> getModelNames() {
      std::vector<std::string> ret; 
      transform(models->begin(), models->end(), back_inserter(ret), [](auto& c) { 
        return c.first; });
      return ret;
    }

    static std::vector<std::string> getDsns() {
      std::vector<std::string> ret; 
      transform(dsns->begin(), dsns->end(), back_inserter(ret), [](auto& c) { 
        return c.first; });
      return ret;
    }

    static soci::session getSession(std::string name) {
      if (dsns->count(name) == 0)
        throw std::runtime_error("Dsn "+name+" not found");

      return soci::session(*(*dsns)[name]); 
    }

    static void Dsn(std::string name, std::string value, unsigned int threads) {
      if (dsns->count(name) > 0)
        throw std::runtime_error("Dsn "+name+" already established");

      if (threads == 0 || name.empty())
        throw ModelException(
        "Unable to retrieve a database session. "
        "The database connection has not yet been initialized.");

      std::shared_ptr<soci::connection_pool> connection_pool = \
        std::make_shared<soci::connection_pool>(threads);

      for (unsigned int i = 0; i != threads; ++i) {
        soci::session & sql = connection_pool->at(i);
        sql.open(value);
        if (sql.get_backend_name() == "mysql") { 
          // Ensure that we automatically reconnect, if our connection times out
          auto mysqlbackend = static_cast<soci::mysql_session_backend *>(sql.get_backend());
          bool reconnect = 1;
          mysql_options(mysqlbackend->conn_, MYSQL_OPT_RECONNECT, &reconnect);
        }
      }

      dsns->insert(std::make_pair(name, connection_pool));
    }

    static void Log(const std::string &message) {
      if (ModelFactory::logger != nullptr) ModelFactory::logger(message);
    }
    static void setLogger(Logger l) { ModelFactory::logger = l;}

  private:
    static std::shared_ptr<map_type> models;
    static std::shared_ptr<dsn_type> dsns;
    static Logger logger;
};

template<typename T>
struct ModelRegister : ModelFactory { 
  ModelRegister(std::string const& s) { 
    ModelMapEntry me;
    me.migrate = &migrateT<T>;

    models->insert(std::make_pair(s, me));
  }
};

