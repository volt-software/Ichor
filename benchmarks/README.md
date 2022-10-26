# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 7950X, 6000MHz@CL38 RAM, ubuntu 22.04, gcc 11.2

| Compile<br/>Options           |      coroutines      |                  events |                   start |         start & stop | 
|-------------------------------|:--------------------:|------------------------:|------------------------:|---------------------:|
| std containers<br/>std alloc  | 1,547,068 µs<br/>5MB | 1,597,950 µs<br/>5765MB | 12,835,175 µs<br/>239MB | 1,414,314 µs<br/>5MB |
| absl containers<br/>std alloc |  744,859 µs<br/>5MB  | 1,116,942 µs<br/>3725MB |  6,636,922 µs<br/>190MB |   848,583 µs<br/>5MB |
| std containers<br/>mimalloc   | 1,377,085 µs<br/>6MB |   742,369 µs<br/>4570MB |  7,896,230 µs<br/>213MB | 1,164,813 µs<br/>6MB |
| absl containers<br/>mimalloc  |  625,363 µs<br/>7MB  |   468,617 µs<br/>3350MB |  6,585,474 µs<br/>180MB | 2,644,657 µs<br/>6MB |

TODO:
- Test with adding new services
- Test with modifying callbacks while broadcasting event
- Test with modifying interceptors while broadcasting event
- Test with creating new services with dependencies while broadcasting event
-

Detailed data:
```
../std_std/ichor_coroutine_benchmark single threaded ran for 734,229 µs with 2,109,440 peak memory usage
../std_std/ichor_coroutine_benchmark multi threaded ran for 1,547,068 µs with 5,009,408 peak memory usage
../std_std/ichor_event_benchmark single threaded ran for 1,044,476 µs with 644,239,360 peak memory usage
../std_std/ichor_event_benchmark multi threaded ran for 1,597,950 µs with 5,764,964,352 peak memory usage
../std_std/ichor_start_benchmark single threaded ran for 3,522,223 µs with 30,400,512 peak memory usage
../std_std/ichor_start_benchmark multi threaded ran for 12,835,175 µs with 239,198,208 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 704,523 µs with 2,043,904 peak memory usage
../std_std/ichor_start_stop_benchmark multi threaded ran for 1,414,314 µs with 4,993,024 peak memory usage
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 524,971 µs with 4,374,528 peak memory usage
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,377,085 µs with 6,307,840 peak memory usage
../std_mimalloc/ichor_event_benchmark single threaded ran for 529,349 µs with 566,665,216 peak memory usage
../std_mimalloc/ichor_event_benchmark multi threaded ran for 742,369 µs with 4,570,415,104 peak memory usage
../std_mimalloc/ichor_start_benchmark single threaded ran for 2,492,560 µs with 27,594,752 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded ran for 7,896,230 µs with 213,123,072 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 531,463 µs with 4,403,200 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,164,813 µs with 6,205,440 peak memory usage
../absl_std/ichor_coroutine_benchmark single threaded ran for 688,268 µs with 2,105,344 peak memory usage
../absl_std/ichor_coroutine_benchmark multi threaded ran for 744,859 µs with 5,074,944 peak memory usage
../absl_std/ichor_event_benchmark single threaded ran for 486,151 µs with 417,505,280 peak memory usage
../absl_std/ichor_event_benchmark multi threaded ran for 1,116,942 µs with 3,725,107,200 peak memory usage
../absl_std/ichor_start_benchmark single threaded ran for 5,810,502 µs with 24,772,608 peak memory usage
../absl_std/ichor_start_benchmark multi threaded ran for 6,636,922 µs with 190,849,024 peak memory usage
../absl_std/ichor_start_stop_benchmark single threaded ran for 755,909 µs with 2,162,688 peak memory usage
../absl_std/ichor_start_stop_benchmark multi threaded ran for 848,583 µs with 5,115,904 peak memory usage
../absl_mimalloc/ichor_coroutine_benchmark single threaded ran for 553,252 µs with 4,354,048 peak memory usage
../absl_mimalloc/ichor_coroutine_benchmark multi threaded ran for 625,363 µs with 6,574,080 peak memory usage
../absl_mimalloc/ichor_event_benchmark single threaded ran for 413,751 µs with 414,257,152 peak memory usage
../absl_mimalloc/ichor_event_benchmark multi threaded ran for 468,617 µs with 3,350,208,512 peak memory usage
 ../absl_mimalloc/ichor_start_benchmark single threaded ran for 5,657,110 µs with 24,166,400 peak memory usage
../absl_mimalloc/ichor_start_benchmark multi threaded ran for 6,585,474 µs with 179,859,456 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark single threaded ran for 645,476 µs with 4,366,336 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark multi threaded ran for 2,644,657 µs with 6,340,608 peak memory usage
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