function(declare_test test_name)
    set(TEST_EXECUTABLE run_${test_name})
    set(TEST_SOURCE ${test_name}.cpp)

    add_executable(${TEST_EXECUTABLE} ${TEST_SOURCE})
    target_link_libraries(${TEST_EXECUTABLE} gtest gtest_main functions 
      config_parser -lpthread -lsoci_core -lsoci_sqlite3 spdlog -lsqlite3 
      -lsoci_mysql -lmysqlclient )
    add_test(${test_name} ${TEST_EXECUTABLE})
endfunction()

function(declare_controller_test test_name)
    set(TEST_EXECUTABLE run_${test_name})
    set(TEST_SOURCE ${test_name}.cpp)

    add_executable(${TEST_EXECUTABLE} ${TEST_SOURCE})
    target_link_libraries(${TEST_EXECUTABLE} gtest gtest_main ${RM_LIBS} 
      -lpthread pistache_shared spdlog -lsoci_core -lsoci_sqlite3 
      -lsqlite3 -lsoci_mysql -lmysqlclient ${RM_LIBS} 
      "-Wl,--whole-archive" ${RM_CONTROLLERS} "-Wl,--no-whole-archive")
    add_test(${test_name} ${TEST_EXECUTABLE})
endfunction()

include_directories(../thirdparty/pistache/tests)

add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

declare_test(functions_test)
declare_test(config_parser_test)
declare_test(model_test)


