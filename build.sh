#!/bin/bash

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

ccompilers=("clang-14" "clang-16" "gcc-11" "gcc-12")
cppcompilers=("clang++-14" "clang++-16" "g++-11" "g++-12")

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


rm -rf ./* ../bin/*
docker build -f ../Dockerfile -t ichor . || exit 1
docker run -v $(pwd)/../:/opt/ichor/src -it ichor || exit 1
run_examples

rm -rf ./* ../bin/*
docker build -f ../Dockerfile-musl -t ichor-musl . || exit 1
docker run -v $(pwd)/../:/opt/ichor/src -it ichor-musl || exit 1
run_examples

rm -rf ./* ../bin/*
docker build -f ../Dockerfile-asan -t ichor-asan . || exit 1
docker run -v $(pwd)/../:/opt/ichor/src -it ichor-asan || exit 1
run_examples

rm -rf ./* ../bin/*
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes || exit 1
docker build -f ../Dockerfile-musl-aarch64 -t ichor-musl-aarch64 . || exit 1
docker run -v $(pwd)/../:/opt/ichor/src -it ichor-musl-aarch64 || exit 1
cat >> ../bin/run_aarch64_examples_and_tests.sh << EOF
#!/bin/sh
FILES=/opt/ichor/src/bin/*
for f in \$FILES; do
  if [[ "\$f" != *"Redis"* ]] && [[ "\$f" != *"benchmark"* ]] && [[ "\$f" != *"minimal"* ]] && [[ "\$f" != *"ping"* ]] && [[ "\$f" != *"pong"* ]] && [[ "\$f" != *"Started"* ]] && [[ "\$f" != *".sh" ]] && [[ -x "\$f" ]] ; then
    echo "Running \$f"
    \$f || echo "Error" && exit 1
  fi
done
EOF
chmod +x ../bin/run_aarch64_examples_and_tests.sh
docker run -v $(pwd)/../:/opt/ichor/src --privileged -it ichor-musl-aarch64 "sh -c 'ulimit -r unlimited && /opt/ichor/src/bin/run_aarch64_examples_and_tests.sh'" || exit 1

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
