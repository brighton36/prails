#pragma once
#include "spdlog/spdlog.h"
#include <pistache/http.h>

class RequestException : public std::exception {
  public:
    std::string s;
    explicit RequestException(const std::string &ss) : s(ss) {}
    template<typename... Args> 
    RequestException(const std::string &reason, Args... args) :
      s(fmt::format(reason, args...)) { }
    ~RequestException() throw () {}
    const char* public_what() const throw() { return "Internal Server Error"; }
    const char* what() const throw() { return s.c_str(); }
};

class ModelException : public std::exception {
  public:
    std::string s;
    explicit ModelException(const std::string &ss) : s(ss) {}
    template<typename... Args> 
    ModelException(const std::string &reason, Args... args) :
      s(fmt::format(reason, args...)) { }
    ~ModelException() throw () {}
    const char* what() const throw() { return s.c_str(); }
};

class PostBodyException : public std::exception {
  public:
    std::string s;
    explicit PostBodyException(const std::string &ss) : s(ss) {}
    template<typename... Args> 
    PostBodyException(const std::string &reason, Args... args) :
      s(fmt::format(reason, args...)) { }
    ~PostBodyException() throw () {}
    const char* what() const throw() { return s.c_str(); }
};

class AccessDenied : public std::exception {
  public:
    std::string log_what;
    std::string display_what;
    template<typename... Args> 
    AccessDenied(const std::string &_what, const std::string &_public_what, Args... args) : 
      log_what(fmt::format(_what, args...)), 
      display_what(fmt::format(_public_what, args...)) { }
    ~AccessDenied() throw () {}

    const char* public_what() const throw() { return display_what.c_str(); }
    const char* what() const throw() { return log_what.c_str(); }
};
