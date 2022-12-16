# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 7950X, 6000MHz@CL38 RAM, ubuntu 22.04, gcc 11.3

| Compile<br/>Options           |      coroutines      |                  events |                  start |         start & stop | 
|-------------------------------|:--------------------:|------------------------:|-----------------------:|---------------------:|
| std containers<br/>std alloc  | 1,221,534 µs<br/>5MB | 1,726,927 µs<br/>5765MB | 4,121,798 µs<br/>237MB | 1,369,609 µs<br/>5MB |
| absl containers<br/>std alloc | 1,914,536 µs<br/>5MB | 1,450,696 µs<br/>3725MB | 1,240,699 µs<br/>171MB | 1,944,217 µs<br/>5MB |
| std containers<br/>mimalloc   | 1,030,269 µs<br/>6MB |   772,448 µs<br/>4596MB | 2,860,991 µs<br/>227MB | 1,100,787 µs<br/>6MB |
| absl containers<br/>mimalloc  | 2,027,534 µs<br/>6MB |   745,321 µs<br/>3350MB | 1,221,573 µs<br/>162MB | 1,931,636 µs<br/>6MB |

Detailed data:
```
../std_std/ichor_coroutine_benchmark single threaded ran for 526,618 µs with 2,076,672 peak memory usage 9,494,548 coroutines/s
../std_std/ichor_coroutine_benchmark multi threaded ran for 919,541 µs with 5,349,376 peak memory usage 43,499,963 coroutines/s
../std_std/ichor_event_benchmark single threaded ran for 1,052,795 µs with 644,497,408 peak memory usage 4,749,262 events/s
../std_std/ichor_event_benchmark multi threaded ran for 1,861,859 µs with 5,765,341,184 peak memory usage 21,483,903 events/s
../std_std/ichor_serializer_benchmark single threaded rapidjson ran for 1,414,303 µs with 4,624,384 peak memory usage 219,189,240 B/s
../std_std/ichor_serializer_benchmark multi threaded rapidjson ran for 1,543,250 µs with 5,427,200 peak memory usage 1,606,998,218 B/s
../std_std/ichor_serializer_benchmark single threaded boost.JSON ran for 2,329,255 µs with 5,427,200 peak memory usage 128,796,546 B/s
../std_std/ichor_serializer_benchmark multi threaded boost.JSON ran for 2,384,402 µs with 5,517,312 peak memory usage 1,006,541,682 B/s
../std_std/ichor_start_benchmark single threaded ran for 2,782,390 µs with 44,445,696 peak memory usage
../std_std/ichor_start_benchmark multi threaded ran for 5,264,392 µs with 361,267,200 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 1,855,356 µs with 148,443,136 peak memory usage 538,980 start & stop /s
../std_std/ichor_start_stop_benchmark multi threaded ran for 2,689,061 µs with 1,301,413,888 peak memory usage 2,975,016 start & stop /s
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 394,948 µs with 4,448,256 peak memory usage 12,659,894 coroutines/s
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 819,952 µs with 6,717,440 peak memory usage 48,783,343 coroutines/s
../std_mimalloc/ichor_event_benchmark single threaded ran for 560,596 µs with 566,935,552 peak memory usage 8,919,078 events/s
../std_mimalloc/ichor_event_benchmark multi threaded ran for 862,304 µs with 4,598,292,480 peak memory usage 46,387,352 events/s
../std_mimalloc/ichor_serializer_benchmark single threaded rapidjson ran for 1,189,574 µs with 4,362,240 peak memory usage 260,597,491 B/s
../std_mimalloc/ichor_serializer_benchmark multi threaded rapidjson ran for 1,779,266 µs with 6,598,656 peak memory usage 1,393,833,187 B/s
../std_mimalloc/ichor_serializer_benchmark single threaded boost.JSON ran for 1,867,545 µs with 6,598,656 peak memory usage 160,638,699 B/s
../std_mimalloc/ichor_serializer_benchmark multi threaded boost.JSON ran for 1,966,052 µs with 7,294,976 peak memory usage 1,220,720,509 B/s
../std_mimalloc/ichor_start_benchmark single threaded ran for 1,980,126 µs with 43,876,352 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded ran for 4,333,236 µs with 356,306,944 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 1,464,828 µs with 117,346,304 peak memory usage 682,674 start & stop /s
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 2,735,721 µs with 972,042,240 peak memory usage 2,924,274 start & stop /s
../absl_std/ichor_coroutine_benchmark single threaded ran for 570,917 µs with 2,134,016 peak memory usage 8,757,840 coroutines/s
../absl_std/ichor_coroutine_benchmark multi threaded ran for 1,264,841 µs with 5,423,104 peak memory usage 31,624,528 coroutines/s
../absl_std/ichor_event_benchmark single threaded ran for 531,334 µs with 417,771,520 peak memory usage 9,410,276 events/s
../absl_std/ichor_event_benchmark multi threaded ran for 1,431,033 µs with 3,725,459,456 peak memory usage 27,951,836 events/s
../absl_std/ichor_serializer_benchmark single threaded rapidjson ran for 1,484,965 µs with 4,726,784 peak memory usage 208,759,129 B/s
../absl_std/ichor_serializer_benchmark multi threaded rapidjson ran for 1,618,720 µs with 5,525,504 peak memory usage 1,532,074,725 B/s
../absl_std/ichor_serializer_benchmark single threaded boost.JSON ran for 2,123,010 µs with 5,525,504 peak memory usage 141,308,802 B/s
../absl_std/ichor_serializer_benchmark multi threaded boost.JSON ran for 2,135,771 µs with 5,611,520 peak memory usage 1,123,715,978 B/s
../absl_std/ichor_start_benchmark single threaded ran for 2,474,527 µs with 36,003,840 peak memory usage
../absl_std/ichor_start_benchmark multi threaded ran for 2,993,144 µs with 287,330,304 peak memory usage
../absl_std/ichor_start_stop_benchmark single threaded ran for 1,763,160 µs with 103,108,608 peak memory usage 567,163 start & stop /s
../absl_std/ichor_start_stop_benchmark multi threaded ran for 2,542,654 µs with 893,505,536 peak memory usage 3,146,318 start & stop /s
../absl_mimalloc/ichor_coroutine_benchmark single threaded ran for 441,021 µs with 4,366,336 peak memory usage 11,337,328 coroutines/s
../absl_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,203,060 µs with 6,832,128 peak memory usage 33,248,549 coroutines/s
../absl_mimalloc/ichor_event_benchmark single threaded ran for 471,418 µs with 414,523,392 peak memory usage 10,606,298 events/s
../absl_mimalloc/ichor_event_benchmark multi threaded ran for 824,055 µs with 3,352,109,056 peak memory usage 48,540,449 events/s
../absl_mimalloc/ichor_serializer_benchmark single threaded rapidjson ran for 1,145,250 µs with 4,378,624 peak memory usage 270,683,256 B/s
../absl_mimalloc/ichor_serializer_benchmark multi threaded rapidjson ran for 1,269,312 µs with 7,262,208 peak memory usage 1,953,814,349 B/s
../absl_mimalloc/ichor_serializer_benchmark single threaded boost.JSON ran for 2,012,652 µs with 7,262,208 peak memory usage 149,057,065 B/s
../absl_mimalloc/ichor_serializer_benchmark multi threaded boost.JSON ran for 2,162,619 µs with 7,364,608 peak memory usage 1,109,765,520 B/s
../absl_mimalloc/ichor_start_benchmark single threaded ran for 2,204,629 µs with 37,232,640 peak memory usage
../absl_mimalloc/ichor_start_benchmark multi threaded ran for 2,497,153 µs with 294,981,632 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark single threaded ran for 1,394,751 µs with 86,884,352 peak memory usage 716,973 start & stop /s
../absl_mimalloc/ichor_start_stop_benchmark multi threaded ran for 2,555,089 µs with 668,049,408 peak memory usage 3,131,006 start & stop /s
```

Realtime example on a vanilla linux:
```
root# echo 950000 > /proc/sys/kernel/sched_rt_runtime_us #force kernel to fail our deadlines
root# ../bin/ichor_realtime_example 
duration of run 9,848 is 50,104 µs which exceeded maximum of 2,000 µs
duration of run 16,464 is 51,101 µs which exceeded maximum of 2,000 µs
duration min/max/avg: 142/51,101/147 µs
root# echo 999999 > /proc/sys/kernel/sched_rt_runtime_us
root# ../bin/ichor_realtime_example 
duration min/max/avg: 142/327/142 µs
root# echo -1 > /proc/sys/kernel/sched_rt_runtime_us
root# ../bin/ichor_realtime_example 
duration min/max/avg: 142/328/142 µs

```

These benchmarks currently lead to the characteristics:
* creating services with dependencies overhead is likely O(N²).
* Starting services, stopping services overhead is likely O(N)
* event handling overhead is amortized O(1)
* Creating more threads is not 100% linearizable in all cases (pure event creation/handling seems to be, otherwise not really)
* Latency spikes while scheduling in user-space is in the order of 100's of microseconds, lower if a proper [realtime tuning guide](https://rigtorp.se/low-latency-guide/) is used (except if the queue goes empty and is forced to sleep)

Help with improving memory usage and the O(N²) behaviour would be appreciated.
