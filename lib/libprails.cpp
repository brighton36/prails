
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

PSYM_MODELS()
PSYM_CONTROLLERS()

constexpr std::string_view help_output {
  "Usage: {program_name} [-f CONFIG_FILE] COMMAND\n"
  "A web server built with prails.\n\n"
  "Global Switches:\n" 
  "  -f CONFIG_FILE Use the supplied file for configuration settings.\n"
  "  --help         Display this help and exit.\n\n"
  "The following commands can be executed:\n"
  "  server         Run in server mode.\n"
  "  migrate        Run model migrations.\n"
  "  output URL [FILE]  Output a URL to stdout (default) or [FILE].\n"
  "The supplied CONFIG_FILE is expected to be a yaml-formatted server configuration file.\n"
  "(See https://en.wikipedia.org/wiki/YAML for details on the YAML file format.)\n\n"
};

unsigned int mode_help(ConfigParser &, shared_ptr<spdlog::logger>, const vector<string> & args) {
  string program_name = filesystem::path(args[0]).filename();
  cout << fmt::format(help_output, fmt::arg("program_name", program_name));
  return 1;
}

unsigned int mode_server(ConfigParser &config, shared_ptr<spdlog::logger> logger, const vector<string> & args) {
  string program_name = filesystem::path(args[0]).filename();
  logger->info("{} log started. Cores={} Threads={}", program_name,
    hardware_concurrency(), config.threads());
  Server server(config);
  server.start();
  return 0;
}

unsigned int mode_migrate(ConfigParser &, shared_ptr<spdlog::logger> logger, const vector<string> & args) {
  string program_name = filesystem::path(args[0]).filename();
  logger->info("{} migration.", program_name);

  for (const auto &reg : ModelFactory::getModelNames()) {
    logger->info("Running migration for {}..", reg);
    ModelFactory::migrate(reg, 1);
  }

  return 0;
}

unsigned int mode_output(ConfigParser &config, shared_ptr<spdlog::logger> logger, const vector<string> & args) {
  string url; 
  string output; 
  
  if (args.size() > 1) {
    url = args[1];
    if (args.size() > 2) output = args[2];
  } else
    throw invalid_argument("Missing output parameter(s)");

  logger->info("Outputting {} to {}.", url, (output.empty()) ? "STDOUT" : output);

  Server server(config);
  try {
    server.startThreaded();
  } catch (const std::runtime_error &e) {
    // We'll assume the server is already running, in this case, and just query
    // the existing process.
    if (string(e.what()) != "Address already in use")
      throw e;
  }

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

  return 0;
}

int prails::main(int argc, char *argv[], map<string, ModeFunction> modes, 
  AppInitFunction appinit) {

  string config_path;
  string run_mode;

  vector<string> args(argv, argv + argc);

  if (!modes.count("server")) modes["server"] = mode_server;
  if (!modes.count("help")) modes["help"] = mode_help;
  if (!modes.count("migrate")) modes["migrate"] = mode_migrate;
  if (!modes.count("output")) modes["output"] = mode_output;

  // NOTE: We remove the entries in the args list as we recognize them. We
  //       save anything we don't recognize, for use below.
  for (auto it = args.begin()+1; it != args.end(); it++) {
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
      run_mode = "help";
      break;
    }
    else if (modes.count(arg) && run_mode.empty()) {
      run_mode = arg;
      break;
    }
    else {
      throw invalid_argument(fmt::format("Unrecognized argument \"{}\"", arg));
      break;
    }
  }

  ConfigParser config;
  shared_ptr<spdlog::logger> logger;

  if (run_mode.empty() || config_path.empty()) 
    run_mode = "help";
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

    if (appinit) appinit(config, logger);
  }

  return modes[run_mode](config, logger, args);
}

