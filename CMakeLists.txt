cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0110 NEW)
cmake_policy(SET CMP0127 NEW)
project(ichor VERSION 0.5.0 DESCRIPTION "C++20 dependency injection framework" HOMEPAGE_URL https://github.com/volt-software/Ichor LANGUAGES C CXX)
if(NOT WIN32)
    # For windows we set this flag manually later on to enable the experimental C++20 stuff
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_C_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_EXTENSIONS OFF)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
string(TOLOWER ${CMAKE_CXX_COMPILER_ID} ICHOR_COMPILER_ID)

if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif()

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
    set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
    set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
endif()


if(ICHOR_COMPILER_ID STREQUAL "gnu" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12")
    message(FATAL_ERROR "Gcc below version 12 not supported. Detected: ${CMAKE_CXX_COMPILER_VERSION}")
endif()
if(ICHOR_COMPILER_ID STREQUAL "clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
    message(FATAL_ERROR "Clang below version 15 not supported.")
endif()

set(ICHOR_TOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ICHOR_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ICHOR_BUILDING_DEBUG ON)
else()
    set(ICHOR_BUILDING_DEBUG OFF)
endif()

option(ICHOR_BUILD_EXAMPLES "Build examples" ON)
option(ICHOR_BUILD_BENCHMARKS "Build benchmarks" ON)
option(ICHOR_BUILD_TESTING "Build tests" ON)
option(ICHOR_ENABLE_INTERNAL_DEBUGGING "Add verbose logging of Ichor internals" OFF)
option(ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING "Add verbose logging of Ichor coroutine internals" OFF)
option(ICHOR_ENABLE_INTERNAL_IO_DEBUGGING "Add verbose logging of Ichor I/O internals" OFF)
option(ICHOR_ENABLE_INTERNAL_STL_DEBUGGING "Add verbose logging of Ichor STL" OFF)
option(ICHOR_BUILD_COVERAGE "Build ichor with coverage" OFF)
option(ICHOR_USE_SPDLOG "Use spdlog as framework logging implementation" OFF)
cmake_dependent_option(ICHOR_USE_SANITIZERS "Enable sanitizers, catching potential errors but slowing down compilation and execution speed" $ICHOR_BUILDING_DEBUG "NOT ICHOR_MUSL" OFF)
cmake_dependent_option(ICHOR_USE_THREAD_SANITIZER "Enable thread sanitizer, catching potential threading errors but slowing down compilation and execution speed. Cannot be combined with ICHOR_USE_SANITIZERS" OFF "NOT WIN32" OFF)
option(ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING "Enable an ugly hack on gcc to enable debugging the point where exceptions are thrown. Useful for debugging boost asio/beast backtraces." OFF)
option(ICHOR_REMOVE_SOURCE_NAMES "Remove compiling source file names and line numbers when logging." $ICHOR_BUILDING_DEBUG)
cmake_dependent_option(ICHOR_USE_MOLD "Use mold when linking, recommended to use with gcc 12+ or clang" OFF "NOT WIN32" OFF)
cmake_dependent_option(ICHOR_USE_SDEVENT "Add sd-event based queue/integration" OFF "NOT WIN32" OFF)
option(ICHOR_DISABLE_RTTI "Disable RTTI. Reduces memory usage, disables dynamic_cast<>()" ON)
option(ICHOR_DISABLE_EXCEPTIONS "Disable exceptions" OFF)
option(ICHOR_USE_HARDENING "Uses compiler-specific flags which add stack protection and similar features, as well as adding safety checks in Ichor itself." ON)
cmake_dependent_option(ICHOR_USE_LIBCPP "Uses clang's libc++ stdlib if compiling with clang" ON "ICHOR_COMPILER_ID STREQUAL \"clang\"" OFF)
cmake_dependent_option(ICHOR_USE_MIMALLOC "Use mimalloc for significant performance improvements" ON "NOT ICHOR_USE_SANITIZERS AND NOT ICHOR_USE_THREAD_SANITIZER" OFF)
cmake_dependent_option(ICHOR_USE_SYSTEM_MIMALLOC "Use system or vendored mimalloc" OFF "NOT ICHOR_USE_SANITIZERS AND NOT ICHOR_USE_THREAD_SANITIZER" OFF)
cmake_dependent_option(ICHOR_USE_LIBURING "Add liburing based queue/integration" ON "NOT WIN32 AND NOT APPLE" OFF)
cmake_dependent_option(ICHOR_FORCE_32_BIT "Force add 32 bit to compile flags" OFF "(ICHOR_COMPILER_ID STREQUAL \"clang\" OR ICHOR_COMPILER_ID STREQUAL \"gnu\")" OFF)
cmake_dependent_option(ICHOR_ENABLE_INTERNAL_URING_DEBUGGING "Add verbose logging of Ichor uring queue" OFF "ICHOR_USE_LIBURING" OFF)

option(ICHOR_USE_BACKWARD "Use backward-cpp to print stacktraces on crashes or when the user wants to. Useful for debugging." $ICHOR_BUILDING_DEBUG)
option(ICHOR_USE_HIREDIS "Add hiredis dependency" OFF)
option(ICHOR_MUSL "Use when building for musl instead of glibc" OFF)
option(ICHOR_AARCH64 "Use when building for aarch64. Turns off some x86-specific compiler flags." OFF)

find_package(OpenSSL 3.0.0)

cmake_dependent_option(ICHOR_USE_OPENSSL "Support using OpenSSL for TLS" ON "OpenSSL_FOUND" OFF)
cmake_dependent_option(ICHOR_USE_BOOST_BEAST "Add boost asio and boost BEAST as dependencies" OFF "NOT ICHOR_USE_SANITIZERS AND NOT ICHOR_USE_THREAD_SANITIZER AND ICHOR_USE_OPENSSL" OFF)


# see https://github.com/llvm/llvm-project/issues/121704
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT (ICHOR_COMPILER_ID STREQUAL "clang" AND ICHOR_USE_SANITIZERS))
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
    if(LTO_SUPPORTED)
        set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        if(ICHOR_COMPILER_ID STREQUAL "clang")
            set(CMAKE_C_COMPILE_OPTIONS_IPO -flto=full)
            set(CMAKE_CXX_COMPILE_OPTIONS_IPO -flto=full)
        endif()
        if(ICHOR_COMPILER_ID STREQUAL "gnu")
            set(CMAKE_C_COMPILE_OPTIONS_IPO -flto=1 -fno-fat-lto-objects)
            set(CMAKE_CXX_COMPILE_OPTIONS_IPO -flto=1 -fno-fat-lto-objects)
        endif()
        set(ICHOR_LTO ON)
        message(STATUS "Enabled LTO")
    else()
        set(ICHOR_LTO OFF)
        message(STATUS "LTO not supported: ${LTO_ERROR}")
    endif()
else()
    set(ICHOR_LTO OFF)
    message(STATUS "Disabled LTO")
endif()

set(BUILD_TESTING OFF) #disable Catch 2 testing

if(ICHOR_COMPILER_ID STREQUAL "clang")
    # clang tidy requires the same compiler arguments as clang
    # some gcc arguments cause it to error out
    option(ICHOR_RUN_CLANG_TIDY "Runs clang-tidy every compile" OFF)
endif()

set(ICHOR_ARCH_OPTIMIZATION OFF CACHE STRING "Tell compiler to optimize for target")
set_property(CACHE ICHOR_ARCH_OPTIMIZATION PROPERTY STRINGS OFF NATIVE X86_64 X86_64_SSE4 X86_64_AVX2 X86_64_AVX512 MODERN_ARM_GENERIC RASPBERRY_PI_ONE RASPBERRY_PI_TWO RASPBERRY_PI_THREE RASPBERRY_PI_FOUR RASPBERRY_PI_FIVE)

if(ICHOR_COMPILER_ID STREQUAL "clang" AND ICHOR_RUN_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*,-llvmlibc-*,-readability-function-cognitive-complexity,-altera-*,-modernize-use-trailing-return-type,-concurrency-mt-unsafe,-fuchsia-default-arguments-calls,-android-*,-readability-identifier-length,-clang-analyzer-optin.cplusplus.UninitializedObject")
endif()

set(FMT_SOURCES ${ICHOR_EXTERNAL_DIR}/fmt/src/format.cc ${ICHOR_EXTERNAL_DIR}/fmt/src/os.cc)
file(GLOB_RECURSE ICHOR_FRAMEWORK_SOURCES ${ICHOR_TOP_DIR}/src/ichor/coroutines/*.cpp ${ICHOR_TOP_DIR}/src/ichor/dependency_management/*.cpp ${ICHOR_TOP_DIR}/src/ichor/DependencyManager.cpp ${ICHOR_TOP_DIR}/src/ichor/LifecycleManager.cpp ${ICHOR_TOP_DIR}/src/ichor/Service.cpp)
set(ICHOR_FRAMEWORK_QUEUE_SOURCES ${ICHOR_TOP_DIR}/src/ichor/event_queues/PriorityQueue.cpp ${ICHOR_TOP_DIR}/src/ichor/event_queues/EventQueue.cpp)
file(GLOB_RECURSE ICHOR_ETCD_SOURCES ${ICHOR_TOP_DIR}/src/services/etcd/*.cpp)
file(GLOB_RECURSE ICHOR_LOGGING_SOURCES ${ICHOR_TOP_DIR}/src/services/logging/*.cpp)
file(GLOB_RECURSE ICHOR_HTTP_SOURCES ${ICHOR_TOP_DIR}/src/services/network/http/*.cpp)
file(GLOB_RECURSE ICHOR_BOOST_BEAST_SOURCES ${ICHOR_TOP_DIR}/src/services/network/boost/*.cpp)
file(GLOB_RECURSE ICHOR_METRICS_SOURCES ${ICHOR_TOP_DIR}/src/services/metrics/*.cpp)
file(GLOB_RECURSE ICHOR_TIMER_SOURCES ${ICHOR_TOP_DIR}/src/services/timer/Timer.cpp ${ICHOR_TOP_DIR}/src/services/timer/TimerFactoryFactory.cpp)
file(GLOB_RECURSE ICHOR_HIREDIS_SOURCES ${ICHOR_TOP_DIR}/src/services/redis/*.cpp)
file(GLOB_RECURSE ICHOR_OPENSSL_SOURCES ${ICHOR_TOP_DIR}/src/services/network/ssl/openssl/*.cpp)
file(GLOB_RECURSE ICHOR_BASE64_SOURCES ${ICHOR_TOP_DIR}/src/base64/*.cpp)
set(ICHOR_STL_SOURCES ${ICHOR_TOP_DIR}/src/ichor/stl/AsyncSingleThreadedMutex.cpp ${ICHOR_TOP_DIR}/src/ichor/stl/LinuxUtils.cpp ${ICHOR_TOP_DIR}/src/ichor/stl/StringUtils.cpp)

if(NOT ICHOR_DISABLE_EXCEPTIONS)
    set(ICHOR_STL_SOURCES ${ICHOR_STL_SOURCES} ${ICHOR_TOP_DIR}/src/ichor/stl/Any.cpp)
endif()

set(ICHOR_IO_SOURCES ${ICHOR_TOP_DIR}/src/services/io/SharedOverThreadsAsyncFileIO.cpp)
set(ICHOR_TCP_SOURCES ${ICHOR_TOP_DIR}/src/services/network/tcp/TcpConnectionService.cpp ${ICHOR_TOP_DIR}/src/services/network/tcp/TcpHostService.cpp)
if(ICHOR_USE_LIBURING)
    set(ICHOR_FRAMEWORK_QUEUE_SOURCES ${ICHOR_FRAMEWORK_QUEUE_SOURCES} ${ICHOR_TOP_DIR}/src/ichor/event_queues/IOUringQueue.cpp)
    set(ICHOR_IO_SOURCES ${ICHOR_IO_SOURCES} ${ICHOR_TOP_DIR}/src/services/io/IOUringAsyncFileIO.cpp)
    set(ICHOR_TCP_SOURCES ${ICHOR_TCP_SOURCES} ${ICHOR_TOP_DIR}/src/services/network/tcp/IOUringTcpConnectionService.cpp ${ICHOR_TOP_DIR}/src/services/network/tcp/IOUringTcpHostService.cpp)
    set(ICHOR_TIMER_SOURCES ${ICHOR_TIMER_SOURCES} ${ICHOR_TOP_DIR}/src/services/timer/IOUringTimerFactoryFactory.cpp ${ICHOR_TOP_DIR}/src/services/timer/IOUringTimer.cpp)
endif()
if(ICHOR_USE_SDEVENT)
    set(ICHOR_FRAMEWORK_QUEUE_SOURCES ${ICHOR_FRAMEWORK_QUEUE_SOURCES} ${ICHOR_TOP_DIR}/src/ichor/event_queues/SdeventQueue.cpp)
endif()
if(ICHOR_USE_BOOST_BEAST)
    set(ICHOR_FRAMEWORK_QUEUE_SOURCES ${ICHOR_FRAMEWORK_QUEUE_SOURCES} ${ICHOR_TOP_DIR}/src/ichor/event_queues/BoostAsioQueue.cpp)
endif()

file(GLOB SPDLOG_SOURCES ${ICHOR_EXTERNAL_DIR}/spdlog/src/*.cpp)

if(ICHOR_USE_MIMALLOC AND NOT ICHOR_USE_SYSTEM_MIMALLOC)
    set(ICHOR_FRAMEWORK_SOURCES ${ICHOR_FRAMEWORK_SOURCES} ${ICHOR_TOP_DIR}/external/mimalloc/src/static.c)
endif()

add_library(ichor ${FMT_SOURCES} ${ICHOR_FRAMEWORK_SOURCES} ${ICHOR_FRAMEWORK_QUEUE_SOURCES} ${ICHOR_LOGGING_SOURCES} ${ICHOR_TCP_SOURCES} ${ICHOR_HTTP_SOURCES} ${ICHOR_METRICS_SOURCES} ${ICHOR_TIMER_SOURCES} ${ICHOR_IO_SOURCES} ${ICHOR_BASE64_SOURCES} ${ICHOR_STL_SOURCES} ${ICHOR_ETCD_SOURCES})

if(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    target_compile_definitions(ichor PUBLIC ICHOR_ENABLE_INTERNAL_DEBUGGING)
endif()
if(ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING)
    target_compile_definitions(ichor PUBLIC ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING)
endif()
if(ICHOR_ENABLE_INTERNAL_IO_DEBUGGING)
    target_compile_definitions(ichor PUBLIC ICHOR_ENABLE_INTERNAL_IO_DEBUGGING)
endif()
if(ICHOR_ENABLE_INTERNAL_STL_DEBUGGING)
    target_compile_definitions(ichor PUBLIC ICHOR_ENABLE_INTERNAL_STL_DEBUGGING)
endif()
if(ICHOR_ENABLE_INTERNAL_URING_DEBUGGING)
    target_compile_definitions(ichor PUBLIC ICHOR_ENABLE_INTERNAL_URING_DEBUGGING)
endif()

if(ICHOR_USE_SPDLOG)
    target_compile_definitions(ichor PUBLIC SPDLOG_COMPILED_LIB SPDLOG_NO_EXCEPTIONS SPDLOG_FMT_EXTERNAL SPDLOG_NO_ATOMIC_LEVELS ICHOR_USE_SPDLOG SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
endif()

if(ICHOR_REMOVE_SOURCE_NAMES)
    target_compile_definitions(ichor PUBLIC ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING)
endif()

if(ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING)
endif()

if(ICHOR_USE_BOOST_BEAST)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_BOOST_BEAST)
endif()
if(ICHOR_MUSL)
    target_compile_definitions(ichor PUBLIC ICHOR_MUSL)
    target_compile_options(ichor PUBLIC -static)
    target_link_options(ichor PUBLIC -static-libgcc -static-libstdc++ -static)
    # reduce filesize of produced binaries greatly
    # does not work with glibc, as that relies on magic sections
    target_compile_options(ichor PUBLIC -ffunction-sections -fdata-sections)
    target_link_options(ichor PUBLIC -Wl,--gc-sections)
endif()
if(ICHOR_AARCH64)
    target_compile_definitions(ichor PUBLIC ICHOR_AARCH64)
endif()
if(ICHOR_BUILDING_DEBUG)
    target_compile_definitions(ichor PUBLIC ICHOR_BUILDING_DEBUG)
endif()
if(ICHOR_LTO)
    if(ICHOR_COMPILER_ID STREQUAL "clang")
        target_compile_options(ichor PUBLIC -fwhole-program-vtables)
        target_link_options(ichor PUBLIC -Wl,--exclude-libs,ALL)
    endif()
    if(ICHOR_COMPILER_ID STREQUAL "gnu")
        target_compile_options(ichor PUBLIC -fwhole-program)
        target_link_options(ichor PUBLIC -Wl,--exclude-libs,ALL)
    endif()
endif()

if(NOT WIN32)
    # Everything is c++ specific so that the liburing source files, which don't meet Ichor's standards, don't spam warnings all over.
    target_compile_options(ichor PUBLIC
            -Wall
            -Wextra
            -Wshadow
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-virtual-dtor>
            -Wno-unused-variable
            -Wno-long-long
            -Wno-unused-parameter
            -Wnull-dereference
            $<$<COMPILE_LANGUAGE:CXX>:-pedantic>
            -Wformat
            -Wformat-security
            $<$<COMPILE_LANGUAGE:CXX>:-Wcast-align>
            $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
            $<$<COMPILE_LANGUAGE:CXX>:-Wdelete-non-virtual-dtor>
            $<$<COMPILE_LANGUAGE:CXX>:-Wconversion>)
    target_compile_options(ichor PUBLIC -pthread $<$<COMPILE_LANGUAGE:CXX>:-ftemplate-backtrace-limit=0>)

    if(ICHOR_COMPILER_ID STREQUAL "gnu")
        target_compile_options(ichor PUBLIC -Wtrampolines)
    endif()
    if(ICHOR_COMPILER_ID STREQUAL "clang")
        target_compile_options(ichor PUBLIC -fstrict-vtable-pointers)
    endif()

    if(CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    endif()

    if(ICHOR_FORCE_32_BIT)
        if(NOT CMAKE_TOOLCHAIN_FILE)
            message(FATAL_ERROR "Not using the toolchain-i386.cmake toolchain")
        elseif(NOT CMAKE_TOOLCHAIN_FILE MATCHES "toolchain-i386.cmake$")
            message(FATAL_ERROR "Not using the toolchain-i386.cmake toolchain")
        endif()
    endif()

    if(ICHOR_FORCE_32_BIT OR CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_definitions(ichor PUBLIC ICHOR_32_BIT)
    endif()

    # TODO Would love to refactor this to a function, but functions don't seem to be able to reach variables not yet defined when the function is defined.
    if(ICHOR_USE_HARDENING AND CMAKE_BUILD_TYPE MATCHES Debug)
        # target_compile_definitions annoyingly adds "-D" in front of "-U" and target_compile_options goes after target_compile_definitions. Argh.
        target_compile_options(ichor PUBLIC -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)

        # Enable runtime iterator debug checks (like checking two different iterators from different container
        target_compile_definitions(ichor PUBLIC _GLIBCXX_ASSERTIONS _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _ITERATOR_DEBUG_LEVEL=2 _LIBCPP_DEBUG=1 _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
    elseif(ICHOR_USE_HARDENING)
        # target_compile_definitions annoyingly adds "-D" in front of "-U" and target_compile_options goes after target_compile_definitions. Argh.
        target_compile_options(ichor PUBLIC -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)

        target_compile_definitions(ichor PUBLIC _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)

        target_compile_options(ichor PUBLIC -fno-delete-null-pointer-checks)
        if(ICHOR_COMPILER_ID STREQUAL "gnu" OR (ICHOR_COMPILER_ID STREQUAL "clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "18"))
            target_compile_options(ichor PUBLIC -fno-strict-aliasing)
        endif()
        if(ICHOR_COMPILER_ID STREQUAL "gnu" OR (ICHOR_COMPILER_ID STREQUAL "clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16"))
            target_compile_options(ichor PUBLIC -ftrivial-auto-var-init=pattern)
        endif()
    endif()
    target_compile_definitions(ichor PUBLIC _FILE_OFFSET_BITS=64 _TIME_BITS=64)

    # don't optimize away overflows
    target_compile_options(ichor PUBLIC -fwrapv)
else()
    #msvc sets some stupid defaults. We can do better.
    # it would be nice to have something similar to fwrapv for MSVC, but it seems that it either costs other ASM optimizations (with d2SSAOptimizer-) or the flag is removed (d2UndefIntOverflow-)
    set(CMAKE_CXX_FLAGS "/W4")
    target_compile_options(ichor PUBLIC /nologo /DWIN32 /D_WINDOWS /EHsc /TP /await:strict /std:c++latest /wd4100 /permissive- /Zc:throwingNew /Zc:__cplusplus /Zc:preprocessor /volatile:iso /Zc:inline /Zc:referenceBinding /Zc:strictStrings /Zc:templateScope /utf-8 /vd0)
    target_compile_options(ichor PUBLIC /bigobj) #some files, like WsConnectionService with sanitizers enabled, need more addressable sections
    target_compile_definitions(ichor PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN )
endif()

if(ICHOR_USE_LIBCPP)
    target_compile_options(ichor PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_LIBCPP)
    target_link_options(ichor PUBLIC -stdlib=libc++)
endif()

# gcc added support for mold in version 12, before that, the flag is ignored.
# Note that with older versions of mold and early versions of gcc 12, issues with the tests have been observed. Please upgrade to the latest if you notice any issues.
if(ICHOR_USE_MOLD AND NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold ")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold ")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=mold ")
    #target_link_options(ichor PUBLIC -Wl,--separate-debug-file) # TODO test mold version first, available since 2.33.0
endif()

if(ICHOR_COMPILER_ID STREQUAL "gnu")
    target_compile_options(ichor PUBLIC -Wduplicated-cond -Wduplicated-branches -Wlogical-op)
endif()

if(WIN32)
    if(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX2")
        target_compile_options(ichor PUBLIC /arch:AVX2)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX512")
        target_compile_options(ichor PUBLIC /arch:AVX512)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "MODERN_ARM_GENERIC")
        target_compile_options(ichor PUBLIC /arch:armv8.1)
    endif()
else()
    if(ICHOR_ARCH_OPTIMIZATION STREQUAL "NATIVE")
        target_compile_options(ichor PUBLIC -march=native)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64")
        target_compile_options(ichor PUBLIC -march=x86-64)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_SSE4")
        target_compile_options(ichor PUBLIC -march=x86-64-v2)
        target_link_options(ichor PUBLIC -Wl,-z,x86-64-v2)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX2")
        target_compile_options(ichor PUBLIC -march=x86-64-v3)
        target_link_options(ichor PUBLIC -Wl,-z,x86-64-v3)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX512")
        target_compile_options(ichor PUBLIC -march=x86-64-v4)
        target_link_options(ichor PUBLIC -Wl,-z,x86-64-v4)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "MODERN_ARM_GENERIC")
        target_compile_options(ichor PUBLIC -march=armv8-a)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "RASPBERRY_PI_ONE")
        if(ICHOR_AARCH64)
            target_compile_options(ichor PUBLIC -march=armv6zk -mcpu=arm1176jzf-s)
        else()
            target_compile_options(ichor PUBLIC -march=armv6zk -mcpu=arm1176jzf-s -mfpu=vfp)
        endif()
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "RASPBERRY_PI_TWO")
        if(ICHOR_AARCH64)
            target_compile_options(ichor PUBLIC -march=armv7-a -mcpu=cortex-a7)
        else()
            target_compile_options(ichor PUBLIC -march=armv7-a -mcpu=cortex-a7 -mfpu=neon-vfpv4)
        endif()
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "RASPBERRY_PI_THREE")
        if(ICHOR_AARCH64)
            target_compile_options(ichor PUBLIC -march=armv8-a+crc -mcpu=cortex-a53)
        else()
            target_compile_options(ichor PUBLIC -march=armv8-a+crc -mcpu=cortex-a53 -mfpu=crypto-neon-fp-armv8)
        endif()
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "RASPBERRY_PI_FOUR")
        if(ICHOR_AARCH64)
            target_compile_options(ichor PUBLIC -march=armv8-a+crc -mcpu=cortex-a72)
        else()
            target_compile_options(ichor PUBLIC -march=armv8-a+crc -mcpu=cortex-a72 -mfpu=crypto-neon-fp-armv8)
        endif()
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "RASPBERRY_PI_FIVE")
        if(ICHOR_AARCH64)
            target_compile_options(ichor PUBLIC -march=armv8-a+crc+crypto -mcpu=cortex-a76)
        else()
            target_compile_options(ichor PUBLIC -march=armv8-a+crc+crypto -mcpu=cortex-a76 -mfpu=crypto-neon-fp-armv8)
        endif()
    endif()
endif()

if(ICHOR_DISABLE_RTTI)
    if(WIN32)
        target_compile_options(ichor PUBLIC /GR-)
    else()
        target_compile_options(ichor PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    endif()
endif()

if(ICHOR_DISABLE_EXCEPTIONS)
    if(WIN32)
        target_compile_options(ichor PUBLIC /EHs-c-)
    else()
        target_compile_options(ichor PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(ichor PUBLIC -fno-unwind-tables -fno-asynchronous-unwind-tables)
        endif()
    endif()
endif()

if(ICHOR_USE_SANITIZERS)
    if(WIN32)
        # MSVC does not support UBsan
        target_compile_options(ichor PUBLIC /fsanitize=address /Zi)
    else()
        set(ICHOR_SANITIZE_OPTIONS -fsanitize=address,undefined)

        target_compile_options(ichor PUBLIC ${ICHOR_SANITIZE_OPTIONS} -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-common)
        target_link_options(ichor PUBLIC ${ICHOR_SANITIZE_OPTIONS})
        target_compile_definitions(ichor PUBLIC __SANITIZE_ADDRESS__ _GLIBCXX_SANITIZE_VECTOR)

        # clang on OSX doesn't accept -no-pie for some reason
        if(NOT APPLE)
            target_link_options(ichor PUBLIC -no-pie)
        endif()

        if(ICHOR_COMPILER_ID STREQUAL "gnu")
            target_compile_options(ichor PUBLIC -no-pie)
        endif()

        if(ICHOR_DISABLE_RTTI)
            # vptr UB checks not compatible with no-rtti
            target_compile_options(ichor PUBLIC -fno-sanitize=vptr)
        endif()
    endif()

    if(ICHOR_USE_BOOST_BEAST)
        target_compile_definitions(ichor PUBLIC BOOST_USE_ASAN)
        if(NOT WIN32 AND NOT APPLE) #boost provided windows compiled dlls/libs, as well as homebrew provided ones, do not provide ucontext
            target_compile_definitions(ichor PUBLIC BOOST_USE_UCONTEXT)
        endif()
        message(WARNING "Sanitizers seem to have issues with stack switches. Need to compile boost library specifically to deal with this, or continue at your own peril. Please see https://www.boost.org/doc/libs/master/libs/context/doc/html/context/stack/sanitizers.html and https://github.com/boostorg/beast/issues/2615 and https://learn.microsoft.com/en-us/answers/questions/652967/address-sanitizer-missing-output")
    endif()
endif()

if(ICHOR_USE_THREAD_SANITIZER)
    target_compile_options(ichor PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(ichor PUBLIC -fsanitize=thread)
    target_compile_definitions(ichor PUBLIC __SANITIZE_THREAD__)

    # clang on OSX doesn't accept -no-pie for some reason
    if(NOT APPLE)
        target_link_options(ichor PUBLIC -no-pie)
    endif()

    if(ICHOR_COMPILER_ID STREQUAL "gnu")
        target_compile_options(ichor PUBLIC -no-pie)
        target_link_options(ichor PUBLIC -static-libtsan)
    else()
        target_link_options(ichor PUBLIC -static-libsan)
    endif()

    if(ICHOR_USE_BOOST_BEAST)
        target_compile_definitions(ichor PUBLIC BOOST_USE_TSAN BOOST_USE_UCONTEXT)
        message(WARNING "Sanitizers seem to have issues with stack switches. Need to compile boost library specifically to deal with this, and then still thread sanitizer seems to have issues. Please see https://www.boost.org/doc/libs/master/libs/context/doc/html/context/stack/sanitizers.html and https://github.com/boostorg/beast/issues/2615")
    endif()
endif()

if(NOT WIN32 AND NOT ICHOR_USE_SANITIZERS AND NOT ICHOR_USE_THREAD_SANITIZER)
    # see https://github.com/google/sanitizers/issues/856
    target_compile_options(ichor PUBLIC -fpie)
    target_link_options(ichor PUBLIC -pie)
endif()

if(WIN32 AND ICHOR_USE_HARDENING)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        target_compile_options(ichor PUBLIC /RTCsu)
    endif()
    target_compile_options(ichor PUBLIC /GS /guard:cf)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_HARDENING)
elseif(ICHOR_USE_HARDENING)
    target_compile_options(ichor PUBLIC -fstack-protector-strong)

    if(NOT ICHOR_AARCH64)
        target_compile_options(ichor PUBLIC -fcf-protection=full)
    else()
        target_compile_options(ichor PUBLIC -mbranch-protection=standard)
    endif()

    # stack clash protection not available on OSX nor on AARCH64
    if(NOT APPLE AND NOT ICHOR_AARCH64)
        target_compile_options(ichor PUBLIC -fstack-clash-protection)
    endif()

    target_compile_definitions(ichor PUBLIC ICHOR_USE_HARDENING)

    if(NOT APPLE)
        target_link_options(ichor PUBLIC -Wl,-z,nodlopen -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -Wl,--as-needed -Wl,--no-copy-dt-needed-entries)
    else()
        target_link_options(ichor PUBLIC -Wl,-bind_at_load)
    endif()
endif()

if(ICHOR_COMPILER_ID STREQUAL "gnu")
    target_compile_options(ichor PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fconcepts-diagnostics-depth=3>)
    target_compile_options(ichor PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
elseif(ICHOR_COMPILER_ID STREQUAL "clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL "15")
    target_compile_options(ichor PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcoroutines-ts>)
endif()

if(NOT WIN32 AND NOT ICHOR_BUILD_COVERAGE)
    # gcc uses gdwarf-4 by default, which messes up using the coz profiler, add "-gdwarf-3" if using coz
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ggdb3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-ggdb -Og -DNDEBUG")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
endif()

if(ICHOR_BUILD_COVERAGE AND ICHOR_COMPILER_ID MATCHES "gnu|clang")
    target_compile_options(ichor PUBLIC -O0 -g --coverage)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(ichor PUBLIC --coverage)
    else()
        target_link_libraries(ichor PUBLIC --coverage)
    endif()
endif()

if(NOT WIN32)
    target_compile_options(ichor PUBLIC -ftls-model=local-exec)
endif()

# Gcc 12.1 and 12.2 have bugs that prevent compilation with Werror at -O2 and higher:
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107138
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
# 12.3 has a bug where [[maybe_unused]] in glaze is somehow ignored.
# boost still uses std::aligned_storage, so we can't use this until we find replacements for boost
#if(NOT ICHOR_COMPILER_ID STREQUAL "gnu" AND NOT WIN32)
#    target_compile_options(ichor PRIVATE "-Werror") #prevent externally compiled sources to error out on warnings
#endif()

# By default, we build a bundled mimalloc and statically-link it to
# Ichor. If you want to dynamically link to the system's
# libmimalloc.so, pass -DICHOR_USE_SYSTEM_MIMALLOC=ON.
if(ICHOR_USE_MIMALLOC)
    if(ICHOR_USE_SYSTEM_MIMALLOC)
        find_package(mimalloc REQUIRED)
        target_link_libraries(ichor PRIVATE mimalloc)
        target_compile_definitions(ichor PRIVATE ICHOR_USE_SYSTEM_MIMALLOC)
    else()
        if(ICHOR_USE_HARDENING OR ICHOR_ENABLE_INTERNAL_DEBUGGING)
            target_compile_definitions(ichor PRIVATE MI_SECURE=4)
        endif()
        if(ICHOR_ENABLE_INTERNAL_DEBUGGING)
            target_compile_definitions(ichor PRIVATE MI_DEBUG=3)
        endif()
        if(ICHOR_USE_SANITIZERS AND NOT WIN32)
            target_compile_definitions(ichor PRIVATE MI_TRACK_ASAN=1)
        endif()
        if(ICHOR_USE_THREAD_SANITIZER AND NOT WIN32)
            target_compile_definitions(ichor PRIVATE MI_TSAN=1)
        endif()
        if(ICHOR_MUSL)
            target_compile_definitions(ichor PRIVATE MI_LIBC_MUSL=1)
        endif()
        if(NOT WIN32)
            target_compile_options(ichor PUBLIC -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free)
        endif()
        target_compile_definitions(ichor PUBLIC ICHOR_USE_MIMALLOC)
        target_include_directories(ichor PUBLIC
                $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/mimalloc/include>
                $<INSTALL_INTERFACE:include/ichor/external>)
    endif()
endif()

if(ICHOR_USE_LIBURING)
    if(ICHOR_USE_SANITIZERS)
        execute_process(COMMAND "./configure" "--enable-sanitizer" WORKING_DIRECTORY ${ICHOR_EXTERNAL_DIR}/liburing/ RESULT_VARIABLE LIBURING_RESULT ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE COMMAND_ECHO STDOUT)
        set(LIBURING_SOURCES ${ICHOR_EXTERNAL_DIR}/liburing/src/nolibc.c ${ICHOR_EXTERNAL_DIR}/liburing/src/queue.c ${ICHOR_EXTERNAL_DIR}/liburing/src/register.c ${ICHOR_EXTERNAL_DIR}/liburing/src/setup.c ${ICHOR_EXTERNAL_DIR}/liburing/src/syscall.c ${ICHOR_EXTERNAL_DIR}/liburing/src/version.c ${ICHOR_EXTERNAL_DIR}/liburing/src/sanitize.c)
    else()
        execute_process(COMMAND "./configure" WORKING_DIRECTORY ${ICHOR_EXTERNAL_DIR}/liburing/ RESULT_VARIABLE LIBURING_RESULT ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE)
        set(LIBURING_SOURCES ${ICHOR_EXTERNAL_DIR}/liburing/src/nolibc.c ${ICHOR_EXTERNAL_DIR}/liburing/src/queue.c ${ICHOR_EXTERNAL_DIR}/liburing/src/register.c ${ICHOR_EXTERNAL_DIR}/liburing/src/setup.c ${ICHOR_EXTERNAL_DIR}/liburing/src/syscall.c ${ICHOR_EXTERNAL_DIR}/liburing/src/version.c)
    endif()

    if(NOT LIBURING_RESULT EQUAL 0)
        message(FATAL_ERROR "Liburing configure not successful: ${LIBURING_RESULT}")
    endif()

    target_sources(ichor PRIVATE ${LIBURING_SOURCES})
    target_compile_options(ichor PRIVATE -include "${ICHOR_EXTERNAL_DIR}liburing/config-host.h")
    target_compile_definitions(ichor PUBLIC _GNU_SOURCE ICHOR_USE_LIBURING)


    target_include_directories(ichor PUBLIC
            $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/liburing/src/arch>
            $<INSTALL_INTERFACE:include/ichor/external>)

    target_include_directories(ichor PUBLIC
            $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/liburing/src/include>
            $<INSTALL_INTERFACE:include/ichor/external>)
endif()

if(ICHOR_USE_SPDLOG)
    target_sources(ichor PRIVATE ${SPDLOG_SOURCES})
endif()
if(ICHOR_USE_BOOST_BEAST)
    target_sources(ichor PRIVATE ${ICHOR_BOOST_BEAST_SOURCES})
endif()

if(ICHOR_USE_SDEVENT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Systemd IMPORTED_TARGET GLOBAL libsystemd)
    if(NOT TARGET PkgConfig::Systemd)
        message(FATAL_ERROR "libsystemd was not found")
    endif()
    target_link_libraries(ichor PUBLIC ${Systemd_LIBRARIES})
    target_compile_definitions(ichor PUBLIC ICHOR_USE_SDEVENT)
endif()

if(ICHOR_USE_BOOST_BEAST) #beast
    find_package(Boost 1.70.0 REQUIRED COMPONENTS context coroutine fiber)
    target_include_directories(ichor PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_directories(ichor PUBLIC ${Boost_LIBRARY_DIRS})
    target_link_libraries(ichor PUBLIC ${Boost_LIBRARIES})
    #_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS -> MSVC gives warnings on things like std::aligned_storage, which is still valid in C++20.
    target_compile_definitions(ichor PUBLIC BOOST_ASIO_NO_DEPRECATED _SILENCE_ALL_CXX23_DEPRECATION_WARNINGS)

    if(Boost_VERSION_STRING VERSION_LESS "1.81.0")
        target_compile_definitions(ichor PUBLIC BOOST_BEAST_USE_STD_STRING_VIEW)
    endif()

    if(ICHOR_USE_HARDENING OR ICHOR_ENABLE_INTERNAL_DEBUGGING)
        target_compile_definitions(ichor PUBLIC BOOST_ASIO_ENABLE_BUFFER_DEBUGGING)
    endif()
endif()

if(ICHOR_USE_HIREDIS)
    find_package(Hiredis REQUIRED)
    target_link_libraries(ichor PUBLIC hiredis)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_HIREDIS)

    target_sources(ichor PRIVATE ${ICHOR_HIREDIS_SOURCES})
endif()

if(ICHOR_USE_OPENSSL)
    find_package(OpenSSL REQUIRED)

    target_include_directories(ichor PUBLIC ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(ichor PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_OPENSSL OPENSSL_NO_DEPRECATED OPENSSL_API_COMPAT=30000)

    target_sources(ichor PRIVATE ${ICHOR_OPENSSL_SOURCES})
endif()

target_link_libraries(ichor PUBLIC ${CMAKE_THREAD_LIBS_INIT})
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(ichor PUBLIC -ldl -lrt)
endif()

target_include_directories(ichor PUBLIC
        $<BUILD_INTERFACE:${ICHOR_TOP_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_include_directories(ichor PUBLIC
        $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/fmt/include>
        $<INSTALL_INTERFACE:include/ichor/external>)

if(ICHOR_USE_SPDLOG)
    target_include_directories(ichor PUBLIC
            $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/spdlog/include>
            $<INSTALL_INTERFACE:include/ichor/external>)
endif()

target_include_directories(ichor PUBLIC
        $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/glaze/include>
        $<INSTALL_INTERFACE:include/ichor/external>)

# Detection for backward-cpp, works without external libraries on windows and apple
# On linux, it still works without these libraries (on gcc + glibc at least), but provides less functionality.
if(ICHOR_USE_BACKWARD AND NOT ICHOR_MUSL)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_BACKWARD=1)
    if(NOT WIN32 AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        if(NOT ICHOR_COMPILER_ID STREQUAL "gnu")
            pkg_check_modules(LIBUNWIND libunwind-generic)
            if(LIBUNWIND_FOUND)
                target_include_directories(ichor PRIVATE ${LIBUNWIND_INCLUDE_DIR})
                target_link_directories(ichor PRIVATE ${LIBUNWIND_LIBRARY_DIRS})
                target_link_libraries(ichor PUBLIC ${LIBUNWIND_LIBRARIES})
                target_compile_definitions(ichor PUBLIC BACKWARD_HAS_LIBUNWIND=1)
            endif()
        endif()
        pkg_check_modules(LIBDW libdw)
        if(LIBDW_FOUND)
            target_include_directories(ichor PRIVATE ${LIBDW_INCLUDE_DIR})
            target_link_directories(ichor PRIVATE ${LIBDW_LIBRARY_DIRS})
            target_link_libraries(ichor PUBLIC ${LIBDW_LIBRARIES})
            target_compile_definitions(ichor PUBLIC BACKWARD_HAS_DW=1)
        endif()
    endif()
endif()

if(hasParent)
    set(ICHOR_TOP_DIR "${ICHOR_TOP_DIR}" PARENT_SCOPE)
    set(ICHOR_EXTERNAL_DIR "${ICHOR_EXTERNAL_DIR}" PARENT_SCOPE)
endif()
configure_package_config_file(
        IchorConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/IchorConfig.cmake
        INSTALL_DESTINATION lib/cmake/ichor
        PATH_VARS
        )
write_basic_package_version_file(
        IchorConfigVersion.cmake
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY SameMinorVersion
        )

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/ichor" # source directory
        DESTINATION "include" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
        )

install(DIRECTORY "${CMAKE_SOURCE_DIR}/external/fmt/include/fmt" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
        )

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/sole" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/base64" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/ankerl" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/ctre" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/tl" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/backward" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
        )

if(ICHOR_USE_SPDLOG)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/external/spdlog/include/spdlog" # source directory
            DESTINATION "include/ichor/external" # target directory
            FILES_MATCHING # install only matched files
            PATTERN "*.h" # select header files
            )
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/external/glaze/include/glaze" # source directory
        DESTINATION "include/ichor/external" # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
        )

install(TARGETS ichor ${MIMALLOC_TARGET}
        EXPORT IchorTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

if(ICHOR_USE_MIMALLOC AND NOT ICHOR_USE_SYSTEM_MIMALLOC)
    set(mi_version "2.0")
    set(mi_install_incdir     "${CMAKE_INSTALL_INCLUDEDIR}/mimalloc-${mi_version}")
    install(FILES external/mimalloc/include/mimalloc.h DESTINATION ${mi_install_incdir})
    install(FILES external/mimalloc/include/mimalloc-override.h DESTINATION ${mi_install_incdir})
    install(FILES external/mimalloc/include/mimalloc-new-delete.h DESTINATION ${mi_install_incdir})
endif()

install(EXPORT IchorTargets
        FILE IchorTargets.cmake
        NAMESPACE Ichor::
        DESTINATION lib/cmake/ichor
        )
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/IchorConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/IchorConfigVersion.cmake"
        DESTINATION lib/cmake/ichor
        )



if(ICHOR_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(ICHOR_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
if(ICHOR_BUILD_TESTING)
    add_subdirectory(${ICHOR_EXTERNAL_DIR}/Catch2)
    #enable string_view support in catch2
    target_compile_features(Catch2 PRIVATE cxx_std_20)
    target_compile_features(Catch2WithMain PRIVATE cxx_std_20)
    list(APPEND CMAKE_MODULE_PATH "${ICHOR_EXTERNAL_DIR}/Catch2/extras")

    enable_testing()
    add_subdirectory(test)

    if(ICHOR_USE_SANITIZERS)
        if(WIN32)
            target_compile_options(Catch2 PUBLIC /fsanitize=address /Zi)
            target_compile_options(Catch2WithMain PUBLIC /fsanitize=address /Zi)
        else()
            target_compile_options(Catch2 PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_compile_options(Catch2WithMain PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_compile_definitions(Catch2 PUBLIC __SANITIZE_ADDRESS__ _GLIBCXX_SANITIZE_VECTOR)
            target_compile_definitions(Catch2WithMain PUBLIC __SANITIZE_ADDRESS__ _GLIBCXX_SANITIZE_VECTOR)

            if(ICHOR_DISABLE_RTTI)
                # vptr UB checks not compatible with no-rtti
                target_compile_options(Catch2 PUBLIC -fno-sanitize=vptr)
                target_compile_options(Catch2WithMain PUBLIC -fno-sanitize=vptr)
            endif()
        endif()
    endif()

    if(ICHOR_MUSL)
        target_compile_options(Catch2 PUBLIC -static)
        target_compile_options(Catch2WithMain PUBLIC -static)
        target_link_options(Catch2 PUBLIC -static-libgcc -static-libstdc++ -static)
        target_link_options(Catch2WithMain PUBLIC -static-libgcc -static-libstdc++ -static)
    endif()

    if(NOT WIN32 AND NOT ICHOR_USE_SANITIZERS AND NOT ICHOR_USE_THREAD_SANITIZER)
        target_compile_options(Catch2 PUBLIC -fpie)
        target_compile_options(Catch2WithMain PUBLIC -fpie)
        target_link_options(Catch2 PUBLIC -pie)
        target_link_options(Catch2WithMain PUBLIC -pie)
    endif()

    # TODO Would love to refactor this to a function, but functions don't seem to be able to reach variables not yet defined when the function is defined.
    if(NOT WIN32 AND ICHOR_USE_HARDENING AND CMAKE_BUILD_TYPE MATCHES Debug)
        # target_compile_definitions annoyingly adds "-D" in front of "-U" and target_compile_options goes after target_compile_definitions. Argh.
        target_compile_options(Catch2 PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
        target_compile_options(Catch2WithMain PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
        # Enable runtime iterator debug checks (like checking two different iterators from different container
        target_compile_definitions(Catch2 PRIVATE _GLIBCXX_ASSERTIONS _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _ITERATOR_DEBUG_LEVEL=2 _LIBCPP_DEBUG=1 _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
        target_compile_definitions(Catch2WithMain PRIVATE _GLIBCXX_ASSERTIONS _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _ITERATOR_DEBUG_LEVEL=2 _LIBCPP_DEBUG=1 _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
    elseif(NOT WIN32 AND ICHOR_USE_HARDENING)
        # target_compile_definitions annoyingly adds "-D" in front of "-U" and target_compile_options goes after target_compile_definitions. Argh.
        target_compile_options(Catch2 PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
        target_compile_options(Catch2WithMain PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3)
        target_compile_definitions(Catch2 PRIVATE _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
        target_compile_definitions(Catch2WithMain PRIVATE _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST)
    endif()

    if(ICHOR_USE_LIBCPP)
        target_compile_options(Catch2 PUBLIC -stdlib=libc++)
        target_link_options(Catch2 PUBLIC -stdlib=libc++)
    endif()
endif()
