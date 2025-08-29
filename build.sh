#!/bin/bash

curr=`basename $(pwd)`

if [[ "$curr" != "build" ]]; then
  echo "Not in build directory"
  exit 1;
fi

. ../build_common.sh

trap cleanup SIGINT SIGTERM

POSITIONAL_ARGS=()
DOCKER=0
DOCKERTSAN=0
DEV=0
GCC=0

while [[ $# -gt 0 ]]; do
  case $1 in
    --docker)
      DOCKER=1
      shift # past value
      ;;
    --docker-tsan)
      DOCKERTSAN=1
      shift # past value
      ;;
    --dev)
      DEV=1
      shift # past value
      ;;
    --gcc)
      GCC=1
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

ccompilers=("clang-15" "clang-19" "gcc-12" "gcc")
cppcompilers=("clang++-15" "clang++-19" "g++-12" "g++")
ldlibpath=("" "" "" "/opt/gcc/14/lib64/")

if [[ $DEV -eq 1 ]]; then
  ccompilers=("clang-19" "gcc")
  cppcompilers=("clang++-19" "g++")
  ldlibpath=("" "/opt/gcc/14/lib64/")
fi

if [[ $GCC -eq 1 ]]; then
  ccompilers=("gcc")
  cppcompilers=("g++")
  ldlibpath=("/opt/gcc/14/lib64/")
fi

if [[ $DOCKER -eq 1 ]]; then
  chmod a+w ../build
  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile -t ichor --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor || exit 1
  run_examples 1 1 0
  run_fast_benchmarks

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-musl -t ichor-musl --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-musl || exit 1
  run_examples 1 1 0
  run_fast_benchmarks

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-asan -t ichor-asan --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-asan || exit 1
  run_examples 0 1 0
  run_benchmarks

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-asan-clang -t ichor-asan-clang --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-asan-clang || exit 1
#  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-asan-clang "ninja" || exit 1
  ../bin/TcpTests
  run_examples 0 1 0
  run_benchmarks

  rm -rf ./* ../bin/*
  docker --debug build -f ../Dockerfile-musl-aarch64 -t ichor-musl-aarch64 --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . --platform=linux/arm64 || exit 1
  docker --debug run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --security-opt seccomp=unconfined -it ichor-musl-aarch64 || exit 1
  # use the following to enable incremental builds for debugging (and don't forget to remove the two rm -rf's)
#  rm -f ../bin/run_aarch64_examples_and_tests.sh
#  cat >> ../bin/run_aarch64_examples_and_tests.sh << EOF
#!/bin/sh
#cd /opt/ichor/src/build
#ninja && ../bin/AsyncFileIOTests
#EOF
#  chmod +x ../bin/run_aarch64_examples_and_tests.sh
#  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged --security-opt seccomp=unconfined -it ichor-musl-aarch64 "sh -c 'ulimit -r unlimited && /opt/ichor/src/bin/run_aarch64_examples_and_tests.sh'" || exit 1
#  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged --security-opt seccomp=unconfined -it ichor-musl-aarch64 "cd /opt/ichor/src/build && ninja" || exit 1
  run_examples 1 0 0
  # run_fast_benchmarks # this is still too slow, need to refactor benchmarks to specify iterations on command line

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-musl-aarch64-bench -t ichor-musl-aarch64-bench --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . --platform=linux/arm64 || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-musl-aarch64-bench || exit 1
  rm -rf ../arm_bench
  mkdir -p ../arm_bench
  mv -f ../bin/* ../arm_bench
  rm -f ../arm_bench/*.a
fi

if [[ $DOCKERTSAN -eq 1 ]]; then
  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-tsan -t ichor-tsan --build-arg CONTAINER_OWNER_GID=$(id -g) --build-arg CONTAINER_OWNER_ID=$(id -u) . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-tsan || exit 1
  run_examples 0 1 0
  run_benchmarks
fi

# Quick libcpp compile check
rm -rf ./* ../bin/*
CC=clang-19 CXX=clang++-19 cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=0 -DICHOR_ENABLE_INTERNAL_DEBUGGING=0 -DICHOR_USE_MOLD=1 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_SPDLOG=1 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_LIBURING=1 -DICHOR_USE_SDEVENT=1 .. || exit 1
ninja || exit 1
ninja test || exit 1
run_examples 1 1 1

rm -rf ./* ../bin/*
CC=gcc-12 CXX=g++-12 cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_FORCE_32_BIT=1 -DCMAKE_TOOLCHAIN_FILE=../toolchain-i386.cmake -DICHOR_USE_SANITIZERS=0 -DICHOR_ENABLE_INTERNAL_DEBUGGING=0 -DICHOR_USE_MOLD=1 -DICHOR_USE_BOOST_BEAST=0 -DICHOR_USE_SPDLOG=1 -DICHOR_USE_HIREDIS=0 -DICHOR_USE_LIBURING=1 -DICHOR_USE_SDEVENT=1 -DICHOR_DISABLE_EXCEPTIONS=1 .. || exit 1
ninja || exit 1
ninja test || exit 1
run_examples 0 1 1

for i in ${!ccompilers[@]}; do
  export LD_LIBRARY_PATH=${ldlibpath[i]}
  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=1 -DICHOR_ENABLE_INTERNAL_DEBUGGING=1 -DICHOR_ENABLE_INTERNAL_IO_DEBUGGING=1 -DICHOR_ENABLE_INTERNAL_STL_DEBUGGING=1 -DICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING=1 -DICHOR_USE_MOLD=1 -DICHOR_USE_BOOST_BEAST=0 -DICHOR_USE_LIBCPP=0 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_SDEVENT=1 .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples 0 1 1
  run_benchmarks

  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=1 -DICHOR_ENABLE_INTERNAL_DEBUGGING=1 -DICHOR_DISABLE_RTTI=0 -DICHOR_USE_MOLD=1 -DICHOR_USE_BOOST_BEAST=0 -DICHOR_USE_LIBCPP=0 -DICHOR_USE_SPDLOG=1 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_SDEVENT=1 .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples 0 1 1
  run_benchmarks

  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_REMOVE_SOURCE_NAMES=0 -DICHOR_USE_SANITIZERS=0 -DICHOR_ENABLE_INTERNAL_DEBUGGING=1 -DICHOR_USE_MOLD=1 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_LIBCPP=0 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_SDEVENT=1 -DICHOR_DISABLE_EXCEPTIONS=1 .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples 1 1 1
  run_benchmarks
done

rm -rf ./* ../bin/*
CC=clang-19 CXX=clang++-19 cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=0 -DICHOR_USE_MOLD=1 -DICHOR_USE_BOOST_BEAST=1 -DICHOR_USE_HIREDIS=1 -DICHOR_USE_SDEVENT=1 .. || exit 1
ninja || exit 1
ninja test || exit 1
run_examples 1 1 1
run_benchmarks
