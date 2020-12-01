function(declare_test test_name)
  set(TEST_EXECUTABLE run_${test_name})
  set(TEST_SOURCE ${test_name}.cpp)

  add_executable(${TEST_EXECUTABLE} ${TEST_SOURCE})
  target_link_libraries(${TEST_EXECUTABLE} gtest gtest_main)

  target_link_libraries(${TEST_EXECUTABLE} -lpthread -lstdc++fs -lsoci_core -lsoci_sqlite3 -lsqlite3 
  -lsoci_mysql -lmysqlclient spdlog server pistache_static prails )
  target_link_libraries(${TEST_EXECUTABLE} 
  "-Wl,--whole-archive" controller config_parser "-Wl,--no-whole-archive")

  add_test(${test_name} ${TEST_EXECUTABLE})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 300) 
endfunction()

add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_definitions(-DPROJECT_BINARY_DIR="${PROJECT_BINARY_DIR}")
add_definitions(-DTESTS_CONFIG_FILE="${PROJECT_SOURCE_DIR}/tests/config/test-server.yml")

declare_test(utilities_test)
declare_test(config_parser_test)
declare_test(model_test)
declare_test(model_logging_test)
declare_test(post_body_test)
declare_test(rest_controller_test)
declare_test(logger_concurrency_test)
declare_test(model_tm_zone_test)
declare_test(server_test)
