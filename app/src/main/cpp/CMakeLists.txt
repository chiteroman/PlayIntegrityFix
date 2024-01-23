cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

if (${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(ARCH "arm64")
elseif (${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(ARCH "arm")
endif ()

file(GLOB SRC shadowhook/*.c shadowhook/arch/${ARCH}/*.c shadowhook/common/*.c shadowhook/third_party/*/*.c)

add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp ${SRC})

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC shadowhook/. shadowhook/include shadowhook/arch/${ARCH} shadowhook/common shadowhook/third_party/xdl shadowhook/third_party/bsd shadowhook/third_party/lss)

find_package(cxx REQUIRED CONFIG)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log cxx::cxx)