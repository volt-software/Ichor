include(CTest)
include(Catch)

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
    catch_discover_tests(${testname})
endforeach()
