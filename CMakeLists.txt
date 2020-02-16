cmake_minimum_required(VERSION 3.12)
project(cppelix)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE CXX STANDARD REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(TOP_DIR ${CMAKE_SOURCE_DIR})
set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/external/)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

option(USE_SPDLOG "Use spdlog as framework logging implementation" OFF)
option(USE_SANITIZERS "Enable sanitizers, catching potential errors but slowing down compilation and execution speed" ON)

if(USE_SPDLOG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSPDLOG_COMPILED_LIB -DSPDLOG_NO_EXCEPTIONS -DSPDLOG_FMT_EXTERNAL -DUSE_SPDLOG ")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCATCH_CONFIG_FAST_COMPILE ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wno-unused-variable -Wno-long-long -Wno-unused-parameter -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -pedantic -Wformat -Wformat-security ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts -pthread ")
if(USE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined ")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong -fstack-clash-protection -fcf-protection -fPIE")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -ggdb -Og")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")

find_package (Threads)

file(GLOB_RECURSE FMT_SOURCES ${EXTERNAL_DIR}/fmt/src/*.cc)
file(GLOB_RECURSE CPPELIX_FRAMEWORK_SOURCES ${TOP_DIR}/src/framework/*.cpp)
file(GLOB_RECURSE CPPELIX_OPTIONAL_BUNDLES_SOURCES ${TOP_DIR}/src/optional_bundles/*.cpp)

if(USE_SPDLOG)
    file(GLOB SPDLOG_SOURCE ${EXTERNAL_DIR}/spdlog/src/*.cpp)
endif()

add_library(cppelix ${SPDLOG_SOURCE} ${FMT_SOURCES} ${CPPELIX_FRAMEWORK_SOURCES} ${CPPELIX_OPTIONAL_BUNDLES_SOURCES})
if(USE_SPDLOG)
    target_sources(cppelix PRIVATE ${SPDLOG_SOURCES})
endif()
target_link_libraries(cppelix ${CMAKE_THREAD_LIBS_INIT})

file(GLOB_RECURSE PROJECT_TEST_SOURCES ${TOP_DIR}/test/*.cpp)
add_executable(cppelix_test ${FMT_SOURCES} ${CPPELIX_FRAMEWORK_SOURCES} ${CPPELIX_OPTIONAL_BUNDLES_SOURCES} ${PROJECT_TEST_SOURCES})
if(USE_SPDLOG)
    target_sources(cppelix_test PRIVATE ${SPDLOG_SOURCES})
endif()
add_test(NAME MyTest COMMAND cppelix_test)
target_link_libraries(cppelix_test ${CMAKE_THREAD_LIBS_INIT})

file(GLOB_RECURSE PROJECT_EXAMPLE_SOURCES ${TOP_DIR}/examples/*.cpp)
add_executable(cppelix_example ${SPDLOG_SOURCE} ${FMT_SOURCES} ${PROJECT_EXAMPLE_SOURCES})
if(USE_SPDLOG)
    target_sources(cppelix_example PRIVATE ${SPDLOG_SOURCES})
endif()
target_link_libraries(cppelix_example ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(cppelix_example cppelix)


include_directories("${EXTERNAL_DIR}/Catch2/single_include")
if(USE_SPDLOG)
    include_directories("${EXTERNAL_DIR}/spdlog/include")
endif()
include_directories("${EXTERNAL_DIR}/fmt/include")
include_directories("${TOP_DIR}/include")