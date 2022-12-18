# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 7950X, 6000MHz@CL38 RAM, ubuntu 22.04, gcc 11.3

| Compile<br/>Options           |      coroutines      |                  events |                  start |         start & stop | 
|-------------------------------|:--------------------:|------------------------:|-----------------------:|---------------------:|
| std containers<br/>std alloc  | 1,013,137 µs<br/>5MB | 1,797,842 µs<br/>5765MB | 5,389,847 µs<br/>362MB | 2,298,210 µs<br/>5MB |
| absl containers<br/>std alloc | 1,397,622 µs<br/>5MB | 1,395,031 µs<br/>3725MB | 2,817,231 µs<br/>282MB | 2,410,041 µs<br/>6MB |
| std containers<br/>mimalloc   |  867,144 µs<br/>7MB  |   818,567 µs<br/>4598MB | 4,404,650 µs<br/>356MB | 1,832,268 µs<br/>7MB |
| absl containers<br/>mimalloc  | 1,187,087 µs<br/>7MB |   836,063 µs<br/>3352MB | 2,497,607 µs<br/>295MB | 2,091,859 µs<br/>7MB |

Detailed data:
```
../std_std/ichor_coroutine_benchmark single threaded ran for 556,739 µs with 2,072,576 peak memory usage 8,980,868 coroutines/s
../std_std/ichor_coroutine_benchmark multi threaded ran for 1,013,137 µs with 5,328,896 peak memory usage 39,481,333 coroutines/s
../std_std/ichor_event_benchmark single threaded ran for 1,050,739 µs with 644,476,928 peak memory usage 4,758,555 events/s
../std_std/ichor_event_benchmark multi threaded ran for 1,797,842 µs with 5,765,300,224 peak memory usage 22,248,896 events/s
../std_std/ichor_serializer_benchmark single threaded rapidjson ran for 729,540 µs with 4,628,480 peak memory usage 212,462,647 B/s
../std_std/ichor_serializer_benchmark multi threaded rapidjson ran for 765,606 µs with 5,427,200 peak memory usage 1,619,632,030 B/s
../std_std/ichor_serializer_benchmark single threaded boost.JSON ran for 1,165,199 µs with 5,427,200 peak memory usage 128,733,375 B/s
../std_std/ichor_serializer_benchmark multi threaded boost.JSON ran for 1,218,898 µs with 5,517,312 peak memory usage 984,495,831 B/s
../std_std/ichor_start_benchmark single threaded ran for 2,907,044 µs with 44,441,600 peak memory usage
../std_std/ichor_start_benchmark multi threaded ran for 5,389,847 µs with 361,631,744 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 1,501,918 µs with 2,076,672 peak memory usage 665,815 start & stop /s
../std_std/ichor_start_stop_benchmark multi threaded ran for 2,298,210 µs with 5,427,200 peak memory usage 3,480,969 start & stop /s
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 402,409 µs with 4,440,064 peak memory usage 12,425,169 coroutines/s
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 867,144 µs with 6,684,672 peak memory usage 46,128,440 coroutines/s
../std_mimalloc/ichor_event_benchmark single threaded ran for 566,218 µs with 566,939,648 peak memory usage 8,830,521 events/s
../std_mimalloc/ichor_event_benchmark multi threaded ran for 818,567 µs with 4,598,132,736 peak memory usage 48,865,883 events/s
../std_mimalloc/ichor_serializer_benchmark single threaded rapidjson ran for 580,956 µs with 4,403,200 peak memory usage 266,801,616 B/s
../std_mimalloc/ichor_serializer_benchmark multi threaded rapidjson ran for 641,717 µs with 7,192,576 peak memory usage 1,932,315,958 B/s
../std_mimalloc/ichor_serializer_benchmark single threaded boost.JSON ran for 1,020,584 µs with 7,192,576 peak memory usage 146,974,673 B/s
../std_mimalloc/ichor_serializer_benchmark multi threaded boost.JSON ran for 1,005,983 µs with 7,364,608 peak memory usage 1,192,863,100 B/s
../std_mimalloc/ichor_start_benchmark single threaded ran for 2,029,792 µs with 43,855,872 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded ran for 4,404,650 µs with 356,323,328 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 1,101,475 µs with 4,358,144 peak memory usage 907,873 start & stop /s
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,832,268 µs with 6,909,952 peak memory usage 4,366,173 start & stop /s
../absl_std/ichor_coroutine_benchmark single threaded ran for 581,070 µs with 2,134,016 peak memory usage 8,604,815 coroutines/s
../absl_std/ichor_coroutine_benchmark multi threaded ran for 1,397,622 µs with 5,406,720 peak memory usage 28,620,041 coroutines/s
../absl_std/ichor_event_benchmark single threaded ran for 540,537 µs with 417,771,520 peak memory usage 9,250,060 events/s
../absl_std/ichor_event_benchmark multi threaded ran for 1,395,031 µs with 3,725,459,456 peak memory usage 28,673,197 events/s
../absl_std/ichor_serializer_benchmark single threaded rapidjson ran for 733,318 µs with 4,780,032 peak memory usage 211,368,055 B/s
../absl_std/ichor_serializer_benchmark multi threaded rapidjson ran for 754,645 µs with 5,603,328 peak memory usage 1,643,156,716 B/s
../absl_std/ichor_serializer_benchmark single threaded boost.JSON ran for 1,053,288 µs with 5,603,328 peak memory usage 142,411,192 B/s
../absl_std/ichor_serializer_benchmark multi threaded boost.JSON ran for 1,122,450 µs with 5,664,768 peak memory usage 1,069,089,937 B/s
../absl_std/ichor_start_benchmark single threaded ran for 2,583,465 µs with 36,003,840 peak memory usage
../absl_std/ichor_start_benchmark multi threaded ran for 2,817,231 µs with 282,361,856 peak memory usage
../absl_std/ichor_start_stop_benchmark single threaded ran for 1,502,495 µs with 2,142,208 peak memory usage 665,559 start & stop /s
../absl_std/ichor_start_stop_benchmark multi threaded ran for 2,410,041 µs with 5,509,120 peak memory usage 3,319,445 start & stop /s
../absl_mimalloc/ichor_coroutine_benchmark single threaded ran for 443,501 µs with 4,358,144 peak memory usage 11,273,931 coroutines/s
../absl_mimalloc/ichor_coroutine_benchmark multi threaded ran for 1,187,087 µs with 6,836,224 peak memory usage 33,695,929 coroutines/s
../absl_mimalloc/ichor_event_benchmark single threaded ran for 458,684 µs with 414,511,104 peak memory usage 10,900,750 events/s
../absl_mimalloc/ichor_event_benchmark multi threaded ran for 836,063 µs with 3,352,096,768 peak memory usage 47,843,284 events/s
../absl_mimalloc/ichor_serializer_benchmark single threaded rapidjson ran for 574,965 µs with 4,386,816 peak memory usage 269,581,626 B/s
../absl_mimalloc/ichor_serializer_benchmark multi threaded rapidjson ran for 740,182 µs with 7,217,152 peak memory usage 1,675,263,651 B/s
../absl_mimalloc/ichor_serializer_benchmark single threaded boost.JSON ran for 1,099,887 µs with 7,217,152 peak memory usage 136,377,646 B/s
../absl_mimalloc/ichor_serializer_benchmark multi threaded boost.JSON ran for 1,139,676 µs with 7,307,264 peak memory usage 1,052,930,832 B/s
../absl_mimalloc/ichor_start_benchmark single threaded ran for 2,240,716 µs with 37,253,120 peak memory usage
../absl_mimalloc/ichor_start_benchmark multi threaded ran for 2,497,607 µs with 295,014,400 peak memory usage
../absl_mimalloc/ichor_start_stop_benchmark single threaded ran for 1,198,513 µs with 4,382,720 peak memory usage 834,367 start & stop /s
../absl_mimalloc/ichor_start_stop_benchmark multi threaded ran for 2,091,859 µs with 6,979,584 peak memory usage 3,824,349 start & stop /s
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

## HTTP benchmark

```bash
$ mkdir build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_ABSEIL=ON -DICHOR_USE_BOOST_BEAST=ON .. && ninja
$ ../bin/ichor_pong_example -s &
$ wrk -c 2 -t1 -d 60s --latency -s ./test.lua "http://localhost:8001/ping"
Running 1m test @ http://localhost:8001/ping
  1 threads and 2 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    29.54us    7.47us   1.84ms   95.90%
    Req/Sec    66.72k     5.59k  103.37k    71.55%
  Latency Distribution
     50%   29.00us
     75%   30.00us
     90%   33.00us
     99%   40.00us
  3990488 requests in 1.00m, 395.79MB read
Requests/sec:  66397.50
Transfer/sec:      6.59MB
```
```bash
$ wrk -c 400 -t2 -d 60s --latency -s ./test.lua "http://localhost:8001/ping"
Running 1m test @ http://localhost:8001/ping
  2 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.03ms  662.78us  21.06ms   59.24%
    Req/Sec    40.00k     5.31k   51.98k    86.33%
  Latency Distribution
     50%    4.66ms
     75%    5.70ms
     90%    5.74ms
     99%    5.89ms
  4775393 requests in 1.00m, 473.63MB read
Requests/sec:  79573.33
Transfer/sec:      7.89MB
```
```bash
$ wrk -c 2000 -t4 -d 60s --latency -s ./test.lua "http://localhost:8001/ping"
Running 1m test @ http://localhost:8001/ping
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    13.52ms    2.08ms  30.17ms   69.18%
    Req/Sec    18.90k     2.93k   23.35k    49.88%
  Latency Distribution
     50%   11.86ms
     75%   15.64ms
     90%   16.39ms
     99%   16.54ms
  4512207 requests in 1.00m, 447.53MB read
  Socket errors: connect 983, read 0, write 0, timeout 0
Requests/sec:  75168.09
Transfer/sec:      7.46MB
```
```bash
top -H -d2
  14625       20   0  179804  12232  10364 R  99,9   0,0   1:57.51 HttpCon #3
  14624       20   0  179804  12232  10364 S  49,5   0,0   0:41.34 DepMan #0
  15809       20   0  251068   4684   3992 S  26,2   0,0   0:05.13 wrk
  15808       20   0  251068   4684   3992 S  25,7   0,0   0:06.16 wrk
```
```bash
$ cat test.lua
wrk.method = "POST"

-- post json data
wrk.body = '{"sequence": 2}'
wrk.headers['Content-Type'] = "application/json"
```

These benchmarks currently lead to the characteristics:
* creating services with dependencies overhead is likely O(N²).
* Starting services, stopping services overhead is likely O(N)
* event handling overhead is amortized O(1)
* Creating more threads is not 100% linearizable in all cases (pure event creation/handling seems to be, otherwise not really)
* Latency spikes while scheduling in user-space is in the order of 100's of microseconds, lower if a proper [realtime tuning guide](https://rigtorp.se/low-latency-guide/) is used (except if the queue goes empty and is forced to sleep)

Help with improving memory usage and the O(N²) behaviour would be appreciated.
