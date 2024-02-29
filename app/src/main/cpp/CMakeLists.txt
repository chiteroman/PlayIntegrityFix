cmake_minimum_required(VERSION 3.22.1)

project(playintegrityfix)

find_package(cxx REQUIRED CONFIG)
find_package(shadowhook REQUIRED CONFIG)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

target_link_libraries(${CMAKE_PROJECT_NAME} log cxx::cxx shadowhook::shadowhook)