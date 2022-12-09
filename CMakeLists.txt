cmake_minimum_required(VERSION 3.12) #might need 3.13 due to https://gitlab.kitware.com/cmake/cmake/-/issues/14444
cmake_policy(SET CMP0048 NEW)
project(ichor VERSION 0.1.0 DESCRIPTION "C++20 dependency injection framework" HOMEPAGE_URL https://github.com/volt-software/Ichor LANGUAGES CXX)
if(NOT WIN32)
    # we set this flag manually later on to enable the experimental C++20 stuff
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE CXX STANDARD REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.19.0")
    cmake_policy(SET CMP0110 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
cmake_policy(SET CMP0077 NEW)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)


if(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

get_directory_property(hasParent PARENT_DIRECTORY)
if(NOT hasParent)
    set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
    set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
endif()


if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL "11.2")
    message(FATAL_ERROR "Gcc 11.2 and below not supported. See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95137")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14")
    message(FATAL_ERROR "Clang below version 14 does not implement enough of C++20 to use Ichor.")
endif()

set(ICHOR_TOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ICHOR_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/")

option(ICHOR_BUILD_EXAMPLES "Build examples" ON)
option(ICHOR_BUILD_BENCHMARKS "Build benchmarks" ON)
option(ICHOR_BUILD_TESTING "Build tests" ON)
option(ICHOR_ENABLE_INTERNAL_DEBUGGING "Add verbose logging of Ichor internals" OFF)
option(ICHOR_BUILD_COVERAGE "Build ichor with coverage" OFF)
option(ICHOR_USE_SPDLOG "Use spdlog as framework logging implementation" OFF)
option(ICHOR_USE_ETCD "Add various dependencies to enable pubsub bundle to be built" OFF)
option(ICHOR_USE_BOOST_BEAST "Add boost asio and boost BEAST as dependencies" OFF)
option(ICHOR_USE_SANITIZERS "Enable sanitizers, catching potential errors but slowing down compilation and execution speed" ON)
cmake_dependent_option(ICHOR_USE_THREAD_SANITIZER "Enable thread sanitizer, catching potential threading errors but slowing down compilation and execution speed. Cannot be combined with ICHOR_USE_SANITIZERS" OFF "NOT WIN32" OFF)
option(ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING "Enable an ugly hack on gcc to enable debugging the point where exceptions are thrown. Useful for debugging boost asio/beast backtraces." OFF)
option(ICHOR_REMOVE_SOURCE_NAMES "Remove compiling source file names and line numbers when logging." OFF)
cmake_dependent_option(ICHOR_USE_MOLD "Use mold when linking, recommended to use with gcc 12+ or clang" OFF "NOT WIN32" OFF)
cmake_dependent_option(ICHOR_USE_SDEVENT "Add sd-event based queue/integration" OFF "NOT WIN32" OFF)
option(ICHOR_USE_ABSEIL "Use abseil provided classes where applicable" OFF)
option(ICHOR_DISABLE_RTTI "Disable RTTI. Reduces memory usage, disables dynamic_cast<>()" ON)
option(ICHOR_USE_HARDENING "Uses compiler-specific flags which add stack protection and similar features, as well as adding safety checks in Ichor itself." ON)
cmake_dependent_option(ICHOR_USE_MIMALLOC "Use mimalloc for significant performance improvements" ON "NOT ICHOR_USE_SANITIZERS" OFF)
cmake_dependent_option(ICHOR_USE_SYSTEM_MIMALLOC "Use system or vendored mimalloc" OFF "NOT ICHOR_USE_SANITIZERS" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(ICHOR_USE_BACKWARD "Use backward-cpp to print stacktraces on crashes or when the user wants to. Useful for debugging." ON)
else()
    option(ICHOR_USE_BACKWARD "Use backward-cpp to print stacktraces on crashes or when the user wants to. Useful for debugging." OFF)
endif()

set(BUILD_TESTING OFF) #disable Catch 2 testing

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # clang tidy requires the same compiler arguments as clang
    # some gcc arguments cause it to error out
    option(ICHOR_RUN_CLANG_TIDY "Runs clang-tidy every compile" OFF)
endif()

set(ICHOR_ARCH_OPTIMIZATION OFF CACHE STRING "Tell compiler to optimize for target")
set_property(CACHE ICHOR_ARCH_OPTIMIZATION PROPERTY STRINGS OFF NATIVE X86_64 X86_64_SSE4 X86_64_AVX2 X86_64_AVX512 MODERN_ARM_GENERIC)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND ICHOR_RUN_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*,-llvmlibc-*,-readability-function-cognitive-complexity,-altera-*,-modernize-use-trailing-return-type,-concurrency-mt-unsafe,-fuchsia-default-arguments-calls,-android-*,-readability-identifier-length,-clang-analyzer-optin.cplusplus.UninitializedObject")
endif()

set(FMT_SOURCES ${ICHOR_EXTERNAL_DIR}/fmt/src/format.cc ${ICHOR_EXTERNAL_DIR}/fmt/src/os.cc)
file(GLOB_RECURSE ICHOR_FRAMEWORK_SOURCES ${ICHOR_TOP_DIR}/src/ichor/*.cpp)
file(GLOB_RECURSE ICHOR_OPTIONAL_ETCD_SOURCES ${ICHOR_TOP_DIR}/src/services/etcd/*.cpp ${ICHOR_TOP_DIR}/src/services/etcd/*.cc)
file(GLOB_RECURSE ICHOR_LOGGING_SOURCES ${ICHOR_TOP_DIR}/src/services/logging/*.cpp)
file(GLOB_RECURSE ICHOR_NETWORK_SOURCES ${ICHOR_TOP_DIR}/src/services/network/*.cpp)
file(GLOB_RECURSE ICHOR_METRICS_SOURCES ${ICHOR_TOP_DIR}/src/services/metrics/*.cpp)
file(GLOB_RECURSE ICHOR_TIMER_SOURCES ${ICHOR_TOP_DIR}/src/services/timer/*.cpp)

file(GLOB SPDLOG_SOURCES ${ICHOR_EXTERNAL_DIR}/spdlog/src/*.cpp)

add_library(ichor ${FMT_SOURCES} ${ICHOR_FRAMEWORK_SOURCES} ${ICHOR_LOGGING_SOURCES} ${ICHOR_NETWORK_SOURCES} ${ICHOR_METRICS_SOURCES} ${ICHOR_TIMER_SOURCES})

if(ICHOR_ENABLE_INTERNAL_DEBUGGING)
    target_compile_definitions(ichor PUBLIC ICHOR_ENABLE_INTERNAL_DEBUGGING)
endif()

if(ICHOR_USE_SPDLOG)
    target_compile_definitions(ichor PUBLIC SPDLOG_COMPILED_LIB SPDLOG_NO_EXCEPTIONS SPDLOG_FMT_EXTERNAL SPDLOG_DISABLE_DEFAULT_LOGGER SPDLOG_NO_ATOMIC_LEVELS ICHOR_USE_SPDLOG SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
endif()

if(ICHOR_USE_BOOST_BEAST)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_BOOST_JSON BOOST_JSON_STANDALONE)
    find_package(Boost 1.75.0 REQUIRED COMPONENTS json context)
    target_include_directories(ichor PUBLIC ${Boost_INCLUDE_DIR})
    target_link_directories(ichor PUBLIC ${Boost_LIBRARY_DIRS})
    target_link_libraries(ichor PUBLIC ${Boost_LIBRARIES})
endif()

if(ICHOR_REMOVE_SOURCE_NAMES)
    target_compile_definitions(ichor PUBLIC ICHOR_REMOVE_SOURCE_NAMES_FROM_LOGGING)
endif()

if(ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING)
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

    target_compile_definitions(ichor PUBLIC ICHOR_USE_ETCD)
endif()

if(ICHOR_USE_BOOST_BEAST)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_BOOST_BEAST)
endif()

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wno-unused-variable -Wno-long-long -Wno-unused-parameter -Wnull-dereference -pedantic -Wformat -Wformat-security -Wcast-align -Woverloaded-virtual ")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -ftemplate-backtrace-limit=0 ")
    if(ICHOR_USE_HARDENING AND (CMAKE_BUILD_TYPE MATCHES DEBUG OR CMAKE_BUILD_TYPE MATCHES RELWITHDEBINFO))
        # Enable runtime iterator debug checks (like checking two different iterators from different containers)
        target_compile_definitions(ichor PUBLIC _GLIBCXX_CONCEPT_CHECKS _GLIBCXX_ASSERTIONS _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _GLIBCXX_SANITIZE_VECTOR _ITERATOR_DEBUG_LEVEL=2)
    endif()
    target_compile_definitions(ichor PUBLIC _FILE_OFFSET_BITS=64 _TIME_BITS=64)

    # Don't want to enable this for external source code
    target_compile_options(ichor PUBLIC -Wconversion)
    # don't optimize away overflows
    target_compile_options(ichor PUBLIC -fwrapv)
else()
    #msvc sets some stupid defaults. We can do better.
    # d2SSAOptimizer-: treat arithmetic overflow as defined behaviour (like fwrapv)
    set(CMAKE_CXX_FLAGS "/nologo /DWIN32 /D_WINDOWS /EHsc /TP /W4 /await:strict /std:c++latest /permissive- /wd4100 /d2SSAOptimizer-")
    target_compile_definitions(ichor PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN )
endif()

# gcc added support for mold in version 12, but it throws catch off guard when using gcc, mold and sanitizers.
if(ICHOR_USE_MOLD AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold ")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wduplicated-cond -Wduplicated-branches -Wlogical-op ")
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
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX2")
        target_compile_options(ichor PUBLIC -march=x86-64-v3)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "X86_64_AVX512")
        target_compile_options(ichor PUBLIC -march=x86-64-v4)
    elseif(ICHOR_ARCH_OPTIMIZATION STREQUAL "MODERN_ARM_GENERIC")
        target_compile_options(ichor PUBLIC -march=armv8-a)
    endif()
endif()

if(ICHOR_DISABLE_RTTI)
    if(WIN32)
        target_compile_options(ichor PUBLIC /GR-)
    else()
        target_compile_options(ichor PUBLIC -fno-rtti)
    endif()
endif()

if(ICHOR_USE_SANITIZERS)
    if(WIN32)
        # MSVC does not support UBsan
        target_compile_options(ichor PUBLIC /fsanitize=address)
    else()
        target_compile_options(ichor PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ichor PUBLIC -fsanitize=address,undefined)

        # clang on OSX doesn't accept -no-pie for some reason
        if(NOT APPLE)
            target_link_options(ichor PUBLIC -no-pie)
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(ichor PUBLIC -no-pie)
        endif()
    endif()

    if(ICHOR_USE_BOOST_BEAST)
        target_compile_definitions(ichor PUBLIC BOOST_USE_ASAN BOOST_USE_UCONTEXT)
    endif()
endif()

if(ICHOR_USE_THREAD_SANITIZER)
    target_compile_options(ichor PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(ichor PUBLIC -fsanitize=thread)

    # clang on OSX doesn't accept -no-pie for some reason
    if(NOT APPLE)
        target_link_options(ichor PUBLIC -no-pie)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(ichor PUBLIC -no-pie)
    endif()
endif()

if(NOT DEFINED ICHOR_USE_SANITIZERS AND NOT DEFINED ICHOR_USE_THREAD_SANITIZER)
    # see https://github.com/google/sanitizers/issues/856
    target_compile_options(ichor PUBLIC -fPIE)
endif()

if(WIN32 AND ICHOR_USE_HARDENING)
    target_compile_options(ichor PUBLIC /GS /guard:cf)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_HARDENING)
elseif(ICHOR_USE_HARDENING)
    target_compile_options(ichor PUBLIC -fstack-protector-strong -fcf-protection)

    # stack clash protection not available on OSX
    if(NOT APPLE)
        target_compile_options(ichor PUBLIC -fstack-clash-protection)
    endif()

    target_compile_definitions(ichor PUBLIC ICHOR_USE_HARDENING)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ichor PRIVATE -fconcepts-diagnostics-depth=3)
    target_compile_options(ichor PUBLIC -fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ichor PUBLIC -fcoroutines-ts)
endif()

if(NOT WIN32 AND NOT ICHOR_BUILD_COVERAGE)
    # gcc uses gdwarf-4 by default, which messes up using the coz profiler, add "-gdwarf-3" if using coz
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-ggdb -Og")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
endif()

if(ICHOR_BUILD_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ichor PUBLIC -O0 -g --coverage)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(ichor PUBLIC --coverage)
    else()
        target_link_libraries(ichor PUBLIC --coverage)
    endif()
endif()

# Gcc 12.1 and 12.2 have bugs that prevent compilation with Werror at -O2 and higher:
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107138
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
if((NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12") AND NOT WIN32)
    target_compile_options(ichor PRIVATE "-Werror") #prevent externally compiled sources to error out on warnings
endif()

# By default, we build a bundled mimalloc and statically-link it to
# Ichor. If you want to dynamically link to the system's
# libmimalloc.so, pass -DICHOR_USE_SYSTEM_MIMALLOC=ON.
if(ICHOR_USE_MIMALLOC)
    if(ICHOR_USE_SYSTEM_MIMALLOC)
        find_package(mimalloc REQUIRED)
        target_link_libraries(ichor PRIVATE mimalloc)
        target_compile_definitions(ichor PRIVATE ICHOR_USE_SYSTEM_MIMALLOC)
    else()
        function(ichor_add_mimalloc)
            set(MI_BUILD_STATIC ON)
            set(MI_BUILD_SHARED OFF)
            set(MI_BUILD_OBJECT OFF)
            set(MI_BUILD_TESTS OFF)
            set(MI_USE_CXX ON)
            option(MI_BUILD_TESTS "Build test executables" OFF)

            add_subdirectory(external/mimalloc EXCLUDE_FROM_ALL)
            target_compile_definitions(mimalloc-static PRIVATE MI_USE_ENVIRON=0)
            target_link_libraries(ichor PUBLIC mimalloc-static)
        endfunction()

        ichor_add_mimalloc()
        set(MIMALLOC_TARGET mimalloc-static)
    endif()
endif()

if(ICHOR_USE_ABSEIL)
    find_package(absl REQUIRED)
    target_link_libraries(ichor PUBLIC absl::flat_hash_map absl::flat_hash_set absl::btree absl::hash)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_ABSEIL)
endif()

if(ICHOR_USE_SPDLOG)
    target_sources(ichor PRIVATE ${SPDLOG_SOURCES})
endif()

if(ICHOR_USE_ETCD)
    target_sources(ichor PRIVATE ${ICHOR_OPTIONAL_ETCD_SOURCES})
    target_compile_definitions(ichor PUBLIC GOOGLE_PROTOBUF_NO_RTTI)
endif()

if(ICHOR_USE_SDEVENT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Systemd IMPORTED_TARGET GLOBAL libsystemd)
    if(NOT TARGET PkgConfig::Systemd)
        message(FATAL_ERROR "libsystemd was not found")
    endif()
    target_link_libraries(ichor PUBLIC PkgConfig::Systemd)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_SDEVENT)
endif()

if(ICHOR_USE_BOOST_BEAST)
    find_package(Boost 1.70.0 REQUIRED COMPONENTS context coroutine)
    target_include_directories(ichor PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_directories(ichor PUBLIC ${Boost_LIBRARY_DIRS})
    target_link_libraries(ichor PUBLIC ${Boost_LIBRARIES})
    target_compile_definitions(ichor PUBLIC BOOST_ASIO_NO_DEPRECATED)

    if(Boost_VERSION_STRING VERSION_LESS "1.81.0")
        target_compile_definitions(ichor PUBLIC BOOST_BEAST_USE_STD_STRING_VIEW)
    endif()


    if(ICHOR_USE_HARDENING OR ICHOR_ENABLE_INTERNAL_DEBUGGING)
        target_compile_definitions(ichor PUBLIC BOOST_ASIO_ENABLE_BUFFER_DEBUGGING)
    endif()

endif()

target_link_libraries(ichor PUBLIC ${CMAKE_THREAD_LIBS_INIT})
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(ichor PUBLIC -ldl -lrt)
endif()

target_include_directories(ichor PUBLIC
        $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/fmt/include>
        $<INSTALL_INTERFACE:include/ichor/external>)

target_include_directories(ichor PUBLIC
        $<BUILD_INTERFACE:${ICHOR_TOP_DIR}/include>
        $<INSTALL_INTERFACE:include>)

if(ICHOR_USE_SPDLOG)
    target_include_directories(ichor PUBLIC
            $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/spdlog/include>
            $<INSTALL_INTERFACE:include/ichor/external>)
endif()

target_include_directories(ichor PUBLIC
        $<BUILD_INTERFACE:${ICHOR_EXTERNAL_DIR}/rapidjson/include>
        $<INSTALL_INTERFACE:include/ichor/external>)

if(ICHOR_USE_ETCD)
    #necessary to get generator protobuf/grpc code to compile
    target_include_directories(ichor PUBLIC "${ICHOR_TOP_DIR}/include/ichor/services/etcd")
endif()

# Detection for backward-cpp, works without external libraries on windows and apple
# On linux, it still works without these libraries (on gcc at least), but provides less functionality.
if(ICHOR_USE_BACKWARD)
    target_compile_definitions(ichor PUBLIC ICHOR_USE_BACKWARD=1)
    if(NOT WIN32 AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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

install(DIRECTORY "${CMAKE_SOURCE_DIR}/external/rapidjson/include/rapidjson" # source directory
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
    list(APPEND CMAKE_MODULE_PATH "${ICHOR_EXTERNAL_DIR}/Catch2/extras")

    enable_testing()
    add_subdirectory(test)

    if(ICHOR_USE_SANITIZERS)
        if(WIN32)
            target_compile_options(Catch2 PUBLIC /fsanitize=address)
        endif()
    endif()

endif()
