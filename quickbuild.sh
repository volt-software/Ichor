#!/bin/bash

curr=`basename $(pwd)`

if [[ "$curr" != "build" ]]; then
  echo "Not in build directory"
  exit 1;
fi

. ../build_common.sh

trap cleanup SIGINT SIGTERM

POSITIONAL_ARGS=()
ASAN=1
TSAN=0
BUILDTYPE="Debug"
RUN_EXAMPLES=0
IODEBUG=0
DEBUG=0
CCOMP=clang-19
CXXCOMP=clang++-19
BOOST=0
SPDLOG=0

while [[ $# -gt 0 ]]; do
  case $1 in
    --gcc)
      CCOMP=gcc
      CXXCOMP=g++
      shift # past value
      ;;
    --oldgcc)
      CCOMP=gcc-12
      CXXCOMP=g++-12
      shift # past value
      ;;
    --oldclang)
      CCOMP=clang-15
      CXXCOMP=clang++-15
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
    --boost)
      BOOST=1
      shift # past value
      ;;
    --spdlog)
      SPDLOG=1
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

rm -rf ./*
rm -rf ../bin/*

CC=${CCOMP} CXX=${CXXCOMP} cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DICHOR_REMOVE_SOURCE_NAMES=0 -DICHOR_ENABLE_INTERNAL_DEBUGGING=${DEBUG} -DICHOR_ENABLE_INTERNAL_IO_DEBUGGING=${IODEBUG} -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_BACKWARD=0 -DICHOR_USE_BOOST_BEAST=${BOOST} -DICHOR_USE_HIREDIS=1 -DICHOR_USE_LIBCPP=0 -DICHOR_USE_SANITIZERS=${ASAN} -DICHOR_USE_THREAD_SANITIZER=${TSAN} -DICHOR_USE_MOLD=1 -DICHOR_USE_SDEVENT=1 -DICHOR_USE_SPDLOG=${SPDLOG} -GNinja .. || exit 1

ninja || exit 1
ninja test || exit 1

if [[ $RUN_EXAMPLES -eq 1 ]]; then
  run_examples $BOOST 1
fi

if command -v checksec --help &> /dev/null
then
  checksec --file=../bin/ichor_tcp_example
fi
