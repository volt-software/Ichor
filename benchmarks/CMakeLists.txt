file(GLOB_RECURSE PROJECT_BENCHMARK_SOURCES ${ICHOR_TOP_DIR}/benchmarks/*.cpp)

foreach(filename ${PROJECT_BENCHMARK_SOURCES})
    get_filename_component(benchname ${filename} NAME_WE)
    add_executable(${benchname} ${filename})
    target_link_libraries(${benchname} ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(${benchname} ichor)
endforeach()
