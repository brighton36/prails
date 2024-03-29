#include "gtest/gtest.h"

#include <pistache/http.h>
#include <pistache/stream.h>
#include <pistache/router.h>

#include "httplib.h"

#include "model.hpp"
#include "controller_factory.hpp"

#include "server.hpp"

// NOTE: This initialization must occur after models are registered
#define PSYM_TEST_ENVIRONMENT() PSYM_TEST_ENVIRONMENT_WITH(PrailsEnvironment)

#define PSYM_TEST_ENVIRONMENT_WITH(GTEST_ENV) \
  PSYM_MODELS() \
  PSYM_CONTROLLERS() \
  ConfigParser * PrailsControllerTest::config = nullptr; \
  GTEST_ENV* const prails_env = \
  static_cast<GTEST_ENV*>(::testing::AddGlobalTestEnvironment(new GTEST_ENV));

class PrailsControllerTest : public ::testing::Test {
  public:
    httplib::Client browser() {
      auto addr = Pistache::Address(PrailsControllerTest::config->address(), 
        PrailsControllerTest::config->port());
      return httplib::Client(addr.host().c_str(), (uint16_t) addr.port());
    }
    
    static ConfigParser *config;
};

class PrailsEnvironment : public ::testing::Environment {
  protected:
    std::unique_ptr<Server> server;
    std::shared_ptr<ConfigParser> config;
    std::shared_ptr<spdlog::logger> logger;

    void InitializeLogger() {
      // NOTE: Depending on how we're linked, it seems that we either share 
      // the memory space with the library's registration, or, we have a local
      // registration. As such, we register both in the setup_logger, as well
      // as here
      try { 
        logger = config->setup_logger();
        spdlog::register_logger(logger); 
      }
      catch (spdlog::spdlog_ex &e) { /* Do nothing if we already exist */ }
    }

    void InitializeDatabase(std::string dsn, unsigned int threads) {
      ModelFactory::Dsn("default", dsn, threads);

      // NOTE: We may want to support setting up specific models to migrate in
      // the constructor...
      for (const auto &reg : ModelFactory::getModelNames())
        ModelFactory::migrate(reg, 1);
    }

    void InitializeServer() {
      server = std::make_unique<Server>(*config);
      server->startThreaded();
    }

    void DestroyServer() {
      server->shutdown();
    }

    void DestroyDatabase() {
      for (const auto &reg : ModelFactory::getModelNames())
        ModelFactory::migrate(reg, 0);
    }

  public:
    void SetUp() override {
      config = std::make_unique<ConfigParser>(std::string(TESTS_CONFIG_FILE));
      PrailsControllerTest::config = config.get();

      InitializeLogger();
      InitializeDatabase(config->dsn(), config->threads());
      InitializeServer();
    }

    void TearDown() override {
      DestroyServer();
      DestroyDatabase();
    }
};
