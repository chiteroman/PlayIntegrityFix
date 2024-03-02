cmake_minimum_required(VERSION 3.22.1)

project(playintegrityfix)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_subdirectory(Dobby)

target_link_libraries(${CMAKE_PROJECT_NAME} log dobby_static)