include_directories(../include)

add_library(post_body STATIC post_body.cpp)
add_library(utilities STATIC utilities.cpp)
add_library(server STATIC server.cpp)
add_library(controller STATIC controller.cpp)
add_library(config_parser STATIC config_parser.cpp)

target_link_libraries(controller utilities)
target_link_libraries(config_parser utilities -lyaml-cpp -lstdc++fs)
target_link_libraries(server -lstdc++fs)
