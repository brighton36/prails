#include <unistd.h>

#include <iostream> // TODO
#include "prails_gtest.hpp"
#include "gmock/gmock.h"
#include "utilities.hpp"

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
  public:
    void SetUp() override {
      config = make_unique<ConfigParser>(string(TESTS_CONFIG_FILE));
      // We override these here,to make this test meaningful:
      config->threads(8);
      config->log_level("debug");

      // If we don't increase spdlog's queue size to something ridiculous, then
      // these tests block as soon as the default queue size is reached, and we
      // don't test concurrency
      config->spdlog_queue_size(1000000);

      InitializeLogger();
      InitializeServer();
      // tODO: Delete the log file on tear down
    }
};

INIT_CONTROLLER_REGISTRY()
REGISTER_CONTROLLER(LoggerConcurrencyController)

INIT_PRAILS_TEST_ENVIRONMENT_WITH(LoggerConcurrencyEnvironment)

TEST(LoggerConcurrency, ab_log_a_visit) {
  using ::testing::MatchesRegex;

  const string abPath = "/usr/bin/ab";
  const unsigned int abThreads = 8;
  const unsigned int abRequests = 500000; // TODO: we can probably lower this

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

  // TODO: Flush the log
  // my_logger->flush();
  //
	// TODO: Ensure the file exists...
  // TODO: Count the number of lines
  ifstream logfile("log/logfile_2020-10-23");
	string line;
	while (getline(logfile, line)) {
    string line_matches = "^\\[[ -:\\.\\d]+] \\[server] \\[(debug|info)] "
      "(Requested /log-a-visit|Routing: GET /log-a-visit to "
      "LoggerConcurrencyController#log_a_visit \\(127\\.0\\.0\\.1\\) )";
    if (!regex_match(line, regex(line_matches, regex::extended)))
      cout << line << endl;
    EXPECT_THAT(line, MatchesRegex(line_matches));
	}
  
}
