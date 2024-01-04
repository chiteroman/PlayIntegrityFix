cmake_minimum_required(VERSION 3.22.1)

project(playintegrityfix)

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(${CMAKE_PROJECT_NAME} SHARED module.cpp)

add_subdirectory(Dobby)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)