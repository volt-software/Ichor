include(ParseAndAddCatchTests)

file(GLOB_RECURSE PROJECT_TEST_SOURCES ${ICHOR_TOP_DIR}/test/*.cpp)

foreach(filename ${PROJECT_TEST_SOURCES})
    get_filename_component(testname ${filename} NAME_WE)
    add_executable(${testname} ${filename})
#    add_test(NAME ${testname} COMMAND ${testname})
    target_link_libraries(${testname} ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(${testname} ichor)
    target_link_libraries(${testname} Catch2::Catch2WithMain)

    ParseAndAddCatchTests(${testname})
endforeach()