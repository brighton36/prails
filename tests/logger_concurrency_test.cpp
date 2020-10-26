#include <unistd.h>
#include <filesystem>

#include "gmock/gmock.h"

#include "prails_gtest.hpp"
#include "utilities.hpp"

#include <iostream>

using namespace std;

class LoggerConcurrencyController : public Controller::Instance { 
  public:
    using Instance::Instance;

    static void Routes(Pistache::Rest::Router& r, 
      shared_ptr<Controller::Instance> controller) {
      using namespace Pistache::Rest::Routes;
      Get(r, "/log-a-visit", bind("log_a_visit", 
        &LoggerConcurrencyController::log_a_visit, controller));
    }

    Controller::Response log_a_visit(const Pistache::Rest::Request&) {
      logger->info("Requested /log-a-visit");
      return Controller::Response(200, "text/html", "success");
    }

  private:
    static ControllerRegister<LoggerConcurrencyController> reg;
};

class LoggerConcurrencyEnvironment : public PrailsEnvironment {
  protected:
    tm server_started;

  public:
    void SetUp() override {
      config = make_unique<ConfigParser>(string(TESTS_CONFIG_FILE));
      // We override these here,to make this test meaningful:
      config->threads(8);
      config->log_level("debug");
      config->log_directory(fmt::format("{}/log", PROJECT_BINARY_DIR));

      // If we don't increase spdlog's queue size to something ridiculous, then
      // these tests block as soon as the default queue size is reached, and we
      // don't test concurrency
      config->spdlog_queue_size(1000000);

      // I guess this test might act weird if you run it at 23:59 ...
      server_started = Model::NowUTC();

      InitializeLogger();
      InitializeServer();
    }

    void flush_logs() {
      config->flush_logs();
    }

    string server_logfile_path() {
      return fmt::format( "{}/log/{}_{:04d}-{:02d}-{:02d}.log", 
        PROJECT_BINARY_DIR, "server", server_started.tm_year + 1900, 
        server_started.tm_mon + 1, 
        server_started.tm_mday);
    }

    void TearDown() override {
      server->shutdown();

      string logfile = server_logfile_path();
      if (remove(logfile.c_str()) != 0)
        throw runtime_error("Unable to remove logfile at end of test: "+logfile);
    }
};

REGISTER_CONTROLLER(LoggerConcurrencyController)

INIT_PRAILS_TEST_ENVIRONMENT_WITH(LoggerConcurrencyEnvironment)

TEST(LoggerConcurrency, ab_log_a_visit) {
  // Note that there's roughly two phases to this test. The first phase 
  // generates the log file, and should saturate all cpus. The second phase
  // validates the log file, and should saturate a single cpu. If you're not 
  // seeing the cpu process time consumed during phase one, the spd_queue_log
  // size is probably not high enough.
  using ::testing::MatchesRegex;

  const string abPath = "/usr/bin/ab";
  const unsigned int abThreads = 8;
  const unsigned int abRequests = 50000;
  const string logline_matches = "^\\[[ -:\\.\\d]+] \\[thread [0-9]+] \\[(debug|info)] "
    "(Requested /log-a-visit|Routing: GET /log-a-visit to "
    "LoggerConcurrencyController#log_a_visit \\(127\\.0\\.0\\.1\\) )$";

  // Make sure ab exists and is executable:
  ASSERT_FALSE(access(abPath.c_str(), X_OK));

  unsigned int return_code;
  string output;
  tie(return_code, output) = prails::utilities::capture_system(fmt::format(
    "{} -d -q -S -n {} -k -c {} http://localhost:{}/log-a-visit",
    abPath, abRequests, abThreads, 8081));

  smatch parts;
  string complete_requests;
  string concurrency_level; 
  string failed_requests;

  if (regex_search(output, parts, regex("\nComplete requests:[ ]+(.+)")))
    complete_requests = string(parts[1]);
  if (regex_search(output, parts, regex("\nConcurrency Level:[ ]+(.+)")))
    concurrency_level = string(parts[1]);
  if (regex_search(output, parts, regex("\nFailed requests:[ ]+(.+)")))
    failed_requests = string(parts[1]);

	EXPECT_EQ(0, return_code);
	EXPECT_EQ(complete_requests, to_string(abRequests));
	EXPECT_EQ(concurrency_level, to_string(abThreads));
	EXPECT_EQ(failed_requests, "0");

  prails_env->flush_logs();

  string log_file_path = prails_env->server_logfile_path();
  cout << "TODO: " << log_file_path << endl;

  EXPECT_TRUE(filesystem::is_regular_file(log_file_path));

	string logline;
  unsigned int log_line_count = 0;
  ifstream logfile(log_file_path);

	while (getline(logfile, logline)) {
    log_line_count += 1;
    EXPECT_THAT(logline, MatchesRegex(logline_matches));
	}

  // Two log lines per request:
	EXPECT_EQ(abRequests*2, log_line_count);
}
