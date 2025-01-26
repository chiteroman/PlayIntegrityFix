cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

add_library(zygisk SHARED main.cpp)

add_library(inject SHARED inject.cpp)

target_link_libraries(zygisk PRIVATE log stdc++)

find_package(cxx REQUIRED CONFIG)

add_subdirectory(Dobby)

target_link_libraries(dobby cxx::cxx)
target_link_libraries(dobby_static cxx::cxx)

target_link_libraries(inject PRIVATE log dobby_static)
