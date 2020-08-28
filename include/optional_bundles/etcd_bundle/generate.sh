#!/bin/bash
protoc -I . -I ../../../external/googleapis/ --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ./rpc.proto
protoc -I . -I ../../../external/googleapis/  --cpp_out=. ./*.proto
