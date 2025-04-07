cmake_minimum_required(VERSION 3.10)

project("zygisk")

link_libraries(log)

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

add_library(zygisk SHARED zygisk.cpp)

add_subdirectory(xdl)

target_link_libraries(zygisk PRIVATE xdl)
