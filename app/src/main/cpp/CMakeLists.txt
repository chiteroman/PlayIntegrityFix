cmake_minimum_required(VERSION 3.22.1)

project(zygisk)

find_package(cxx REQUIRED CONFIG)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

add_subdirectory(Dobby)

SET_OPTION(Plugin.Android.BionicLinkerUtil ON)

target_link_libraries(dobby cxx::cxx)

target_link_libraries(dobby_static cxx::cxx)

target_link_libraries(${CMAKE_PROJECT_NAME} log dobby_static cxx::cxx)