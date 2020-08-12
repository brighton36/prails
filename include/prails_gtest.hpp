#include "gtest/gtest.h"

#include <pistache/http.h>
#include <pistache/stream.h>
#include <pistache/router.h>

#include "httplib.h"

#include "model.hpp"
#include "model_factory.hpp"
#include "controller_factory.hpp"

#include "server.hpp"

// NOTE: This initialization must occur after models are registered
#define INIT_PRAILS_TEST_ENVIRONMENT() \
  ConfigParser * PrailsControllerTest::config = nullptr; \
  PrailsEnvironment* const prails_env = \
  static_cast<PrailsEnvironment*>(::testing::AddGlobalTestEnvironment(new PrailsEnvironment));

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
  private:
    std::unique_ptr<Server> server;
    std::shared_ptr<ConfigParser> config;
  public:
    void SetUp() override {
      config = std::make_unique<ConfigParser>(std::string(TESTS_CONFIG_FILE));

      PrailsControllerTest::config = config.get();

      //spdlog::set_level(spdlog::level::debug);

      Model::Initialize(*config);

      // NOTE: We may want to support setting up specific models to migrate in
      // the constructor...
      for (const auto &reg : ModelFactory::getRegistrations())
        ModelFactory::migrate(reg);

      server = std::make_unique<Server>(*config);
      server->startThreaded();
    }

    void TearDown() override {
      server->shutdown();
    }

};
