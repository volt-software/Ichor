#!/bin/bash

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

POSITIONAL_ARGS=()
ASAN=1
TSAN=0
GCC=0
BUILDTYPE="Debug"
RUN_EXAMPLES=0
IODEBUG=0
DEBUG=0

while [[ $# -gt 0 ]]; do
  case $1 in
    --gcc)
      GCC=1
      shift # past value
      ;;
    --tsan)
      ASAN=0
      TSAN=1
      shift # past value
      ;;
    --nosan)
      ASAN=0
      TSAN=0
      shift # past value
      ;;
    --release)
      ASAN=0
      TSAN=0
      BUILDTYPE="Release"
      shift # past value
      ;;
    --iodebug)
      IODEBUG=1
      shift # past value
      ;;
    --debug)
      DEBUG=1
      shift # past value
      ;;
    --run-examples)
      RUN_EXAMPLES=1
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

rm -rf ./*
rm -rf ../bin/*

if [[ $GCC -eq 1 ]]; then
  CC=gcc CXX=g++ cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DICHOR_REMOVE_SOURCE_NAMES=0 -DICHOR_ENABLE_INTERNAL_DEBUGGING=${DEBUG} -DICHOR_ENABLE_INTERNAL_IO_DEBUGGING=${IODEBUG} -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_BACKWARD=0 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_SANITIZERS=${ASAN} -DICHOR_USE_THREAD_SANITIZER=${TSAN} -DICHOR_USE_MOLD=1 -GNinja ..
else
  CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DICHOR_REMOVE_SOURCE_NAMES=0 -DICHOR_ENABLE_INTERNAL_DEBUGGING=${DEBUG} -DICHOR_ENABLE_INTERNAL_IO_DEBUGGING=${IODEBUG} -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_BACKWARD=0 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_LIBCPP=0 -DICHOR_USE_SANITIZERS=${ASAN} -DICHOR_USE_THREAD_SANITIZER=${TSAN} -DICHOR_USE_MOLD=1 -DICHOR_USE_SPDLOG=0 -GNinja ..
fi

ninja && ninja test

if [[ $RUN_EXAMPLES -eq 1 ]]; then
    ../bin/ichor_http_example || exit 1
    ../bin/ichor_multithreaded_example || exit 1
    ../bin/ichor_optional_dependency_example || exit 1
    ../bin/ichor_event_statistics_example || exit 1
    ../bin/ichor_serializer_example || exit 1
    ../bin/ichor_tcp_example || exit 1
    ../bin/ichor_timer_example || exit 1
    ../bin/ichor_factory_example || exit 1
    ../bin/ichor_introspection_example || exit 1
    ../bin/ichor_websocket_example || exit 1
    ../bin/ichor_websocket_example -t4 || exit 1
    ../bin/ichor_yielding_timer_example || exit 1
    ../bin/ichor_etcd_example || exit 1
fi

if command -v checksec --help &> /dev/null
then
    checksec --file=../bin/ichor_tcp_example
fi
