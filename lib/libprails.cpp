#include <iostream>
#include <filesystem>

#include "httplib.h"

#include "prails.hpp"
#include "server.hpp"
#include "model.hpp"
#include "controller_factory.hpp"

using namespace std;
using namespace Pistache;
using namespace prails::utilities;

enum class RunMode { Help, WebServer, Migration, OutputUrl };

PSYM_MODELS()
PSYM_CONTROLLERS()

int prails::main(int argc, char *argv[]) {
  string config_path;
  RunMode run_mode = RunMode::Help;

  string program_name = filesystem::path(string(argv[0])).filename();
  vector<string> args(argv + 1, argv + argc);

  // NOTE: We remove the entries in the args list as we recognize them. We
  //       save anything we don't recognize, for use below.
  for (auto it = args.begin(); it != args.end(); it++) {
    string arg = *it;
    args.erase(it--);

    if (arg == "-f") {
      // Advance the pointer, and store the next string as our config path.
      it++;
      if (it == args.end()) 
        throw invalid_argument("-f specified, but missing param");

      config_path = *it;
      args.erase(it--);
    }
    else if ( (arg == "--help") || (arg == "-h") ) {
      run_mode = RunMode::Help;
      break;
    }
    else if (arg == "migrate") {
      run_mode = RunMode::Migration;
      break;
    }
    else if (arg == "server") {
      run_mode = RunMode::WebServer;
      break;
    }
    else if (arg == "output") {
      run_mode = RunMode::OutputUrl;
      break;
    }
    else {
      throw invalid_argument(fmt::format("Unrecognized argument \"{}\"", arg));
      break;
    }
  }

  ConfigParser config;
  shared_ptr<spdlog::logger> logger;

  if (config_path.empty()) 
    run_mode = RunMode::Help;
  else {
    config = ConfigParser(config_path);
    config.is_logging_to_console(true);

    logger = config.setup_logger();
    if (logger == nullptr) throw runtime_error("Unable to create logger");

    spdlog::register_logger(logger);
    ModelFactory::setLogger([&logger](auto message) { 
      logger->debug("DB Query: {}", std::string(message));
    });

    logger->info("Using config={}", config_path);

    ModelFactory::Dsn("default", config.dsn(), config.threads());
    Controller::Initialize(config);
  }

  switch (run_mode) {
    case RunMode::Help: {
      cout << fmt::format("Usage: {} [-f CONFIG_FILE] COMMAND\n"
        "A web server built with prails.\n\n"
        "Global Switches:\n" 
        "  -f CONFIG_FILE Use the supplied file for configuration settings.\n"
        "  --help         Display this help and exit.\n\n"
        "The following commands can be executed:\n"
        "  server         Run in server mode.\n"
        "  migrate        Run model migrations.\n"
        "  output URL [FILE]  Output a URL to stdout (default) or [FILE].\n"
        "The supplied CONFIG_FILE is expected to be a yaml-formatted server configuration file.\n"
        "(See https://en.wikipedia.org/wiki/YAML for details on the YAML file format.)\n\n",
        program_name);
      return 1;
    }
    case RunMode::WebServer: {
      logger->info("{} log started. Cores={} Threads={}", program_name,
        hardware_concurrency(), config.threads());
      Server server(config);
      server.start();
      } break;
    case RunMode::OutputUrl: {
      string url; 
      string output; 
      
      if (args.size() > 0) {
        url = args[0];
        if (args.size() > 1) output = args[1];
      } else
        throw invalid_argument("Missing output parameter(s)");

      logger->info("Outputting {} to {}.", url, 
        (output.empty()) ? "STDOUT" : output);

      Server server(config);
      server.startThreaded();

      auto addr = Pistache::Address(config.address(), config.port());
      auto browser = httplib::Client(addr.host().c_str(), (uint16_t) addr.port());
      auto res = browser.Get(url.c_str());

      if (res->status != 200)
        throw logic_error(fmt::format("Error when requesting resource: {}", 
          res->status));

      if (output.empty())
        cout << res->body << endl;
      else {
        std::ofstream out(output);
        out << res->body;
        out.close();
      }

      server.shutdown();
    } break;
    case RunMode::Migration: {
      logger->info("{} migration.", program_name);

      for (const auto &reg : ModelFactory::getModelNames()) {
        logger->info("Running migration for {}..", reg);
        ModelFactory::migrate(reg);
      }
    } break; 
  }

  return 0;
}
