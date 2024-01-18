cmake_minimum_required(VERSION 3.22.1)

project("playintegrityfix")

include_directories(${CMAKE_SOURCE_DIR}/libcxx/include)

link_libraries(${CMAKE_SOURCE_DIR}/libcxx/${CMAKE_ANDROID_ARCH_ABI}/libcxx.a)

add_library(${CMAKE_PROJECT_NAME} SHARED
        main.cpp)

if (NOT TARGET dobby)
    set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/Dobby)
    macro(SET_OPTION option value)
        set(${option} ${value} CACHE INTERNAL "" FORCE)
    endmacro()
    SET_OPTION(DOBBY_DEBUG OFF)
    SET_OPTION(DOBBY_GENERATE_SHARED OFF)
    SET_OPTION(Plugin.Android.BionicLinkerUtil ON)
    add_subdirectory(${DOBBY_DIR} dobby)
    get_property(DOBBY_INCLUDE_DIRECTORIES
            TARGET dobby
            PROPERTY INCLUDE_DIRECTORIES)
    include_directories(
            .
            ${DOBBY_INCLUDE_DIRECTORIES}
            $<TARGET_PROPERTY:dobby,INCLUDE_DIRECTORIES>
    )
endif ()

target_link_libraries(${CMAKE_PROJECT_NAME}
        log
        dobby)