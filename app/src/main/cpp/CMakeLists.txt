cmake_minimum_required(VERSION 3.22.1)
project(zygisk)

include_directories(D:/libcxx_build/jni/libcxx/include)

link_libraries(D:/libcxx_build/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libcxx.a)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)

add_subdirectory(Dobby)

SET_OPTION(Plugin.Android.BionicLinkerUtil ON)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)