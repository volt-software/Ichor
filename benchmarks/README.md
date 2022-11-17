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
../std_std/ichor_coroutine_benchmark single threaded ran for 788,072 µs with 2,060,288 peak memory usage 6,344,597 coroutines/s
../std_std/ichor_coroutine_benchmark multi threaded ran for 1,221,534 µs with 4,972,544 peak memory usage 32,745,711 coroutines/s
../std_std/ichor_event_benchmark single threaded ran for 1,021,796 µs with 644,202,496 peak memory usage 4,893,344 events/s
../std_std/ichor_event_benchmark multi threaded ran for 1,726,927 µs with 5,764,931,584 peak memory usage 23,162,530 events/s
../std_std/ichor_serializer_benchmark single threaded ran for 128,512 µs with 4,349,952 peak memory usage 241,222,609 B/s
../std_std/ichor_serializer_benchmark multi threaded ran for 170,078 µs with 5,050,368 peak memory usage 1,458,154,493 B/s
../std_std/ichor_start_benchmark single threaded ran for 1,355,569 µs with 30,830,592 peak memory usage
../std_std/ichor_start_benchmark multi threaded ran for 4,121,798 µs with 237,264,896 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 729,747 µs with 2,064,384 peak memory usage 1,370,337 start & stop /s
../std_std/ichor_start_stop_benchmark multi threaded ran for 1,369,609 µs with 4,988,928 peak memory usage 5,841,083 start & stop /s
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 526,518 µs with 4,382,720 peak memory usage 9,496,351 coroutines/s
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,030,269 µs with 6,266,880 peak memory usage 38,824,811 coroutines/s
../std_mimalloc/ichor_event_benchmark single threaded ran for 530,509 µs with 566,640,640 peak memory usage 9,424,910 events/s
../std_mimalloc/ichor_event_benchmark multi threaded ran for 772,448 µs with 4,595,884,032 peak memory usage 51,783,421 events/s
../std_mimalloc/ichor_serializer_benchmark single threaded ran for 89,272 µs with 4,423,680 peak memory usage 347,253,338 B/s
../std_mimalloc/ichor_serializer_benchmark multi threaded ran for 109,884 µs with 6,721,536 peak memory usage 2,256,925,485 B/s
../std_mimalloc/ichor_start_benchmark single threaded ran for 1,081,355 µs with 29,233,152 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded ran for 2,860,991 µs with 226,861,056 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 566,299 µs with 4,407,296 peak memory usage 1,765,851 start & stop /s
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,100,787 µs with 6,275,072 peak memory usage 7,267,527 start & stop /s
../absl_std/ichor_coroutine_benchmark single threaded ran for 781,776 µs with 2,121,728 peak memory usage 6,395,693 coroutines/s
../absl_std/ichor_coroutine_benchmark multi threaded ran for 1,914,536 µs with 5,009,408 peak memory usage 20,892,790 coroutines/s
../absl_std/ichor_event_benchmark single threaded ran for 490,068 µs with 417,484,800 peak memory usage 10,202,665 events/s
../absl_std/ichor_event_benchmark multi threaded ran for 1,450,696 µs with 3,725,062,144 peak memory usage 27,572,971 events/s
../absl_std/ichor_serializer_benchmark single threaded ran for 125,925 µs with 4,411,392 peak memory usage 246,178,280 B/s
../absl_std/ichor_serializer_benchmark multi threaded ran for 257,721 µs with 5,111,808 peak memory usage 962,280,916 B/s
../absl_std/ichor_start_benchmark single threaded ran for 1,221,086 µs with 22,859,776 peak memory usage
../absl_std/ichor_start_benchmark multi threaded ran for 1,240,699 µs with 171,200,512 peak memory usage
../absl_std/ichor_start_stop_benchmark single threaded ran for 839,179 µs with 2,117,632 peak memory usage 1,191,640 start & stop /s
../absl_std/ichor_start_stop_benchmark multi threaded ran for 1,944,217 µs with 5,070,848 peak memory usage 4,114,767 start & stop /s
../absl_mimalloc/ichor_coroutine_benchmark single threaded ran for 623,569 µs with 4,337,664 peak memory usage 8,018,358 coroutines/s
../absl_mimalloc/ichor_coroutine_benchmark multi threaded ran for 2,027,534 µs with 6,447,104 peak memory usage 19,728,399 coroutines/s
../absl_mimalloc/ichor_event_benchmark single threaded ran for 423,526 µs with 414,257,152 peak memory usage 11,805,650 events/s
../absl_mimalloc/ichor_event_benchmark multi threaded ran for 745,321 µs with 3,350,261,760 peak memory usage 53,668,151 events/s
../absl_mimalloc/ichor_serializer_benchmark single threaded ran for 90,178 µs with 4,358,144 peak memory usage 343,764,554 B/s
../absl_mimalloc/ichor_serializer_benchmark multi threaded ran for 109,462 µs with 6,828,032 peak memory usage 2,265,626,427 B/s
../absl_mimalloc/ichor_start_benchmark single threaded ran for 1,220,268 µs with 21,995,520 peak memory usage
../absl_mimalloc/ichor_start_benchmark multi threaded ran for 1,221,573 µs with 161,972,224 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark single threaded ran for 629,772 µs with 4,304,896 peak memory usage 1,587,876 start & stop /s
../absl_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,931,636 µs with 6,365,184 peak memory usage 4,141,567 start & stop /s
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