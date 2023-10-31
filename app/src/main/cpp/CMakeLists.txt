cmake_minimum_required(VERSION 3.22.1)

project(zygisk)

include_directories(libcxx/include)

link_libraries(${CMAKE_SOURCE_DIR}/libcxx/${CMAKE_ANDROID_ARCH_ABI}.a)

add_subdirectory(Dobby)

add_library(${CMAKE_PROJECT_NAME} SHARED
        main.cpp)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE
        c_std_23
        cxx_std_23)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
        -fno-exceptions
        -fno-rtti
        -fvisibility=hidden
        -fvisibility-inlines-hidden)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        log
        dobby_static)