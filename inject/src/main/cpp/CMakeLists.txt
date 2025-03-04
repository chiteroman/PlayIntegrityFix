cmake_minimum_required(VERSION 3.28)

project("inject")

add_library(inject SHARED inject.cpp)

add_subdirectory(Dobby)

target_link_libraries(inject log dobby_static)
