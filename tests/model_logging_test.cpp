#include "prails_gtest.hpp"
#include "gtest/gtest.h"

#include "tester_models.hpp"

using namespace std;

INIT_MODEL_REGISTRY()
INIT_CONTROLLER_REGISTRY()

REGISTER_MODEL(TesterModel)

INIT_PRAILS_TEST_ENVIRONMENT()

TEST_F(PrailsControllerTest, log_to_stdout) {
  std::string model_logger_output;

  ModelFactory::setLogger( [&model_logger_output](auto message) { 
    model_logger_output.append(message);
  });

  TesterModel model({
    {"first_name", "John"},
    {"last_name", "Smith"},
    {"email", "jsmith@google.com"}
    });

  EXPECT_NO_THROW(model.save());
  
  ASSERT_TRUE(true);
  ASSERT_EQ(model_logger_output, 
    "insert into tester_models (email, first_name, last_name)"
    " values(:email, :first_name, :last_name)"
    );
}
