# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 7950X, 6000MHz@CL38 RAM, ubuntu 22.04, gcc 11.3

| Compile<br/>Options           |      coroutines      |                  events |                  start |         start & stop | 
|-------------------------------|:--------------------:|------------------------:|-----------------------:|---------------------:|
| std containers<br/>std alloc  | 1,181,501 µs<br/>5MB | 1,800,140 µs<br/>5765MB | 4,243,344 µs<br/>265MB | 1,377,814 µs<br/>5MB |
| absl containers<br/>std alloc | 1,993,405 µs<br/>5MB | 1,339,492 µs<br/>3725MB | 2,474,879 µs<br/>200MB | 2,322,150 µs<br/>5MB |
| std containers<br/>mimalloc   | 1,040,090 µs<br/>6MB |   800,751 µs<br/>4597MB | 2,912,374 µs<br/>244MB | 1,183,614 µs<br/>6MB |
| absl containers<br/>mimalloc  | 1,892,567 µs<br/>6MB |   662,971 µs<br/>3350MB | 2,407,267 µs<br/>185MB | 2,061,239 µs<br/>6MB |

Detailed data:
```
../std_std/ichor_coroutine_benchmark single threaded ran for 726,930 µs with 2,048,000 peak memory usage
../std_std/ichor_coroutine_benchmark multi threaded ran for 1,181,501 µs with 4,980,736 peak memory usage
../std_std/ichor_event_benchmark single threaded ran for 1,027,811 µs with 644,333,568 peak memory usage
../std_std/ichor_event_benchmark multi threaded ran for 1,800,140 µs with 5,764,935,680 peak memory usage
../std_std/ichor_serializer_benchmark single threaded ran for 126,093 µs with 4,472,832 peak memory usage
../std_std/ichor_serializer_benchmark multi threaded ran for 160,487 µs with 5,021,696 peak memory usage
../std_std/ichor_start_benchmark single threaded ran for 1,506,940 µs with 33,914,880 peak memory usage
../std_std/ichor_start_benchmark multi threaded ran for 4,243,344 µs with 264,970,240 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 708,308 µs with 2,048,000 peak memory usage
../std_std/ichor_start_stop_benchmark multi threaded ran for 1,377,814 µs with 4,997,120 peak memory usage
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 532,867 µs with 4,370,432 peak memory usage
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,040,090 µs with 6,201,344 peak memory usage
../std_mimalloc/ichor_event_benchmark single threaded ran for 543,139 µs with 566,661,120 peak memory usage
../std_mimalloc/ichor_event_benchmark multi threaded ran for 800,751 µs with 4,597,104,640 peak memory usage
../std_mimalloc/ichor_serializer_benchmark single threaded ran for 100,978 µs with 4,419,584 peak memory usage
../std_mimalloc/ichor_serializer_benchmark multi threaded ran for 105,303 µs with 6,664,192 peak memory usage
../std_mimalloc/ichor_start_benchmark single threaded ran for 1,265,096 µs with 31,125,504 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded ran for 2,912,374 µs with 244,310,016 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 571,592 µs with 4,378,624 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,183,614 µs with 6,320,128 peak memory usage
../absl_std/ichor_coroutine_benchmark single threaded ran for 829,385 µs with 2,105,344 peak memory usage
../absl_std/ichor_coroutine_benchmark multi threaded ran for 1,993,405 µs with 5,038,080 peak memory usage
../absl_std/ichor_event_benchmark single threaded ran for 494,519 µs with 417,583,104 peak memory usage
../absl_std/ichor_event_benchmark multi threaded ran for 1,339,492 µs with 3,725,058,048 peak memory usage
../absl_std/ichor_serializer_benchmark single threaded ran for 124,899 µs with 4,542,464 peak memory usage
../absl_std/ichor_serializer_benchmark multi threaded ran for 144,409 µs with 5,107,712 peak memory usage
../absl_std/ichor_start_benchmark single threaded ran for 2,275,728 µs with 26,370,048 peak memory usage
../absl_std/ichor_start_benchmark multi threaded ran for 2,474,879 µs with 200,626,176 peak memory usage
../absl_std/ichor_start_stop_benchmark single threaded ran for 831,378 µs with 4,550,656 peak memory usage
../absl_std/ichor_start_stop_benchmark multi threaded ran for 2,322,150 µs with 5,091,328 peak memory usage
../absl_mimalloc/ichor_coroutine_benchmark single threaded ran for 707,157 µs with 4,333,568 peak memory usage
../absl_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,892,567 µs with 6,340,608 peak memory usage
../absl_mimalloc/ichor_event_benchmark single threaded ran for 433,040 µs with 414,236,672 peak memory usage
../absl_mimalloc/ichor_event_benchmark multi threaded ran for 662,971 µs with 3,349,970,944 peak memory usage
../absl_mimalloc/ichor_serializer_benchmark single threaded ran for 93,149 µs with 4,321,280 peak memory usage
../absl_mimalloc/ichor_serializer_benchmark multi threaded ran for 126,922 µs with 6,889,472 peak memory usage
../absl_mimalloc/ichor_start_benchmark single threaded ran for 2,102,909 µs with 24,690,688 peak memory usage
../absl_mimalloc/ichor_start_benchmark multi threaded ran for 2,407,267 µs with 184,938,496 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark single threaded ran for 699,493 µs with 4,313,088 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark multi threaded ran for 2,061,239 µs with 6,365,184 peak memory usage
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