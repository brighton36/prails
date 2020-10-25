#pragma once
#include <string>

#include "spdlog/spdlog.h"

class ConfigParser {
  public:
    explicit ConfigParser(std::string);
    ConfigParser();
    std::string path();
    unsigned int port();
    unsigned int threads();
    void threads(unsigned int);
    unsigned int spdlog_queue_size();
    void spdlog_queue_size(unsigned int);
    std::string address();
    std::string static_resource_path();
    std::string views_path();
    std::string config_path();
    std::string dsn();
    std::string cors_allow();
    std::string html_error(unsigned int);

    std::string log_directory();
    std::string log_level();
    void log_level(const std::string &);
    spdlog::level::level_enum spdlog_level();
		std::shared_ptr<spdlog::logger> setup_logger(
      const std::string &logger_name = "server");
    void flush_logs();

  private:
    unsigned int port_;
    unsigned int threads_;
    unsigned int spdlog_queue_size_;
    std::string path_;
    std::string address_;
    std::string static_resource_path_;
    std::string views_path_;
    std::string config_path_;
    std::string log_level_;
    std::string dsn_;
    std::string cors_allow_;
    std::string log_directory_;
    std::string base_path;
    std::string expand_path(std::string);
		std::vector<spdlog::sink_ptr> sinks;
};
