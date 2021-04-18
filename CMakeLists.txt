cmake_minimum_required(VERSION 3.12)
project(ichor)
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

include(ExternalProject)

option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)
option(BUILD_TESTS "Build tests" ON)
option(USE_SPDLOG "Use spdlog as framework logging implementation" OFF)
option(USE_RAPIDJSON "Add RapidJSON as a possible serializer implementation" OFF)
option(USE_PUBSUB "Add various dependencies to enable pubsub bundle to be built" OFF)
option(USE_ETCD "Add various dependencies to enable pubsub bundle to be built" OFF)
option(USE_BOOST_BEAST "Add boost asio and boost BEAST as dependencies" OFF)
option(USE_SANITIZERS "Enable sanitizers, catching potential errors but slowing down compilation and execution speed" ON)
option(USE_THREAD_SANITIZER "Enable thread sanitizer, catching potential threading errors but slowing down compilation and execution speed. Cannot be combined with USE_SANITIZERS" OFF)
option(USE_UGLY_HACK_EXCEPTION_CATCHING "Enable an ugly hack on gcc to enable debugging the point where exceptions are thrown. Useful for debugging boost asio/beast backtraces." OFF)
option(REMOVE_SOURCE_NAMES "Remove compiling source file names and line numbers when logging." OFF)

if(USE_SPDLOG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSPDLOG_COMPILED_LIB -DSPDLOG_NO_EXCEPTIONS -DSPDLOG_FMT_EXTERNAL -DSPDLOG_DISABLE_DEFAULT_LOGGER -DSPDLOG_NO_ATOMIC_LEVELS -DUSE_SPDLOG -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE ")
endif()
if(USE_RAPIDJSON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_RAPIDJSON ")
endif()
if(REMOVE_SOURCE_NAMES)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DREMOVE_SOURCE_NAMES_FROM_LOGGING ")
endif()
if(USE_UGLY_HACK_EXCEPTION_CATCHING)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_UGLY_HACK_EXCEPTION_CATCHING ")
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

if(USE_BOOST_BEAST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_BOOST_BEAST ")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCATCH_CONFIG_FAST_COMPILE ")
set(CMAKE_CXX_FLAGSCMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wno-unused-variable -Wno-long-long -Wno-unused-parameter -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -pedantic -Wformat -Wformat-security ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts -pthread ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti ")

if(USE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -no-pie -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "-no-pie")
endif()

if(USE_THREAD_SANITIZER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -no-pie -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "-no-pie")
endif()

if(NOT DEFINED USE_SANITIZERS AND NOT DEFINED USE_THREAD_SANITIZER)
    # see https://github.com/google/sanitizers/issues/856
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong -fstack-clash-protection -fcf-protection")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
endif()

# gcc uses gwarf-4 by default, which messes up using the coz profiler
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -ggdb -gdwarf-3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -ggdb -gdwarf-3 -Og")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")

set(EXTERNAL_C_FLAGS "-fstack-protector-strong -fstack-clash-protection -fcf-protection")
if(USE_SANITIZERS)
    set(EXTERNAL_C_FLAGS "${EXTERNAL_C_FLAGS} -fsanitize=address,undefined -no-pie -fno-omit-frame-pointer")
    set(EXTERNAL_LINKER_FLAGS "-no-pie")
else()
    set(EXTERNAL_C_FLAGS "${EXTERNAL_C_FLAGS} -fPIE ")
endif()

find_package (Threads)

file(GLOB_RECURSE FMT_SOURCES ${EXTERNAL_DIR}/fmt/src/*.cc)
file(GLOB_RECURSE ICHOR_FRAMEWORK_SOURCES ${TOP_DIR}/src/ichor/*.cpp)
file(GLOB_RECURSE ICHOR_OPTIONAL_ETCD_SOURCES ${TOP_DIR}/src/optional_bundles/etcd_bundle/*.cpp ${TOP_DIR}/src/optional_bundles/etcd_bundle/*.cc)
file(GLOB_RECURSE ICHOR_LOGGING_SOURCES ${TOP_DIR}/src/optional_bundles/logging_bundle/*.cpp)
file(GLOB_RECURSE ICHOR_NETWORK_SOURCES ${TOP_DIR}/src/optional_bundles/network_bundle/*.cpp)
file(GLOB_RECURSE ICHOR_METRICS_SOURCES ${TOP_DIR}/src/optional_bundles/metrics_bundle/*.cpp)
file(GLOB_RECURSE ICHOR_SERIALIZATION_SOURCES ${TOP_DIR}/src/optional_bundles/serialization_bundle/*.cpp)

file(GLOB SPDLOG_SOURCES ${EXTERNAL_DIR}/spdlog/src/*.cpp)

add_library(ichor ${FMT_SOURCES} ${ICHOR_FRAMEWORK_SOURCES} ${ICHOR_LOGGING_SOURCES} ${ICHOR_NETWORK_SOURCES} ${ICHOR_METRICS_SOURCES} ${ICHOR_SERIALIZATION_SOURCES})

if(USE_SPDLOG)
    target_sources(ichor PRIVATE ${SPDLOG_SOURCES})
endif()

if(USE_ETCD)
    target_sources(ichor PRIVATE ${ICHOR_OPTIONAL_ETCD_SOURCES})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGOOGLE_PROTOBUF_NO_RTTI")
endif()

if(USE_BOOST_BEAST)
#TODO figure out how to make asio/beast compile as libraries, rather than header only. This would hopefully save some compilation time.
#    target_link_libraries(ichor asio beast)
    target_link_libraries(ichor boost_coroutine)
    target_compile_definitions(ichor PUBLIC "BOOST_BEAST_USE_STD_STRING_VIEW")
endif()

target_link_libraries(ichor ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(ichor -ldl -lrt)

include_directories("${EXTERNAL_DIR}/wyhash")
include_directories("${EXTERNAL_DIR}/cppcoro/include")
include_directories("${EXTERNAL_DIR}/fmt/include")
include_directories("${EXTERNAL_DIR}/robin-map/include")
include_directories("${EXTERNAL_DIR}/parallel-hashmap")
include_directories("${TOP_DIR}/include")

if(USE_SPDLOG)
    include_directories("${EXTERNAL_DIR}/spdlog/include")
endif()

if(USE_RAPIDJSON)
    include_directories("${EXTERNAL_DIR}/rapidjson/include")
endif()

if(USE_ETCD)
    #necessary to get generator protobuf/grpc code to compile
    include_directories("${TOP_DIR}/include/ichor/optional_bundles/etcd_bundle")
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
if(BUILD_TESTS)
    add_subdirectory(external/Catch2)
    add_subdirectory(test)
endif()


# no tests yet
#file(GLOB_RECURSE PROJECT_TEST_SOURCES ${TOP_DIR}/test/*.cpp)
#add_executable(ichor_test ${PROJECT_TEST_SOURCES})
#if(USE_SPDLOG)
#    target_sources(ichor_test PRIVATE ${SPDLOG_SOURCES})
#endif()
#add_test(NAME MyTest COMMAND ichor_test)
#target_link_libraries(ichor_test ${CMAKE_THREAD_LIBS_INIT})