cmake_minimum_required(VERSION 3.10)
project(supervisor)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 REQUIRED Core Network)

add_subdirectory(third_party)
add_subdirectory(src)
