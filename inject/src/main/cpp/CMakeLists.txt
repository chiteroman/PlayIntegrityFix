cmake_minimum_required(VERSION 3.10)

project("inject")

link_libraries(log)

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(inject SHARED inject.cpp)

add_subdirectory(Dobby)

target_link_libraries(inject PRIVATE dobby_static)
