cmake_minimum_required (VERSION 3.1.3)
project (prails VERSION 0.1)

#######################################################################
# Download external git dependencies before compile-time:
# From: https://github.com/google/googletest/tree/master/googletest
configure_file(CMakeLists.txt.in external/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/external )
if(result)
  message(FATAL_ERROR "CMake step for external dependency failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/external )
if(result)
  message(FATAL_ERROR "Build step for external dependency failed: ${result}")
endif()

# Add the external dependency paths to our build
include_directories(
	${CMAKE_CURRENT_BINARY_DIR}/external/pistache-prefix/src/pistache/include
	${CMAKE_CURRENT_BINARY_DIR}/external/spdlog-prefix/src/spdlog/include)

link_directories(
	${CMAKE_CURRENT_BINARY_DIR}/external/pistache-prefix/src/pistache-build/src
	${CMAKE_CURRENT_BINARY_DIR}/external/spdlog-prefix/src/spdlog-build)

#######################################################################
include(GNUInstallDirs)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -Wno-missing-field-initializers")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage -fstack-protector-all --param=ssp-buffer-size=4")
endif()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "build tests alongside the project" ON)

if (BUILD_TESTS)
  find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
  find_program(CTEST_COVERAGE_COMMAND NAMES gcov)
  find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)

  if (CMAKE_CXX_CPPCHECK)
    message("-- Cppcheck found " ${CMAKE_CXX_CPPCHECK})
    list(
      APPEND CMAKE_CXX_CPPCHECK
      "--enable=all"
      "-DCPPCHECK"
      "-I${PROJECT_SOURCE_DIR}/include"
      "--suppress=*:*/thirdparty/*"
    )
  else()
    message("-- Cppcheck not found")
    set(CMAKE_CXX_CPPCHECK "")
  endif()
    INCLUDE(Dart)
    add_custom_target(test_memcheck COMMAND ${CMAKE_CTEST_COMMAND}  
      --force-new-ctest-process --test-action memcheck)
    add_custom_target(coverage COMMAND ${CMAKE_CTEST_COMMAND}  
      --force-new-ctest-process --test-action coverage)
endif()

include_directories(include thirdparty/include thirdparty/pistache/include 
  thirdparty/spdlog/include)
include_directories(/usr/include/soci/ /usr/include/mysql/)
add_subdirectory(lib)

set(RM_LIBS functions server config_parser)
set(RM_CONTROLLERS controller)

if (BUILD_TESTS)
	add_subdirectory(
		${CMAKE_CURRENT_BINARY_DIR}/external/googletest-prefix/src/googletest
		${CMAKE_CURRENT_BINARY_DIR}/external/googletest-prefix/build
		EXCLUDE_FROM_ALL)
	include_directories(
		${CMAKE_CURRENT_BINARY_DIR}/external/pistache-prefix/src/pistache/tests)

  enable_testing()
  add_subdirectory(tests)
endif()

# TODO: I think we want to be an .so
add_executable(${CMAKE_PROJECT_NAME} "main.cpp")

target_link_libraries(${CMAKE_PROJECT_NAME} -lpthread -lsoci_core 
  -lsoci_sqlite3 -lsqlite3 -lsoci_mysql -lmysqlclient)
target_link_libraries(${CMAKE_PROJECT_NAME} pistache spdlog)
target_link_libraries(${CMAKE_PROJECT_NAME} ${RM_LIBS} )
target_link_libraries(${CMAKE_PROJECT_NAME} 
  "-Wl,--whole-archive" ${RM_CONTROLLERS} "-Wl,--no-whole-archive")

#set( CMAKE_VERBOSE_MAKEFILE on )

