include_directories(../include)

add_library(functions functions.cpp)
add_library(server server.cpp)
add_library(controller controller.cpp)
add_library(config_parser config_parser.cpp)


# TODO: remove

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

target_link_libraries(controller functions)
target_link_libraries(config_parser functions -lyaml-cpp)
