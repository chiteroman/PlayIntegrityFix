cmake_minimum_required(VERSION 3.22.1)

project(zygisk)

include_directories(${CMAKE_SOURCE_DIR}/libcxx/include)

link_libraries(${CMAKE_SOURCE_DIR}/libcxx/${CMAKE_ANDROID_ARCH_ABI}.a)

add_library(${CMAKE_PROJECT_NAME} SHARED ${CMAKE_SOURCE_DIR}/main.cpp)

add_subdirectory(${CMAKE_SOURCE_DIR}/Dobby)

SET_OPTION(Plugin.Android.BionicLinkerUtil ON)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE c_std_23 cxx_std_23)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)