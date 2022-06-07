cmake_minimum_required(VERSION 3.10)
project(supervisor)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED Core Network)
find_package(Boost REQUIRED system filesystem)
# https://github.com/jarro2783/cxxopts
find_package(cxxopts REQUIRED)
# https://github.com/ArthurSonzogni/FTXUI
find_package(ftxui REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
