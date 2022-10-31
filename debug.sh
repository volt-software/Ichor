#!/bin/bash
export ASAN_OPTIONS="detect_stack_use_after_return=1:fast_unwind_on_malloc=0"

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

while [ 1 ]
do
    gdb -ex run -ex quit ../bin/ichor_http_example
done
