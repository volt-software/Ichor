#!/bin/bash

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

POSITIONAL_ARGS=()
DOCKER=1

while [[ $# -gt 0 ]]; do
  case $1 in
    --no-docker)
      DOCKER=0
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

ccompilers=("clang-14" "clang-18" "gcc-11" "gcc")
cppcompilers=("clang++-14" "clang++-18" "g++-11" "g++")

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
  ../bin/ichor_factory_example || exit 1
  ../bin/ichor_introspection_example || exit 1
  ../bin/ichor_websocket_example || exit 1
  ../bin/ichor_websocket_example -t4 || exit 1
  ../bin/ichor_yielding_timer_example || exit 1
  ../bin/ichor_etcd_example || exit 1
}
run_benchmarks ()
{
  ../bin/ichor_coroutine_benchmark || exit 1
  ../bin/ichor_event_benchmark || exit 1
  ../bin/ichor_serializer_benchmark || exit 1
  ../bin/ichor_start_benchmark || exit 1
  ../bin/ichor_start_stop_benchmark || exit 1
  ../bin/ichor_utils_benchmark -r || exit 1
}


if [[ $DOCKER -eq 1 ]]; then
  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile -t ichor . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor || exit 1
  run_examples
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-musl -t ichor-musl . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-musl || exit 1
  run_examples
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-musl "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-asan -t ichor-asan . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-asan || exit 1
  run_examples
  run_benchmarks
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-asan "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-asan-clang -t ichor-asan-clang . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-asan-clang || exit 1
  run_examples
  run_benchmarks
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-asan-clang "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1

  rm -rf ./* ../bin/*
  docker build -f ../Dockerfile-tsan -t ichor-tsan . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-tsan || exit 1
  run_examples
  run_benchmarks
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-tsan "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1

  rm -rf ./* ../bin/*
  docker run --rm --privileged multiarch/qemu-user-static --reset -p yes || exit 1
  docker build -f ../Dockerfile-musl-aarch64 -t ichor-musl-aarch64 . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-musl-aarch64 || exit 1
  rm -f ../bin/run_aarch64_examples_and_tests.sh
  cat >> ../bin/run_aarch64_examples_and_tests.sh << EOF
#!/bin/sh
FILES=/opt/ichor/src/bin/*
for f in \$FILES; do
  if [[ "\$f" != *"Tests" ]] && [[ "\$f" != *"benchmark" ]] && [[ "\$f" != *"minimal"* ]] && [[ "\$f" != *"tcp"* ]] && [[ "\$f" != *"ping"* ]] && [[ "\$f" != *"etcd"* ]] && [[ "\$f" != *"pong"* ]] && [[ "\$f" != *".sh" ]] && [[ -x "\$f" ]] && [[ ! -d "\$f" ]] ; then
    echo "Running \${f}"
    \$f || exit 1
  fi
done
EOF
  chmod +x ../bin/run_aarch64_examples_and_tests.sh
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-musl-aarch64 "sh -c 'ulimit -r unlimited && /opt/ichor/src/bin/run_aarch64_examples_and_tests.sh'" || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-musl-aarch64 "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1

  rm -rf ./* ../bin/*
  docker run --rm --privileged multiarch/qemu-user-static --reset -p yes || exit 1
  docker build -f ../Dockerfile-musl-aarch64-bench -t ichor-musl-aarch64-bench . || exit 1
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm -it ichor-musl-aarch64-bench || exit 1
  mkdir -p ../arm_bench
  mv -f ../bin/* ../arm_bench
  rm -f ../arm_bench/*.a
  docker run -v $(pwd)/../:/opt/ichor/src -v $(pwd)/../build:/opt/ichor/build --rm --privileged -it ichor-musl-aarch64 "rm -rf /opt/ichor/src/bin/* /opt/ichor/src/build/*" || exit 1
fi

# Quick libcpp compile check
rm -rf ./* ../bin/*
CC=clang-18 CXX=clang++-18 cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=OFF -DICHOR_ENABLE_INTERNAL_DEBUGGING=OFF -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_SPDLOG=ON -DICHOR_USE_HIREDIS=ON .. || exit 1
ninja || exit 1
ninja test || exit 1
run_examples

for i in ${!ccompilers[@]}; do
  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=ON -DICHOR_ENABLE_INTERNAL_DEBUGGING=ON -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_LIBCPP=OFF -DICHOR_USE_HIREDIS=ON .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples
  run_benchmarks

  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DICHOR_USE_SANITIZERS=ON -DICHOR_ENABLE_INTERNAL_DEBUGGING=ON -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_LIBCPP=OFF -DICHOR_USE_SPDLOG=ON -DICHOR_USE_HIREDIS=ON .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples
  run_benchmarks

  rm -rf ./* ../bin/*
  CC=${ccompilers[i]} CXX=${cppcompilers[i]} cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_ENABLE_INTERNAL_DEBUGGING=ON -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_LIBCPP=OFF -DICHOR_USE_HIREDIS=ON .. || exit 1
  ninja || exit 1
  ninja test || exit 1
  run_examples
  run_benchmarks
done

rm -rf ./* ../bin/*
CC=clang-18 CXX=clang++-18 cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_USE_SANITIZERS=OFF -DICHOR_USE_MOLD=ON -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_HIREDIS=ON .. || exit 1
ninja || exit 1
ninja test || exit 1
run_examples
run_benchmarks
