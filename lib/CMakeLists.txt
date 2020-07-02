include_directories(../include)

add_library(functions functions.cpp)
add_library(server server.cpp)
add_library(controller controller.cpp)
add_library(config_parser config_parser.cpp)

target_link_libraries(controller functions)
target_link_libraries(config_parser functions -lyaml-cpp)
