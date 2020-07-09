function(declare_test test_name)
  set(TEST_EXECUTABLE run_${test_name})
  set(TEST_SOURCE ${test_name}.cpp)

  add_executable(${TEST_EXECUTABLE} ${TEST_SOURCE})
  target_link_libraries(${TEST_EXECUTABLE} gtest gtest_main -lpthread 
    -lsoci_core -lsoci_sqlite3 -lsqlite3 -lsoci_mysql -lmysqlclient 
		pistache spdlog functions config_parser )
  add_test(${test_name} ${TEST_EXECUTABLE})
endfunction()

add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

declare_test(functions_test)
declare_test(config_parser_test)
declare_test(model_test)

# TODO: Dry this up ...
declare_test(post_body_test)
target_link_libraries(run_post_body_test
  "-Wl,--whole-archive" config_parser controller "-Wl,--no-whole-archive")

include_directories(${CMAKE_BINARY_DIR}/_deps/pistache-src/tests)

declare_test(rest_controller_test)
target_link_libraries(run_rest_controller_test server)
target_link_libraries(run_rest_controller_test
  "-Wl,--whole-archive" controller "-Wl,--no-whole-archive")
