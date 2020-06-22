#include "prails.hpp"
#include "main.hpp"
#include "server.hpp"

using namespace std;
using namespace Pistache;

enum class RunMode { WebServer, Migration };

#include <iostream> // TODO: Remove
//void prails::main(int* argc, wchar_t** argv) {
int prails::main(int argc, char *argv[]) {
  std::cout << "Inside the main()" << std::endl;
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

      // TODO: There's got to be a better registry-way to do this:
      //Account::Migrate();
      //Task::Migrate();
      //Station::Migrate();
      //StationAlternative::Migrate();
      //StationAlias::Migrate();
      //Season::Migrate();
      //Agency::Migrate();
      } break; 
  }

  return 0;
}
