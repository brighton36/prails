#include "prails.hpp"
#include "server.hpp"
#include "model_factory.hpp"
#include "controller_factory.hpp"

using namespace std;
using namespace Pistache;

enum class RunMode { WebServer, Migration };

shared_ptr<ControllerFactory::map_type> ControllerFactory::map = nullptr;
shared_ptr<ModelFactory::map_type> ModelFactory::map = nullptr;

int prails::main(int argc, char *argv[]) {
  string config_path;
  RunMode run_mode = RunMode::WebServer;

  string program_name = string(argv[0]);
  vector<string> args(argv + 1, argv + argc);

  for(string a : args) {
    if (a.at(0) != '-') config_path = a;
    else if (a == "-M") run_mode = RunMode::Migration;
    else if (a == "-S") run_mode = RunMode::WebServer;
  }

  if ( (args.size() == 0 ) || has_any(args, {"-h", "--help"}) || (config_path.empty() ) ) {
    cout << fmt::format("Usage: {} [-S] [-M] CONFIG_FILE\n"
      "A web server built with prails.\n\n"
      "-S           Run in server mode.\n"
      "-M           Run model migrations.\n"
      "--help       Display this help and exit.\n\n"
      "The supplied CONFIG_FILE is expected to be a yaml-formatted server configuration file.\n"
      "(See https://en.wikipedia.org/wiki/YAML for details on the YAML file format.)\n\n",
      program_name);
    return 1;
  }

  spdlog::info("Using config={}", config_path);

  ConfigParser config(config_path);
  Model::Initialize(config);
  Controller::Initialize(config);

  switch (run_mode) {
    case RunMode::WebServer: {
      spdlog::info("{} log started. Cores={} Threads={}", program_name,
        hardware_concurrency(), config.threads());
      Server server(config);
      server.start();
      } break;
    case RunMode::Migration: {
      spdlog::info("{} migration.", program_name);

      for (const auto &reg : ModelFactory::getRegistrations()) {
        spdlog::info("Running migration for {}..", reg);
        ModelFactory::migrate(reg);
      }
      } break; 
  }

  return 0;
}
