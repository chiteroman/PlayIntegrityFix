cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

find_package(cxx REQUIRED CONFIG)

link_libraries(cxx::cxx)

if (${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(ARCH "arm64")
elseif (${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(ARCH "arm")
endif ()

file(GLOB SHADOWHOOK_SRC
        shadowhook/*.c
        shadowhook/arch/${ARCH}/*.c
        shadowhook/common/*.c
        shadowhook/third_party/xdl/*.c
)

add_library(${CMAKE_PROJECT_NAME} SHARED
        main.cpp
        ${SHADOWHOOK_SRC}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        shadowhook
        shadowhook/arch/${ARCH}
        shadowhook/include
        shadowhook/common
        shadowhook/third_party/bsd
        shadowhook/third_party/lss
        shadowhook/third_party/xdl
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log)
