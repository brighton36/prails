include_directories(../include)

add_library(utilities utilities.cpp)
add_library(server server.cpp)
add_library(controller controller.cpp)
add_library(config_parser config_parser.cpp)

target_link_libraries(controller utilities)
target_link_libraries(config_parser utilities -lyaml-cpp)
