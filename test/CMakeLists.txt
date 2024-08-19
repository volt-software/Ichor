include(CTest)
include(Catch)

option(ICHOR_SKIP_EXTERNAL_TESTS "When running tests in containers, etcd and redis aren't running. This option skips those." OFF)

file(GLOB_RECURSE PROJECT_TEST_SOURCES ${ICHOR_TOP_DIR}/test/*.cpp)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized ") # Catch2 has something that triggers gcc.
endif()

foreach(filename ${PROJECT_TEST_SOURCES})
    get_filename_component(testname ${filename} NAME_WE)

    add_executable(${testname} ${filename})
    target_link_libraries(${testname} ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(${testname} ichor)
    target_link_libraries(${testname} Catch2::Catch2WithMain)
    target_compile_definitions(${testname} PUBLIC CATCH_CONFIG_FAST_COMPILE)

    # We want to build them, but not run them.
    if(ICHOR_SKIP_EXTERNAL_TESTS AND (${testname} STREQUAL "EtcdTests" OR ${testname} STREQUAL "RedisTests"))
        continue()
    endif()
    if(ICHOR_SKIP_EXTERNAL_TESTS)
        target_compile_definitions(${testname} PUBLIC ICHOR_SKIP_EXTERNAL_TESTS)
    endif()
    if(${testname} STREQUAL "AsyncFileIOTests" AND NOT WIN32 AND NOT APPLE AND NOT (ICHOR_SKIP_EXTERNAL_TESTS AND ICHOR_AARCH64))
        add_executable(${testname}_uring ${filename})
        target_link_libraries(${testname}_uring ${CMAKE_THREAD_LIBS_INIT})
        target_link_libraries(${testname}_uring ichor)
        target_link_libraries(${testname}_uring Catch2::Catch2WithMain)
        target_compile_definitions(${testname}_uring PUBLIC CATCH_CONFIG_FAST_COMPILE TEST_URING)
        catch_discover_tests(${testname}_uring)
    endif()

    catch_discover_tests(${testname})
endforeach()
