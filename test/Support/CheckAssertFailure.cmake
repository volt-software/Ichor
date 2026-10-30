# Runs AssertFailure in the given MODE and checks that it terminated and printed something matching EXPECTED.
# Usage: cmake -DEXE=<AssertFailure> -DMODE=<always|hardened> -DEXPECTED=<regex> -P CheckAssertFailure.cmake
execute_process(COMMAND "${EXE}" "${MODE}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE res)
set(output "${out}${err}")
message("${output}")
if(NOT output MATCHES "${EXPECTED}")
    message(FATAL_ERROR "AssertFailure ${MODE}: output doesn't match \"${EXPECTED}\"")
endif()
# without hardening ICHOR_ASSERT isn't checked, and AssertFailure says so and exits normally
if(res EQUAL 0 AND NOT output MATCHES "isn't checked without hardening")
    message(FATAL_ERROR "AssertFailure ${MODE}: the failing assertion didn't terminate the process")
endif()
