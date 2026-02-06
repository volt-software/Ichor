#!/bin/bash

curr=`basename $(pwd)`

if [[ "$curr" != "build" ]]; then
  echo "Not in build directory"
  exit 1;
fi

trap cleanup SIGINT SIGTERM

LLVM_PREFIX="$(brew --prefix llvm)" 

export CPPFLAGS="-nostdinc++ -isystem $LLVM_PREFIX/include/c++/v1" 
export LDFLAGS="-nostdinc++ -L$LLVM_PREFIX/lib -L$LLVM_PREFIX/lib/c++ -Wl,-rpath,$LLVM_PREFIX/lib/c++ -Wl,-rpath,$LLVM_PREFIX/lib/c++ -L$LLVM_PREFIX/lib/unwind -Wl,-rpath,$LLVM_PREFIX/lib/unwind -lunwind -lc++ -lc++abi"

rm -rf * ../bin/*
cmake -GNinja -DCMAKE_C_COMPILER="$LLVM_PREFIX/bin/clang" -DCMAKE_CXX_COMPILER="$LLVM_PREFIX/bin/clang++" -DCMAKE_C_FLAGS="$CPPFLAGS" -DCMAKE_CXX_FLAGS="$CPPFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS" ..
ninja
