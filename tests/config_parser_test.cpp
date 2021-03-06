#include "config_parser.hpp"
#include <filesystem>

#include "gtest/gtest.h"

using namespace std;

TEST(config_parser, test_server_config) {
  string config_path = std::string(TESTS_CONFIG_FILE);
  ConfigParser config(config_path);

  EXPECT_EQ(config.port(), 8081);
  EXPECT_EQ(config.path(), config_path);
  EXPECT_EQ(config.threads(), 1);
  EXPECT_EQ(config.address(), "127.0.0.1");
  EXPECT_EQ(config.static_resource_path(), 
    string(PROJECT_SOURCE_DIR)+"/tests/public");
  EXPECT_EQ(config.views_path(), 
    string(PROJECT_SOURCE_DIR)+"/tests/views");
  EXPECT_EQ(config.config_path(), 
    string(PROJECT_SOURCE_DIR)+"/tests/config");
  EXPECT_EQ(config.log_directory(), 
    string(PROJECT_SOURCE_DIR)+"/build/log");
  EXPECT_EQ(config.log_level(), "off");
  EXPECT_EQ(config.spdlog_level(), spdlog::level::off);
  EXPECT_EQ(config.dsn(), "sqlite3://:memory:");
}
