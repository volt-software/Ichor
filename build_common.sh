#!/bin/bash

cleanup ()
{
  kill -s SIGTERM $!
  exit 0
}

run_examples ()
{
  BOOST=${1}
  URING=${2}
  SDEVENT=${3}
  ../bin/ichor_multithreaded_example || exit 1
  ../bin/ichor_optional_dependency_example || exit 1
  ../bin/ichor_event_statistics_example || exit 1
  ../bin/ichor_serializer_example || exit 1
  ../bin/ichor_timer_example || exit 1
  ../bin/ichor_factory_example || exit 1
  ../bin/ichor_introspection_example || exit 1
  ../bin/ichor_yielding_timer_example || exit 1
  if [[ $BOOST -eq 1 ]]; then
    ../bin/ichor_tcp_example || exit 1
    ../bin/ichor_http_example_boost || exit 1
    ../bin/ichor_websocket_example || exit 1
    ../bin/ichor_etcd_example_boost || exit 1
  fi
  if [[ URING -eq 1 ]]; then
    ../bin/ichor_multithreaded_example_uring || exit 1
    ../bin/ichor_tcp_example_uring || exit 1
    ../bin/ichor_timer_example_uring || exit 1
    ../bin/ichor_yielding_timer_example_uring || exit 1
    ../bin/ichor_http_example_uring || exit 1
    ../bin/ichor_etcd_example_uring || exit 1
  fi
}
run_benchmarks ()
{
  ../bin/ichor_coroutine_benchmark || exit 1
  ../bin/ichor_event_benchmark || exit 1
  ../bin/ichor_event_benchmark -u || exit 1
  ../bin/ichor_serializer_benchmark || exit 1
  ../bin/ichor_start_benchmark || exit 1
  ../bin/ichor_start_stop_benchmark || exit 1
  ../bin/ichor_utils_benchmark -r || exit 1
}
run_fast_benchmarks ()
{
  ../bin/ichor_coroutine_benchmark || exit 1
  ../bin/ichor_event_benchmark || exit 1
  ../bin/ichor_event_benchmark -u || exit 1
  ../bin/ichor_serializer_benchmark || exit 1
  ../bin/ichor_utils_benchmark -r || exit 1
}

# What a clang-tidy finding looks like in a build log (ICHOR_RUN_CLANG_TIDY, .clang-tidy makes every finding an error)
CLANG_TIDY_FINDING_RE=': error: .* \[[a-z0-9.-]+,-warnings-as-errors\]$'

# ninja for a build with ICHOR_RUN_CLANG_TIDY on. -k 0 keeps building past a failed edge, so one run shows every
# clang-tidy finding rather than only those of the first sources to fail. A finding in a header fails every source
# that includes it, so on failure each distinct finding is listed once more at the end.
ninja_clang_tidy ()
{
  # In the build directory, so that the next build's cleanup removes it if this one is interrupted
  local log=clang-tidy-build.log
  ninja -k 0 2>&1 | tee "$log"
  local rc=${PIPESTATUS[0]}
  if [[ $rc -ne 0 ]]; then
    local findings
    findings=$(grep -E "$CLANG_TIDY_FINDING_RE" "$log" | sort -u)
    if [[ -n "$findings" ]]; then
      echo "-- $(wc -l <<< "$findings") distinct clang-tidy finding(s):"
      echo "$findings"
    fi
  fi
  rm -f "$log"
  return $rc
}
