cmake_minimum_required(VERSION 3.22.1)
project(zygisk)

include_directories(D:/libcxx_build/jni/libcxx/include)

link_libraries(D:/libcxx_build/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libcxx.a)

if ((${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a"))

    file(GLOB shadowhook_src shadowhook/*.c shadowhook/arch/arm64/*.c shadowhook/common/*.c shadowhook/third_party/*/*.c)
    add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp ${shadowhook_src})
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE shadowhook shadowhook/arch/arm64 shadowhook/common shadowhook/include shadowhook/third_party/bsd shadowhook/third_party/lss shadowhook/third_party/xdl)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log)

elseif (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")

    file(GLOB shadowhook_src shadowhook/*.c shadowhook/arch/arm/*.c shadowhook/common/*.c shadowhook/third_party/*/*.c)
    add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp ${shadowhook_src})
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE shadowhook shadowhook/arch/arm shadowhook/common shadowhook/include shadowhook/third_party/bsd shadowhook/third_party/lss shadowhook/third_party/xdl)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log)

else ()

    add_library(${CMAKE_PROJECT_NAME} SHARED main.cpp)
    add_subdirectory(Dobby)
    SET_OPTION(Plugin.Android.BionicLinkerUtil ON)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE log dobby_static)

endif ()