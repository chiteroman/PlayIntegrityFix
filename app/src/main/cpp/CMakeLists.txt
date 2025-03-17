cmake_minimum_required(VERSION 3.30.3)

project("playintegrityfix")

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

add_subdirectory(Dobby)

target_link_libraries(${CMAKE_PROJECT_NAME} log dobby_static)
