# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 7950X, 6000MHz@CL38 RAM, ubuntu 22.04, gcc 11.3

| Compile<br/>Options           |      coroutines      |                  events |                  start |         start & stop | 
|-------------------------------|:--------------------:|------------------------:|-----------------------:|---------------------:|
| std containers<br/>std alloc  | 1,625,225 µs<br/>5MB | 1,798,801 µs<br/>5765MB | 5,837,216 µs<br/>269MB | 1,404,590 µs<br/>5MB |
| absl containers<br/>std alloc | 2,082,084 µs<br/>5MB | 1,247,183 µs<br/>3725MB | 3,506,627 µs<br/>206MB | 1,902,961 µs<br/>5MB |
| std containers<br/>mimalloc   | 1,461,037 µs<br/>6MB |   790,691 µs<br/>4597MB | 4,308,959 µs<br/>241MB | 1,228,233 µs<br/>6MB |
| absl containers<br/>mimalloc  | 2,365,551 µs<br/>6MB |   995,326 µs<br/>3066MB | 3,533,197 µs<br/>185MB | 1,729,573 µs<br/>6MB |

Detailed data:
```
../std_std/ichor_coroutine_benchmark single threaded ran for 755,377 µs with 2,109,440 peak memory usage
../std_std/ichor_coroutine_benchmark multi threaded ran for 1,625,225 µs with 4,997,120 peak memory usage
../std_std/ichor_event_benchmark single threaded ran for 1,079,014 µs with 644,186,112 peak memory usage
../std_std/ichor_event_benchmark multi threaded ran for 1,798,801 µs with 5,764,915,200 peak memory usage
../std_std/ichor_serializer_benchmark single threaded ran for 166,456 µs with 4,317,184 peak memory usage
../std_std/ichor_serializer_benchmark multi threaded ran for 199,656 µs with 5,079,040 peak memory usage
../std_std/ichor_start_benchmark single threaded ran for 1,686,562 µs with 33,972,224 peak memory usage
../std_std/ichor_start_benchmark multi threaded ran for 5,837,216 µs with 269,119,488 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 723,629 µs with 2,105,344 peak memory usage
../std_std/ichor_start_stop_benchmark multi threaded ran for 1,404,590 µs with 5,025,792 peak memory usage
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 543,014 µs with 4,423,680 peak memory usage
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,461,037 µs with 6,197,248 peak memory usage
../std_mimalloc/ichor_event_benchmark single threaded ran for 541,121 µs with 566,636,544 peak memory usage
../std_mimalloc/ichor_event_benchmark multi threaded ran for 790,691 µs with 4,597,989,376 peak memory usage
../std_mimalloc/ichor_serializer_benchmark single threaded ran for 107,068 µs with 4,386,816 peak memory usage
../std_mimalloc/ichor_serializer_benchmark multi threaded ran for 132,187 µs with 6,651,904 peak memory usage
../std_mimalloc/ichor_start_benchmark single threaded ran for 1,416,221 µs with 30,838,784 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded ran for 4,308,959 µs with 240,508,928 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 583,636 µs with 4,403,200 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,228,233 µs with 6,295,552 peak memory usage
../absl_std/ichor_coroutine_benchmark single threaded ran for 824,735 µs with 2,105,344 peak memory usage
../absl_std/ichor_coroutine_benchmark multi threaded ran for 2,082,084 µs with 5,083,136 peak memory usage
../absl_std/ichor_event_benchmark single threaded ran for 498,557 µs with 417,460,224 peak memory usage
../absl_std/ichor_event_benchmark multi threaded ran for 1,247,183 µs with 3,725,066,240 peak memory usage
../absl_std/ichor_serializer_benchmark single threaded ran for 162,520 µs with 2,105,344 peak memory usage
../absl_std/ichor_serializer_benchmark multi threaded ran for 174,979 µs with 5,185,536 peak memory usage
../absl_std/ichor_start_benchmark single threaded ran for 3,011,951 µs with 26,529,792 peak memory usage
../absl_std/ichor_start_benchmark multi threaded ran for 3,506,627 µs with 206,782,464 peak memory usage
../absl_std/ichor_start_stop_benchmark single threaded ran for 815,238 µs with 2,093,056 peak memory usage
../absl_std/ichor_start_stop_benchmark multi threaded ran for 1,902,961 µs with 5,054,464 peak memory usage
../absl_mimalloc/ichor_coroutine_benchmark single threaded ran for 642,898 µs with 4,378,624 peak memory usage
../absl_mimalloc/ichor_coroutine_benchmark multi threaded ran for 2,365,551 µs with 6,483,968 peak memory usage
../absl_mimalloc/ichor_event_benchmark single threaded ran for 423,809 µs with 414,248,960 peak memory usage
../absl_mimalloc/ichor_event_benchmark multi threaded ran for 995,326 µs with 3,066,720,256 peak memory usage
../absl_mimalloc/ichor_serializer_benchmark single threaded ran for 130,445 µs with 4,329,472 peak memory usage
../absl_mimalloc/ichor_serializer_benchmark multi threaded ran for 138,909 µs with 6,889,472 peak memory usage
../absl_mimalloc/ichor_start_benchmark single threaded ran for 3,068,149 µs with 24,727,552 peak memory usage
../absl_mimalloc/ichor_start_benchmark multi threaded ran for 3,533,197 µs with 185,094,144 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark single threaded ran for 708,235 µs with 4,313,088 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,729,573 µs with 6,381,568 peak memory usage
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