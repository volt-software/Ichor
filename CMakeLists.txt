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

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(USE_SPDLOG "Use spdlog as framework logging implementation" OFF)
option(USE_RAPIDJSON "Add RapidJSON as a possible serializer implementation" OFF)
option(USE_PUBSUB "Add various dependencies to enable pubsub bundle to be built" OFF)
option(USE_ETCD "Add various dependencies to enable pubsub bundle to be built" OFF)
option(USE_SANITIZERS "Enable sanitizers, catching potential errors but slowing down compilation and execution speed" ON)
option(REMOVE_SOURCE_NAMES "Remove compiling source file names and line numbers when logging." OFF)

if(USE_SPDLOG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSPDLOG_COMPILED_LIB -DSPDLOG_NO_EXCEPTIONS -DSPDLOG_FMT_EXTERNAL -DUSE_SPDLOG -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE ")
endif()
if(USE_RAPIDJSON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_RAPIDJSON ")
endif()
if(REMOVE_SOURCE_NAMES)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREMOVE_SOURCE_NAMES_FROM_LOGGING ")
endif()
if(USE_ETCD)
    find_package(Protobuf REQUIRED)
    find_package(gRPC CONFIG)

    if(NOT gRPC_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_search_module(GRPC REQUIRED grpc)
        pkg_search_module(GRPCPP REQUIRED grpc++>=1.16.0)
        set(ETCD_TARGET ${GRPC_LIBRARIES} ${GRPCPP_LIBRARIES} ${PROTOBUF_LIBRARIES})
    else()
        set(ETCD_TARGET gRPC::grpc++ ${PROTOBUF_LIBRARIES})
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_ETCD ")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCATCH_CONFIG_FAST_COMPILE ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wno-unused-variable -Wno-long-long -Wno-unused-parameter -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -pedantic -Wformat -Wformat-security ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts -pthread ")
if(USE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined ")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong -fstack-clash-protection -fcf-protection -fPIE")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g3 -ggdb -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -ggdb -Og")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")

find_package (Threads)

file(GLOB_RECURSE FMT_SOURCES ${EXTERNAL_DIR}/fmt/src/*.cc)
file(GLOB_RECURSE CPPELIX_FRAMEWORK_SOURCES ${TOP_DIR}/src/framework/*.cpp)
file(GLOB_RECURSE CPPELIX_OPTIONAL_ETCD_SOURCES ${TOP_DIR}/src/optional_bundles/etcd_bundle/*.cpp ${TOP_DIR}/src/optional_bundles/etcd_bundle/*.cc)
file(GLOB_RECURSE CPPELIX_LOGGING_SOURCES ${TOP_DIR}/src/optional_bundles/logging_bundle/*.cpp)
file(GLOB_RECURSE CPPELIX_NETWORK_SOURCES ${TOP_DIR}/src/optional_bundles/network_bundle/*.cpp)
file(GLOB_RECURSE CPPELIX_METRICS_SOURCES ${TOP_DIR}/src/optional_bundles/metrics_bundle/*.cpp)
file(GLOB_RECURSE CPPELIX_SERIALIZATION_SOURCES ${TOP_DIR}/src/optional_bundles/serialization_bundle/*.cpp)

set(uv_sources
        ${EXTERNAL_DIR}/libuv/src/fs-poll.c
        ${EXTERNAL_DIR}/libuv/src/idna.c
        ${EXTERNAL_DIR}/libuv/src/inet.c
        ${EXTERNAL_DIR}/libuv/src/random.c
        ${EXTERNAL_DIR}/libuv/src/strscpy.c
        ${EXTERNAL_DIR}/libuv/src/threadpool.c
        ${EXTERNAL_DIR}/libuv/src/timer.c
        ${EXTERNAL_DIR}/libuv/src/uv-common.c
        ${EXTERNAL_DIR}/libuv/src/uv-data-getter-setters.c
        ${EXTERNAL_DIR}/libuv/src/version.c
        ${EXTERNAL_DIR}/libuv/src/unix/async.c
        ${EXTERNAL_DIR}/libuv/src/unix/core.c
        ${EXTERNAL_DIR}/libuv/src/unix/dl.c
        ${EXTERNAL_DIR}/libuv/src/unix/fs.c
        ${EXTERNAL_DIR}/libuv/src/unix/getaddrinfo.c
        ${EXTERNAL_DIR}/libuv/src/unix/getnameinfo.c
        ${EXTERNAL_DIR}/libuv/src/unix/loop-watcher.c
        ${EXTERNAL_DIR}/libuv/src/unix/loop.c
        ${EXTERNAL_DIR}/libuv/src/unix/pipe.c
        ${EXTERNAL_DIR}/libuv/src/unix/poll.c
        ${EXTERNAL_DIR}/libuv/src/unix/process.c
        ${EXTERNAL_DIR}/libuv/src/unix/random-devurandom.c
        ${EXTERNAL_DIR}/libuv/src/unix/signal.c
        ${EXTERNAL_DIR}/libuv/src/unix/stream.c
        ${EXTERNAL_DIR}/libuv/src/unix/tcp.c
        ${EXTERNAL_DIR}/libuv/src/unix/thread.c
        ${EXTERNAL_DIR}/libuv/src/unix/tty.c
        ${EXTERNAL_DIR}/libuv/src/unix/udp.c
        ${EXTERNAL_DIR}/libuv/src/unix/linux-core.c
        ${EXTERNAL_DIR}/libuv/src/unix/linux-inotify.c
        ${EXTERNAL_DIR}/libuv/src/unix/linux-syscalls.c
        ${EXTERNAL_DIR}/libuv/src/unix/procfs-exepath.c
        ${EXTERNAL_DIR}/libuv/src/unix/random-getrandom.c
        ${EXTERNAL_DIR}/libuv/src/unix/random-sysctl-linux.c)

set(uv_defines -D _GNU_SOURCE -D _POSIX_C_SOURCE=200112)

if(USE_SPDLOG)
    file(GLOB SPDLOG_SOURCES ${EXTERNAL_DIR}/spdlog/src/*.cpp)
endif()

add_library(cppelix ${uv_sources} ${FMT_SOURCES} ${CPPELIX_FRAMEWORK_SOURCES} ${CPPELIX_LOGGING_SOURCES} ${CPPELIX_NETWORK_SOURCES} ${CPPELIX_METRICS_SOURCES} ${CPPELIX_SERIALIZATION_SOURCES})
if(USE_SPDLOG)
    target_sources(cppelix PRIVATE ${SPDLOG_SOURCES})
endif()
if(USE_ETCD)
    target_link_libraries(cppelix ${ETCD_TARGET})
    target_sources(cppelix PRIVATE ${CPPELIX_OPTIONAL_ETCD_SOURCES})
endif()
target_link_libraries(cppelix ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(cppelix -ldl -lrt)
target_compile_definitions(cppelix PRIVATE ${uv_defines})
target_compile_options(cppelix PRIVATE -fPIC)

# no tests yet
#file(GLOB_RECURSE PROJECT_TEST_SOURCES ${TOP_DIR}/test/*.cpp)
#add_executable(cppelix_test ${PROJECT_TEST_SOURCES})
#if(USE_SPDLOG)
#    target_sources(cppelix_test PRIVATE ${SPDLOG_SOURCES})
#endif()
#add_test(NAME MyTest COMMAND cppelix_test)
#target_link_libraries(cppelix_test ${CMAKE_THREAD_LIBS_INIT})

include_directories("${EXTERNAL_DIR}/Catch2/single_include")
include_directories("${EXTERNAL_DIR}/wyhash")
include_directories("${EXTERNAL_DIR}/libuv/include")
include_directories("${EXTERNAL_DIR}/libuv/src")
include_directories("${EXTERNAL_DIR}/cppcoro/include")
include_directories("${EXTERNAL_DIR}/fmt/include")
include_directories("${TOP_DIR}/include")

if(USE_SPDLOG)
    include_directories("${EXTERNAL_DIR}/spdlog/include")
endif()

if(USE_RAPIDJSON)
    include_directories("${EXTERNAL_DIR}/rapidjson/include")
endif()

if(USE_ETCD)
    include_directories("${TOP_DIR}/include/optional_bundles/etcd_bundle")
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
