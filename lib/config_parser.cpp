#include "config_parser.hpp"
#include "utilities.hpp"

#include "yaml-cpp/yaml.h"
#include "spdlog/async.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

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

  // TODO: Register a default server logger
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
    if (yaml["address"]) address_ = yaml["address"].as<string>();
    if (yaml["static_resource_path"]) 
      static_resource_path_ = yaml["static_resource_path"].as<string>();
    if (yaml["views_path"]) views_path_ = yaml["views_path"].as<string>();
    if (yaml["config_path"]) config_path_ = yaml["config_path"].as<string>();
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

shared_ptr<spdlog::logger> ConfigParser::setup_logger() {
	string logger_name = "server";
	auto logger = spdlog::get(logger_name);
	if (not logger) {
		// TODO: Grab this from the config
		std::vector<spdlog::sink_ptr> sinks;
		sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		sinks.push_back(std::make_shared<spdlog::sinks::daily_file_sink_mt>("logfile", 23, 59));

		if (sinks.size() > 0) {
			logger = make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
			spdlog::register_logger(logger);
		} else
			logger = spdlog::stdout_color_mt(logger_name);
	}

	logger->set_level(spdlog_level());

	return logger;
}


string ConfigParser::path() { return path_; }
unsigned int ConfigParser::port() { return port_; }
unsigned int ConfigParser::threads() { return threads_; }
string ConfigParser::address() { return address_; }
string ConfigParser::static_resource_path() { return expand_path(static_resource_path_); }
string ConfigParser::views_path() { return expand_path(views_path_); }
string ConfigParser::config_path() { return expand_path(config_path_); }
string ConfigParser::log_level() { return log_level_; }
string ConfigParser::dsn() { return dsn_; }
string ConfigParser::cors_allow() { return cors_allow_; }

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

