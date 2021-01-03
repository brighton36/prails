#pragma once
#include "spdlog/spdlog.h"

// TODO: Can we just inherit each of these from a common base...
class RequestException : public std::exception {
  public:
    std::string s;
    explicit RequestException(const std::string &ss) : s(ss) {}
    template<typename... Args> RequestException(const std::string &reason, Args... args) :
      s(fmt::format(reason, args...)) { 
        auto logger = spdlog::get("server");
        if (logger == nullptr)
          throw std::runtime_error("Unable to acquire exception logger");
        logger->error(s.c_str()); 
      }
    ~RequestException() throw () {}
    const char* what() const throw() { return s.c_str(); }
};

class ModelException : public std::exception {
  public:
    std::string s;
    explicit ModelException(const std::string &ss) : s(ss) {}
    template<typename... Args> ModelException(const std::string &reason, Args... args) :
      s(fmt::format(reason, args...)) { }
    ~ModelException() throw () {}
    const char* what() const throw() { return s.c_str(); }
};

class PostBodyException : public std::exception {
  public:
    std::string s;
    explicit PostBodyException(const std::string &ss) : s(ss) {}
    template<typename... Args> PostBodyException(const std::string &reason, Args... args) :
      s(fmt::format(reason, args...)) { }
    ~PostBodyException() throw () {}
    const char* what() const throw() { return s.c_str(); }
};
