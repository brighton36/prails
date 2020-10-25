#include "config_parser.hpp"
#include "utilities.hpp"

#include "yaml-cpp/yaml.h"
#include "spdlog/async.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/null_sink.h>

#include <filesystem>
#include <regex>

using namespace std;
using namespace prails::utilities;

ConfigParser::ConfigParser() {
  port_ = 8080;
  threads_ = 2;
  address_ = "0.0.0.0";
  base_path = ".";
  static_resource_path_ = "public";
  views_path_ = "views";
  config_path_ = "config";
  log_level_ = "info";

  spdlog_queue_size(8192);
}

ConfigParser::ConfigParser(string config_file_path) {
  ConfigParser();

  path_ = config_file_path;

  if (!config_file_path.empty()) {
    if (!path_is_readable(config_file_path))
      throw invalid_argument("Unable to read the supplied configuration file.");

    base_path = filesystem::path(filesystem::canonical(config_file_path)).parent_path();

    auto yaml = YAML::LoadFile(config_file_path);
    if (yaml["port"]) port_ = yaml["port"].as<unsigned int>();
    if (yaml["threads"]) threads_ = yaml["threads"].as<unsigned int>();
    if (yaml["spdlog_queue_size"]) 
      spdlog_queue_size(yaml["spdlog_queue_size"].as<unsigned int>());
    if (yaml["address"]) address_ = yaml["address"].as<string>();
    if (yaml["static_resource_path"]) 
      static_resource_path_ = yaml["static_resource_path"].as<string>();
    if (yaml["views_path"]) views_path_ = yaml["views_path"].as<string>();
    if (yaml["config_path"]) config_path_ = yaml["config_path"].as<string>();
    if (yaml["log_directory"]) 
      log_directory_ = yaml["log_directory"].as<string>();
    if (yaml["log_level"]) log_level_ = yaml["log_level"].as<string>();
    if (yaml["dsn"]) dsn_ = yaml["dsn"].as<string>();
    if (yaml["cors_allow"]) cors_allow_ = yaml["cors_allow"].as<string>();
  }

  if(!regex_match(log_level(), regex("^(?:critical|err|warn|info|debug|trace|off)$")))
    throw invalid_argument("Invalid Log Level specified in config");

  if (static_resource_path_.empty())
    throw invalid_argument("Unreadable or missing static_resource_path.");

  if (views_path_.empty())
    throw invalid_argument("Unreadable or missing views_path.");

  if (!path_is_readable(config_path()))
    throw invalid_argument("Unreadable or missing config_path.");
}

void ConfigParser::flush_logs() {
  // TODO: Iterate the loggers? sinks?
	auto logger = spdlog::get("server");
	if (logger) logger->flush();
}

shared_ptr<spdlog::logger> ConfigParser::setup_logger(const string &logger_name) {
	auto logger = spdlog::get(logger_name);
	if (not logger) {
    if (sinks.size() == 0) {
      if (log_directory().empty())
        sinks.push_back(make_shared<spdlog::sinks::null_sink_mt>());
      else
        sinks.push_back(make_shared<spdlog::sinks::daily_file_sink_mt>(
          join({log_directory(), "server.log"}, "/"), 23, 59));

      // TODO: let's see where we're creating these logger_name's
      // TODO: some_logger->set_pattern(">>>>>>>>> %H:%M:%S %z %v <<<<<<<<<");
      //sinks.push_back(make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    logger = make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
    spdlog::register_logger(logger);
	}

	logger->set_level(spdlog_level());

	return logger;
}


string ConfigParser::path() { return path_; }
unsigned int ConfigParser::port() { return port_; }
unsigned int ConfigParser::threads() { return threads_; }
unsigned int ConfigParser::spdlog_queue_size() { return spdlog_queue_size_; }
string ConfigParser::address() { return address_; }
string ConfigParser::static_resource_path() { return expand_path(static_resource_path_); }
string ConfigParser::views_path() { return expand_path(views_path_); }
string ConfigParser::config_path() { return expand_path(config_path_); }
string ConfigParser::log_directory() { 
  return (log_directory_.empty()) ? string() : expand_path(log_directory_); 
}
string ConfigParser::log_level() { return log_level_; }
string ConfigParser::dsn() { return dsn_; }
string ConfigParser::cors_allow() { return cors_allow_; }

void ConfigParser::threads(unsigned int t) { threads_ = t; }
void ConfigParser::spdlog_queue_size(unsigned int q) { 
  spdlog_queue_size_ = q;
  spdlog::init_thread_pool(spdlog_queue_size_, 1);
}

void ConfigParser::log_level(const string &s) { log_level_ = s; }

spdlog::level::level_enum ConfigParser::spdlog_level() { 
  using namespace spdlog;

  if ("critical" == log_level()) return level::critical;
  else if ("err" == log_level()) return level::err;
  else if ("warn" == log_level()) return level::warn;
  else if ("info" == log_level()) return level::info;
  else if ("debug" == log_level()) return level::debug;
  else if ("trace" == log_level()) return level::trace;
  return level::off;
}

string ConfigParser::expand_path(string p) {
  return filesystem::weakly_canonical(remove_trailing_slash(
    (filesystem::path(p).is_relative()) ? string(base_path)+"/"+p : p
  ));
}

