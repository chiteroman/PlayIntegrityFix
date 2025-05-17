cmake_minimum_required(VERSION 3.30.5)

project("playintegrityfix")

link_libraries(log)

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

add_subdirectory(Dobby)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE dobby_static)
