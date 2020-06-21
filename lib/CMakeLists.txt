include_directories(../include)

add_library(functions functions.cpp)
add_library(server server.cpp)
add_library(controller controller.cpp)
add_library(config_parser config_parser.cpp)

# TODO: remove
#add_library(pages_controller pages_controller.cpp)
#add_library(auth_controller auth_controller.cpp)
#add_library(customer_controller customer_controller.cpp)
#add_library(tasks_controller tasks_controller.cpp)
#add_library(stations_controller stations_controller.cpp)

#add_library(agencies_controller agencies_controller.cpp)
#add_library(seasons_controller seasons_controller.cpp)
#add_library(station_aliases_controller station_aliases_controller.cpp)
#add_library(station_alternatives_controller station_alternatives_controller.cpp)

#target_link_libraries(auth_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
#  -lsoci_mysql -lmysqlclient)
#target_link_libraries(customer_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)
#target_link_libraries(tasks_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)
#target_link_libraries(stations_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)

#target_link_libraries(agencies_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)
#target_link_libraries(seasons_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)
#target_link_libraries(station_aliases_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)
#target_link_libraries(station_alternatives_controller -lsoci_core -lsoci_sqlite3 -lsqlite3 
  #-lsoci_mysql -lmysqlclient)

target_link_libraries(controller -lsass)
target_link_libraries(config_parser functions -lyaml-cpp)
