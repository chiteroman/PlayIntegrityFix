cmake_minimum_required(VERSION 3.30.3)

project("playintegrityfix")

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

add_subdirectory(Dobby)

find_package(cxx REQUIRED CONFIG)

target_link_libraries(dobby cxx::cxx)
target_link_libraries(dobby_static cxx::cxx)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)
