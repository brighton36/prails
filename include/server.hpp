#pragma once
#include "controller.hpp"
#include "config_parser.hpp"

class Server {
  public:
    explicit Server(ConfigParser &);
    void start();
    void startThreaded();
    void shutdown();
  private:
    size_t threads;
    std::shared_ptr<spdlog::logger> logger;
    std::string path_static;
    std::string path_views;
    std::shared_ptr<Pistache::Http::Endpoint> http_endpoint;
    std::map<std::string, std::string> extension_to_mime;
    Pistache::Rest::Router router;

    std::map<std::string, std::shared_ptr<Controller::Instance>> controllers;

    void setupRoutes();
    void doNotFound(const Pistache::Rest::Request&, Pistache::Http::ResponseWriter);
};

