#pragma once
#include "controller.hpp"
#include "config_parser.hpp"

class Server {
  public:
    explicit Server(ConfigParser &);
    void start();
    void startThreaded();
    void shutdown();
    static std::optional<std::string> ExtToMime(const std::string &);
  private:
    size_t threads;
    size_t max_request_size;
    std::shared_ptr<spdlog::logger> logger;
    std::string html_error500;
    std::string html_error404;
    std::string path_static;
    std::string path_views;
    std::shared_ptr<Pistache::Http::Endpoint> http_endpoint;
    Pistache::Rest::Router router;

    std::map<std::string, std::shared_ptr<Controller::Instance>> controllers;

    void setupRoutes();
    void doNotFound(const Pistache::Rest::Request&, Pistache::Http::ResponseWriter);
};

