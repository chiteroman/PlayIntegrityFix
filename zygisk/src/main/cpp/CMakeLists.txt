cmake_minimum_required(VERSION 3.28)

project("zygisk")

add_library(zygisk SHARED zygisk.cpp)

target_link_libraries(zygisk log)
