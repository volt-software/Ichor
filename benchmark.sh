#!/bin/bash

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

POSITIONAL_ARGS=()
REBUILD=1

while [[ $# -gt 0 ]]; do
  case $1 in
    --norebuild)
      REBUILD=0
      shift # past value
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

curr=`basename $(pwd)`

if [[ "$curr" != "build" ]]; then
  echo "Not in build directory"
  exit 1;
fi

run_benchmarks ()
{
  coroutine="$1"/ichor_coroutine_benchmark
  event="$1"/ichor_event_benchmark
  serializer="$1"/ichor_serializer_benchmark
  start="$1"/ichor_start_benchmark
  start_stop="$1"/ichor_start_stop_benchmark
  utils="$1"/ichor_utils_benchmark
  eval $coroutine || exit 1
  eval $event || exit 1
  eval $event -u || exit 1
  eval $serializer || exit 1
  eval $start -a || exit 1
  eval $start -c || exit 1
  eval $start_stop || exit 1
  eval $utils -r || exit 1
  eval $utils -a || exit 1
}

if [ $REBUILD -eq 1 ]; then
  rm -rf ../std_std ../std_mimalloc ../std_no_hardening ../clang_libcpp_mimalloc
  mkdir -p ../std_std ../std_mimalloc ../std_no_hardening ../clang_libcpp_mimalloc

  rm -rf ./* ../bin/*
  CC=gcc CXX=g++ cmake -GNinja -DICHOR_USE_MIMALLOC=OFF -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_MOLD=ON -DICHOR_BUILD_EXAMPLES=OFF -DICHOR_BUILD_TESTING=OFF -DICHOR_USE_BOOST_BEAST=ON .. || exit 1
  ninja || exit 1
  cp ../bin/*benchmark ../std_std || exit 1

  rm -rf ./* ../bin/*
  CC=gcc CXX=g++ cmake -GNinja -DICHOR_USE_MIMALLOC=ON -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_MOLD=ON -DICHOR_BUILD_EXAMPLES=OFF -DICHOR_BUILD_TESTING=OFF -DICHOR_USE_BOOST_BEAST=ON .. || exit 1
  ninja || exit 1
  cp ../bin/*benchmark ../std_mimalloc || exit 1

  rm -rf ./* ../bin/*
  CC=clang CXX=clang++ cmake -GNinja -DICHOR_USE_MIMALLOC=ON -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_MOLD=ON -DICHOR_BUILD_EXAMPLES=OFF -DICHOR_BUILD_TESTING=OFF -DICHOR_USE_BOOST_BEAST=ON .. || exit 1
  ninja || exit 1
  cp ../bin/*benchmark ../clang_libcpp_mimalloc || exit 1

  rm -rf ./* ../bin/*
  CC=gcc CXX=g++ cmake -GNinja -DICHOR_USE_MIMALLOC=ON -DICHOR_USE_HARDENING=OFF -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_MOLD=ON -DICHOR_BUILD_EXAMPLES=OFF -DICHOR_BUILD_TESTING=OFF -DICHOR_USE_BOOST_BEAST=ON .. || exit 1
  ninja || exit 1
  cp ../bin/*benchmark ../std_no_hardening || exit 1
fi

run_benchmarks ../std_std
run_benchmarks ../std_mimalloc
run_benchmarks ../std_no_hardening
run_benchmarks ../clang_libcpp_mimalloc
