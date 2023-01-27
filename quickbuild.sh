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
MIMALLOC=0
BUILDTYPE="Debug"

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
      MIMALLOC=1
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
if [[ $GCC -eq 1 ]]; then
  CC=/opt/gcc/12/bin/gcc CXX=/opt/gcc/12/bin/g++ cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_MIMALLOC=${MIMALLOC} -DICHOR_USE_BACKWARD=0 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_SANITIZERS=${ASAN} -DICHOR_USE_THREAD_SANITIZER=${TSAN} -GNinja ..
else
  CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=${BUILDTYPE} -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_MIMALLOC=${MIMALLOC} -DICHOR_USE_BACKWARD=0 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_SANITIZERS=${ASAN} -DICHOR_USE_THREAD_SANITIZER=${TSAN} -GNinja ..
fi

ninja
