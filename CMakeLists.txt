cmake_minimum_required(VERSION 3.25)
project(bank)

set(CMAKE_CXX_STANDARD 17)

include_directories("/usr/include/mariadb")
link_directories("/usr/local/lib/mariadb")
add_library(mariadbcpp STATIC IMPORTED)
set_property(TARGET mariadbcpp PROPERTY IMPORTED_LOCATION "/usr/local/lib/mariadb/libmariadbcpp.so")

add_executable(bank main.cpp)

target_link_libraries(bank mariadbcpp)
