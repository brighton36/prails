#include "config_parser.hpp"
#include "utilities.hpp"

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
  max_request_size_ = 4096; // pistache's DefaultMaxRequestSize
  address_ = "0.0.0.0";
  base_path = ".";
  static_resource_path_ = "public";
  views_path_ = "views";
  config_path_ = "config";
  log_level_ = "info";

  spdlog_queue_size(8192);
}

ConfigParser::ConfigParser(string config_file_path) : ConfigParser() {
  path_ = config_file_path;

  if (!config_file_path.empty()) {
    if (!path_is_readable(config_file_path))
      throw invalid_argument("Unable to read the supplied configuration file.");

    base_path = filesystem::path(filesystem::canonical(config_file_path)).parent_path();

    yaml = YAML::LoadFile(config_file_path);
    if (has_value("port")) port_ = get<unsigned int>("port");
    if (has_value("threads")) threads_ = get<unsigned int>("threads");
    if (has_value("max_request_size"))
      max_request_size_ = get<unsigned int>("max_request_size");
    if (has_value("spdlog_queue_size")) 
      spdlog_queue_size(get<unsigned int>("spdlog_queue_size"));
    if (has_value("address")) address_ = get<string>("address");
    if (has_value("static_resource_path")) 
      static_resource_path_ = get<string>("static_resource_path");
    if (has_value("views_path")) views_path_ = get<string>("views_path");
    if (has_value("config_path")) config_path_ = get<string>("config_path");
    if (has_value("log_directory")) log_directory_ = get<string>("log_directory");
    if (has_value("log_level")) log_level_ = get<string>("log_level");
    if (has_value("dsn")) dsn_ = get<string>("dsn");
    if (has_value("cors_allow")) cors_allow_ = get<string>("cors_allow");
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
  for_each(sinks.begin(), sinks.end(), [](const auto &sink) { sink->flush(); });
}

shared_ptr<spdlog::logger> ConfigParser::setup_logger(const string &logger_name) {
	auto logger = spdlog::get(logger_name);
	if (not logger) {
    if (sinks.size() == 0) {
      if (!log_directory().empty()) {
        auto file_sink = make_shared<spdlog::sinks::daily_file_sink_mt>(
          join({log_directory(), "server.log"}, "/"), 23, 59);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] %v");
        sinks.push_back(file_sink);
      }

      if (is_logging_to_console())
        sinks.push_back(make_shared<spdlog::sinks::stdout_color_sink_mt>());

      // We want to ensure there's at least something in here:
      if (sinks.size() == 0)
        sinks.push_back(make_shared<spdlog::sinks::null_sink_mt>());
    }
    logger = make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
	}

	logger->set_level(spdlog_level());

	return logger;
}


string ConfigParser::path() { return path_; }
unsigned int ConfigParser::port() { return port_; }
unsigned int ConfigParser::threads() { return threads_; }
unsigned int ConfigParser::max_request_size() { 
  return max_request_size_;
}
unsigned int ConfigParser::spdlog_queue_size() { return spdlog_queue_size_; }
string ConfigParser::address() { return address_; }
string ConfigParser::static_resource_path() { return expand_path(static_resource_path_); }
string ConfigParser::views_path() { return expand_path(views_path_); }
string ConfigParser::config_path() { return expand_path(config_path_); }
string ConfigParser::log_directory() { 
  return (log_directory_.empty()) ? string() : expand_path(log_directory_); 
}
bool ConfigParser::is_logging_to_console() { return is_logging_to_console_; }
string ConfigParser::log_level() { return log_level_; }
string ConfigParser::dsn() { return dsn_; }
string ConfigParser::cors_allow() { return cors_allow_; }

void ConfigParser::log_directory(const string &d) { log_directory_ = d; }
void ConfigParser::threads(unsigned int t) { threads_ = t; }
void ConfigParser::spdlog_queue_size(unsigned int q) { 
  spdlog_queue_size_ = q;
  spdlog::init_thread_pool(spdlog_queue_size_, 1);
}

void ConfigParser::log_level(const string &s) { log_level_ = s; }
void ConfigParser::is_logging_to_console(bool i) { is_logging_to_console_ = i; }

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

string ConfigParser::html_error(unsigned int error) {
  // NOTE: We'll want to support putting alternatives in the config file at some point
  const string html_error = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
    "<html><head><title>{}</title></head><body><h1>{}</h1><p>{}</p><hr>"
    "<address>{}</address></body></html>";

  string addr = fmt::format("Server at {} Port {}", address(), port());

  switch (error) {
    case 400:
      return fmt::format(html_error, "400 Bad Request", "Bad Request", 
        "Your browser sent a request that this server could not understand.", 
        addr);
    case 404:
      return fmt::format(html_error, "404 Not Found", "Not Found", 
        "The requested URL was not found on this server.", addr);
    case 500:
      return fmt::format(html_error, "500 Internal Server Error", "Internal Server Error", 
        "There was an error processing this request.", addr);
  }

  return fmt::format(html_error, fmt::format("{} Unhandled Error", error), 
    "Unhandled Error", fmt::format("The request returned an error with code {}", 
      error), addr);
}
