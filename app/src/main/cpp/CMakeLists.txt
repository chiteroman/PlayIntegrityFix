cmake_minimum_required(VERSION 3.5)

project("playintegrityfix")

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(zygisk SHARED main.cpp)

target_link_libraries(zygisk PRIVATE log)

add_library(inject SHARED inject.cpp)

add_subdirectory(Dobby)

target_link_libraries(inject PRIVATE log dobby_static)