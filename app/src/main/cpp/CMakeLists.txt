cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

add_subdirectory(Dobby)

SET_OPTION(Plugin.Android.BionicLinkerUtil ON)

find_package(cxx REQUIRED CONFIG)

target_link_libraries(dobby cxx::cxx)

target_link_libraries(dobby_static cxx::cxx)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE cxx::cxx log dobby_static)