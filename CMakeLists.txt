cmake_minimum_required (VERSION 3.1.3)
project (prails VERSION 0.1)

##############################################################################
# Dependencies:
include(${CMAKE_ROOT}/Modules/FetchContent.cmake)

# Pistache:
FetchContent_Declare( pistache
  GIT_REPOSITORY "https://github.com/oktal/pistache.git" )
FetchContent_MakeAvailable(pistache)
FetchContent_GetProperties(pistache)
if(NOT pistache_POPULATED)
  FetchContent_Populate(pistache)
  add_subdirectory(${pistache_SOURCE_DIR} ${pistache_BINARY_DIR})
endif()

# Spdlog:
FetchContent_Declare( spdlog GIT_TAG v1.6.1
  GIT_REPOSITORY "https://github.com/gabime/spdlog.git" )
set(SPDLOG_BUILD_SHARED ON CACHE INTERNAL "Build shared object spdlog library")

FetchContent_MakeAvailable(spdlog)
FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
  FetchContent_Populate(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

# Google Test:
FetchContent_Declare( googletest GIT_TAG master
  GIT_REPOSITORY "https://github.com/google/googletest.git" )
FetchContent_MakeAvailable(googletest)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

include_directories(
  ${CMAKE_BINARY_DIR}/_deps/pistache-src/include
  ${CMAKE_BINARY_DIR}/_deps/pistache-src/third-party/rapidjson/include
  ${CMAKE_BINARY_DIR}/_deps/spdlog-src/include)

include_directories(include thirdparty/include)
include_directories(/usr/include/soci/ /usr/include/mysql/)
include_directories(${CMAKE_BINARY_DIR}/_deps/pistache-src/tests) # httplib.h
link_directories( ${CMAKE_BINARY_DIR}/_deps/spdlog-build)

#######################################################################
# Compiler Options
include(CheckCXXCompilerFlag)

# Flags for all builds:
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra \
  -Wno-missing-field-initializers -fPIC \
  -fstack-protector-all --param=ssp-buffer-size=4")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Release")
  # These optimizations cause Address Boundary segfaults in soci::to_base (during
  # Count queries?) on gcc-8. Newer gcc's are not affected. In any case, we disable 
  # them, since Release is built -O3: 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-forwprop -fno-tree-phiprop \
    -fno-tree-fre")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -O0 -ftest-coverage")
  link_libraries("-lgcov")
endif()

CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#######################################################################
# Test targets
option(BUILD_TESTS "build tests alongside the project" ON)

if (BUILD_TESTS)
  find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
  find_program(CTEST_COVERAGE_COMMAND NAMES gcov)
  find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)

  if (CMAKE_CXX_CPPCHECK)
    message("-- Cppcheck found " ${CMAKE_CXX_CPPCHECK})
    list(
      APPEND CMAKE_CXX_CPPCHECK
      "--enable=warning,style,performance,portability,unusedFunction"
      "-DCPPCHECK"
      "-I${PROJECT_SOURCE_DIR}/include"
      "--suppress=unmatchedSuppression"
      "--suppress=unusedFunction:*lib/utilities.cpp"
      "--suppress=*:*thirdparty/*"
      "--suppress=*:*_deps/*"
			"--inline-suppr"
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
  enable_testing()
  add_subdirectory(tests)
endif()

#######################################################################
# Project Components
add_subdirectory(lib)

add_library(prails SHARED lib/libprails.cpp)
set_target_properties(prails PROPERTIES VERSION ${PROJECT_VERSION})

target_link_libraries(prails -lpthread -lstdc++fs -lsoci_core -lsoci_sqlite3 -lsqlite3 
  -lsoci_mysql -lmysqlclient pistache_static spdlog utilities server config_parser)

target_link_libraries(prails 
  "-Wl,--whole-archive" controller "-Wl,--no-whole-archive")

#######################################################################
# Install Components
install(TARGETS prails COMPONENT runtime LIBRARY DESTINATION lib)
