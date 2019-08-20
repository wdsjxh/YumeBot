cmake_minimum_required(VERSION 3.12)

project(YumeBot CXX)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.14/conan.cmake"
                  "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_add_remote(NAME Cafe
    URL https://api.bintray.com/conan/chino/Cafe)

conan_cmake_run(CONANFILE conanfile.txt
                BASIC_SETUP CMAKE_TARGETS
                BUILD missing)

add_subdirectory(YumeBot)

set(YUMEBOT_INCLUDE_TEST OFF CACHE BOOL "Include YumeBot.Test")

if(YUMEBOT_INCLUDE_TEST)
    add_subdirectory(YumeBot.Test)
endif()

set(YUMEBOT_INCLUDE_CLI OFF CACHE BOOL "Include YumeBot.Cli")

if(YUMEBOT_INCLUDE_CLI)
    add_subdirectory(YumeBot.Cli)
endif()
