#!/bin/bash
#Recommend using the following ASAN_OPTIONS:
#export ASAN_OPTIONS="detect_stack_use_after_return=1:fast_unwind_on_malloc=0:malloc_context_size=60:detect_invalid_pointer_pairs=1:verbosity=1"

cleanup ()
{
    kill -s SIGTERM $!
    exit 0
}

trap cleanup SIGINT SIGTERM

while [ 1 ]
do
    #gdb -ex run -ex quit ../bin/ichor_websocket_example
    ../bin/ichor_websocket_example || exit
done
