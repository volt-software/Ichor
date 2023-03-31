#!/bin/bash

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

ccompilers=("clang-14" "clang-16" "gcc" "gcc-12")
cppcompilers=("clang++-14" "clang++-16" "g++" "g++-12")

if [[ "$1" == "--dev" ]]; then
  ccompilers=("clang" "gcc")
  cppcompilers=("clang++" "g++")
fi

if [[ "$1" == "--gcc" ]]; then
  ccompilers=("gcc")
  cppcompilers=("g++")
fi

curr=`basename $(pwd)`

if [[ "$curr" != "build" ]]; then
  echo "Not in build directory"
  exit 1;
fi

run_examples ()
{
  ../bin/ichor_http_example || exit 1
  ../bin/ichor_multithreaded_example || exit 1
  ../bin/ichor_optional_dependency_example || exit 1
  ../bin/ichor_event_statistics_example || exit 1
  ../bin/ichor_serializer_example || exit 1
  ../bin/ichor_tcp_example || exit 1
  ../bin/ichor_timer_example || exit 1
  ../bin/ichor_tracker_example || exit 1
  ../bin/ichor_websocket_example || exit 1
  ../bin/ichor_websocket_example -t4 || exit 1
  ../bin/ichor_yielding_timer_example || exit 1
}
run_benchmarks ()
{
  ../bin/ichor_coroutine_benchmark || exit 1
  ../bin/ichor_event_benchmark || exit 1
  ../bin/ichor_serializer_benchmark || exit 1
  ../bin/ichor_start_benchmark || exit 1
  ../bin/ichor_start_stop_benchmark || exit 1
}

for i in ${!ccompilers[@]}; do
  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=ON -DICHOR_USE_ABSEIL=ON -DICHOR_ENABLE_INTERNAL_DEBUGGING=OFF -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_HIREDIS=ON .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples

  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=ON -DICHOR_USE_ABSEIL=OFF -DICHOR_ENABLE_INTERNAL_DEBUGGING=ON -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_SPDLOG=ON -DICHOR_USE_HIREDIS=ON .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples

  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_ABSEIL=ON -DICHOR_ENABLE_INTERNAL_DEBUGGING=OFF -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_HIREDIS=ON .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples
  run_benchmarks
done
