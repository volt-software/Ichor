cmake_minimum_required(VERSION 3.18)
project(ichor)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE CXX STANDARD REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_policy(SET CMP0110 NEW)

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
    set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
    set(ICHOR_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(ICHOR_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
    set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
else()
    set(ICHOR_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(ICHOR_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/)
endif()

include(CTest)

option(ICHOR_BUILD_EXAMPLES "Build examples" ON)
option(ICHOR_BUILD_BENCHMARKS "Build benchmarks" ON)
option(ICHOR_BUILD_TESTING "Build tests" ON)
option(ICHOR_USE_SPDLOG "Use spdlog as framework logging implementation" OFF)
option(ICHOR_USE_PUBSUB "Add various dependencies to enable pubsub bundle to be built" OFF)
option(ICHOR_USE_ETCD "Add various dependencies to enable pubsub bundle to be built" OFF)
option(ICHOR_USE_BOOST_BEAST "Add boost asio and boost BEAST as dependencies" OFF)
option(ICHOR_USE_SANITIZERS "Enable sanitizers, catching potential errors but slowing down compilation and execution speed" ON)
option(ICHOR_USE_THREAD_SANITIZER "Enable thread sanitizer, catching potential threading errors but slowing down compilation and execution speed. Cannot be combined with ICHOR_USE_SANITIZERS" OFF)
option(ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING "Enable an ugly hack on gcc to enable debugging the point where exceptions are thrown. Useful for debugging boost asio/beast backtraces." OFF)
option(ICHOR_REMOVE_SOURCE_NAMES "Remove compiling source file names and line numbers when logging." OFF)

set(ICHOR_SERIALIZATION_FRAMEWORK OFF CACHE STRING "Enable serialization support")
set_property(CACHE ICHOR_SERIALIZATION_FRAMEWORK PROPERTY STRINGS OFF RAPIDJSON BOOST_JSON)

set(ICHOR_ARCH_OPTIMIZATION OFF CACHE STRING "Tell compiler to optimize for target")
set_property(CACHE ICHOR_ARCH_OPTIMIZATION PROPERTY STRINGS OFF NATIVE X86_64 X86_64_AVX2)

if(ICHOR_USE_SPDLOG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSPDLOG_COMPILED_LIB -DSPDLOG_NO_EXCEPTIONS -DSPDLOG_FMT_EXTERNAL -DSPDLOG_DISABLE_DEFAULT_LOGGER -DSPDLOG_NO_ATOMIC_LEVELS -DICHOR_USE_SPDLOG -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE ")
endif()
if(ICHOR_SERIALIZATION_FRAMEWORK STREQUAL "RAPIDJSON")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_RAPIDJSON ")
elseif(ICHOR_SERIALIZATION_FRAMEWORK STREQUAL "BOOST_JSON")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_BOOST_JSON -DBOOST_JSON_STANDALONE")
endif()
if(ICHOR_REMOVE_SOURCE_NAMES)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING ")
endif()
if(ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING ")
endif()
if(ICHOR_USE_ETCD)
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

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DICHOR_USE_ETCD ")
endif()

if(ICHOR_USE_BOOST_BEAST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DICHOR_USE_BOOST_BEAST ")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCATCH_CONFIG_FAST_COMPILE ")
set(CMAKE_CXX_FLAGSCMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wno-unused-variable -Wno-long-long -Wno-unused-parameter -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -pedantic -Wformat -Wformat-security ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts -fconcepts-diagnostics-depth=3 -pthread ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti ")


if(ICHOR_ARCH_OPTIMIZATION STREQUAL "NATIVE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native ")
elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64 ")
elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64-v3 ")
endif()

if(ICHOR_USE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -no-pie -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "-no-pie")

    if(ICHOR_USE_BOOST_BEAST)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_USE_ASAN -DBOOST_USE_UCONTEXT")
    endif()
endif()

if(ICHOR_USE_THREAD_SANITIZER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -no-pie -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "-no-pie")
endif()

if(NOT DEFINED ICHOR_USE_SANITIZERS AND NOT DEFINED ICHOR_USE_THREAD_SANITIZER)
    # see https://github.com/google/sanitizers/issues/856
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong -fstack-clash-protection -fcf-protection")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
endif()

# gcc uses gdwarf-4 by default, which messes up using the coz profiler
#-gdwarf-3
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ggdb")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-ggdb3 -Og")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")

find_package (Threads)

file(GLOB_RECURSE FMT_SOURCES ${ICHOR_EXTERNAL_DIR}/fmt/src/*.cc)
file(GLOB_RECURSE ICHOR_FRAMEWORK_SOURCES ${ICHOR_TOP_DIR}/src/ichor/*.cpp)
file(GLOB_RECURSE ICHOR_OPTIONAL_ETCD_SOURCES ${ICHOR_TOP_DIR}/src/optional_bundles/etcd_bundle/*.cpp ${ICHOR_TOP_DIR}/src/optional_bundles/etcd_bundle/*.cc)
file(GLOB_RECURSE ICHOR_LOGGING_SOURCES ${ICHOR_TOP_DIR}/src/optional_bundles/logging_bundle/*.cpp)
file(GLOB_RECURSE ICHOR_NETWORK_SOURCES ${ICHOR_TOP_DIR}/src/optional_bundles/network_bundle/*.cpp)
file(GLOB_RECURSE ICHOR_METRICS_SOURCES ${ICHOR_TOP_DIR}/src/optional_bundles/metrics_bundle/*.cpp)
file(GLOB_RECURSE ICHOR_SERIALIZATION_SOURCES ${ICHOR_TOP_DIR}/src/optional_bundles/serialization_bundle/*.cpp)

file(GLOB SPDLOG_SOURCES ${ICHOR_EXTERNAL_DIR}/spdlog/src/*.cpp)

add_library(ichor ${FMT_SOURCES} ${ICHOR_FRAMEWORK_SOURCES} ${ICHOR_LOGGING_SOURCES} ${ICHOR_NETWORK_SOURCES} ${ICHOR_METRICS_SOURCES} ${ICHOR_SERIALIZATION_SOURCES})

if(ICHOR_USE_SPDLOG)
    target_sources(ichor PRIVATE ${SPDLOG_SOURCES})
endif()

if(ICHOR_USE_ETCD)
    target_sources(ichor PRIVATE ${ICHOR_OPTIONAL_ETCD_SOURCES})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGOOGLE_PROTOBUF_NO_RTTI")
endif()

if(ICHOR_USE_BOOST_BEAST)
#TODO figure out how to make asio/beast compile as libraries, rather than header only. This would hopefully save some compilation time.
#    target_link_libraries(ichor asio beast)
    target_link_libraries(ichor boost_coroutine)
    target_compile_definitions(ichor PUBLIC "BOOST_BEAST_USE_STD_STRING_VIEW")
endif()

target_link_libraries(ichor ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(ichor -ldl -lrt)

target_include_directories(ichor PUBLIC "${ICHOR_EXTERNAL_DIR}/wyhash")
target_include_directories(ichor PUBLIC "${ICHOR_EXTERNAL_DIR}/cppcoro/include")
target_include_directories(ichor PUBLIC "${ICHOR_EXTERNAL_DIR}/fmt/include")
target_include_directories(ichor PUBLIC "${ICHOR_TOP_DIR}/include")

if(ICHOR_USE_SPDLOG)
    target_include_directories(ichor PUBLIC "${ICHOR_EXTERNAL_DIR}/spdlog/include")
endif()

if(ICHOR_SERIALIZATION_FRAMEWORK STREQUAL "RAPIDJSON")
    target_include_directories(ichor PUBLIC "${ICHOR_EXTERNAL_DIR}/rapidjson/include")
elseif(ICHOR_SERIALIZATION_FRAMEWORK STREQUAL "BOOST_JSON")
    target_include_directories(ichor PUBLIC "${ICHOR_EXTERNAL_DIR}/json/include")
endif()

if(ICHOR_USE_ETCD)
    #necessary to get generator protobuf/grpc code to compile
    target_include_directories(ichor PUBLIC "${ICHOR_TOP_DIR}/include/ichor/optional_bundles/etcd_bundle")
endif()

if(ICHOR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(ICHOR_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
if(ICHOR_BUILD_TESTING)
    add_subdirectory(${ICHOR_EXTERNAL_DIR}/Catch2)
    list(APPEND CMAKE_MODULE_PATH "${ICHOR_EXTERNAL_DIR}/Catch2/extras")

    enable_testing()
    add_subdirectory(test)
endif()
