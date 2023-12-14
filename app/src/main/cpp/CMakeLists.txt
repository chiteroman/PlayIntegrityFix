cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

link_libraries(${CMAKE_SOURCE_DIR}/libcxx/${CMAKE_ANDROID_ARCH_ABI}.a)

include_directories(libcxx/include)

add_library(${CMAKE_PROJECT_NAME} SHARED module.cpp)

add_subdirectory(Dobby)

SET_OPTION(Plugin.Android.BionicLinkerUtil ON)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)