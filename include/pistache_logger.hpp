#pragma once

#include <memory>
#include "pistache/string_logger.h"

class StringToSpdLogger : public Pistache::Log::StringLogger {
  private:
    std::shared_ptr<spdlog::logger> logger_;
  public:
    explicit StringToSpdLogger(std::shared_ptr<spdlog::logger> logger) 
      : logger_(logger) {}
    ~StringToSpdLogger() override {}

    void log(Pistache::Log::Level level, const std::string &message) override {
      switch(level) {
        case Pistache::Log::Level::TRACE :
          logger_->trace("Pistache: {}", message);
          break;
        case Pistache::Log::Level::DEBUG :
          logger_->debug("Pistache: {}", message);
          break;
        case Pistache::Log::Level::INFO :
          logger_->info("Pistache: {}", message);
          break;
        case Pistache::Log::Level::WARN :
          logger_->warn("Pistache: {}", message);
          break;
        case Pistache::Log::Level::ERROR :
          logger_->error("Pistache: {}", message);
          break;
        case Pistache::Log::Level::FATAL :
          logger_->critical("Pistache: {}", message);
          break;
      }
    }

  bool isEnabledFor(Pistache::Log::Level) const override { return true; }
};
