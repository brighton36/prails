#include "prails.hpp"
#include "main.hpp"
#include "server.hpp"

using namespace std;
using namespace Pistache;

enum class RunMode { WebServer, Migration };

shared_ptr<ControllerFactory::map_type> ControllerFactory::map = nullptr;
shared_ptr<ModelFactory::map_type> ModelFactory::map = nullptr;

int prails::main(int argc, char *argv[]) {
  string config_path;
  RunMode run_mode = RunMode::WebServer;

  vector<string> args(argv + 1, argv + argc);

  for(string a : args) {
    if (a.at(0) != '-') config_path = a;
    else if (a == "-M") run_mode = RunMode::Migration;
    else if (a == "-S") run_mode = RunMode::WebServer;
  }

  if ( (args.size() == 0 ) || has_any(args, {"-h", "--help"}) || (config_path.empty() ) ) {
    cout << "TODO: Help" << endl;
    return 1;
  }

  spdlog::info("Using config={}", config_path);

  ConfigParser config(config_path);

  Model::Initialize(config);
  Controller::Initialize(config);

  switch (run_mode) {
    case RunMode::WebServer: {
      spdlog::info("prails log started. Cores={} Threads={}", 
        hardware_concurrency(), config.threads());
      Server server(config);
      server.start();
      } break;
    case RunMode::Migration: {
      spdlog::info("prails migration.");

      for (const auto &reg : ModelFactory::getRegistrations()) {
        spdlog::info("Running migration for {}..", reg);
        ModelFactory::migrate(reg);
      }
      } break; 
  }

  return 0;
}
