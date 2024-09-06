cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

find_package(cxx REQUIRED CONFIG)

if (${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(ARCH "arm64")
elseif (${ANDROID_ABI} STREQUAL "armeabi-v7a")
    set(ARCH "arm")
endif ()

file(GLOB BYTEHOOK_SRC
        bytehook/*.c
)

add_library(${CMAKE_PROJECT_NAME} SHARED
        main.cpp
        cJSON/cJSON.c
        ${BYTEHOOK_SRC}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        cJSON
        bytehook/.
        bytehook/include
        bytehook/third_party/bsd
        bytehook/third_party/lss
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log cxx::cxx)
