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
    ../bin/ichor_http_example || exit 1
    ../bin/ichor_websocket_example || exit 1
    ../bin/ichor_websocket_example -t4 || exit 1
    ../bin/ichor_etcd_example || exit 1
  fi
  if [[ URING -eq 1 ]]; then
    ../bin/ichor_tcp_example_uring || exit 1
    ../bin/ichor_timer_example_uring || exit 1
    ../bin/ichor_yielding_timer_example_uring || exit 1
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
