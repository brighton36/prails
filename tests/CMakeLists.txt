function(declare_test test_name)
  set(TEST_EXECUTABLE run_${test_name})
  set(TEST_SOURCE ${test_name}.cpp)

  add_executable(${TEST_EXECUTABLE} ${TEST_SOURCE})
  target_link_libraries(${TEST_EXECUTABLE} gtest gtest_main prails )
  add_test(${test_name} ${TEST_EXECUTABLE})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 300) 
endfunction()

add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_definitions(-DTESTS_CONFIG_FILE="${PROJECT_SOURCE_DIR}/tests/config/test-server.yml")

declare_test(utilities_test)
declare_test(config_parser_test)
declare_test(model_test)
target_link_libraries(run_model_test server)

# TODO: Dry this up ...
declare_test(post_body_test)
target_link_libraries(run_post_body_test
  "-Wl,--whole-archive" controller "-Wl,--no-whole-archive")

declare_test(rest_controller_test)
target_link_libraries(run_rest_controller_test server)
target_link_libraries(run_rest_controller_test
  "-Wl,--whole-archive" controller "-Wl,--no-whole-archive")

declare_test(logger_concurrency_test)
target_link_libraries(run_logger_concurrency_test server)
target_link_libraries(run_logger_concurrency_test
  "-Wl,--whole-archive" controller "-Wl,--no-whole-archive")
