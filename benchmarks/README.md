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

Some HTTP benchmarks with different frameworks

### Ichor + Boost.BEAST
```bash
$ mkdir build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_ABSEIL=ON -DICHOR_USE_BOOST_BEAST=ON .. && ninja
$ ulimit -n 65535 && ../bin/ichor_pong_example -s &
$ wrk -c2 -t1 -d60s --latency -s ../benchmarks/wrk.lua http://localhost:8001/ping
Running 1m test @ http://localhost:8001/ping
  1 threads and 2 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    29.17us    4.49us   1.36ms   92.57%
    Req/Sec    67.51k     3.69k   80.86k    77.37%
  Latency Distribution
     50%   29.00us
     75%   30.00us
     90%   32.00us
     99%   39.00us
  4038387 requests in 1.00m, 427.49MB read
Requests/sec:  67194.72
Transfer/sec:      7.11MB
```
```bash
$ wrk -c400 -t2 -d60s --latency -s ../benchmarks/wrk.lua http://localhost:8001/ping
Running 1m test @ http://localhost:8001/ping
  2 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.59ms  256.25us   9.76ms   79.41%
    Req/Sec    43.74k     2.46k   58.80k    92.17%
  Latency Distribution
     50%    4.61ms
     75%    4.66ms
     90%    4.88ms
     99%    4.96ms
  5223768 requests in 1.00m, 552.98MB read
Requests/sec:  87048.80
Transfer/sec:      9.21MB
```
```bash
$ wrk -c20000 -t4 -d60s --latency -s ../benchmarks/wrk.lua http://localhost:8001/ping
Running 1m test @ http://localhost:8001/ping
  4 threads and 20000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   270.41ms   21.43ms 879.87ms   95.93%
    Req/Sec    18.38k     2.01k   24.18k    72.12%
  Latency Distribution
     50%  272.47ms
     75%  274.42ms
     90%  276.31ms
     99%  281.80ms
  4382499 requests in 1.00m, 463.92MB read
Requests/sec:  72971.55
Transfer/sec:      7.72MB
```
```bash
top -H -d2
    PID       PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND 
  67529       20   0 2653500 307896   5356 R  99,9   0,5   0:04.35 HttpCon #3                                                                                                                                                                                                                                                                                          
  67528       20   0 2653500 307896   5356 S  33,3   0,5   0:01.43 DepMan #0                                                                                                                                                                                                                                                                                           
  67534       20   0  573972  99932   3940 S  22,9   0,2   0:01.04 wrk                                                                                                                                                                                                                                                                                                 
  67533       20   0  573972  99932   3940 S  20,9   0,2   0:00.95 wrk                                                                                                                                                                                                                                                                                                 
  67535       20   0  573972  99932   3940 R  20,4   0,2   0:00.92 wrk                                                                                                                                                                                                                                                                                                 
  67536       20   0  573972  99932   3940 S  20,4   0,2   0:00.93 wrk  
```
```bash
$ cat wrk.lua
wrk.method = "POST"

-- post json data
wrk.body = '{"sequence": 2}'
wrk.headers['Content-Type'] = "application/json"
```

### Pure Boost.BEAST (http_server_fast)

See [beast_benchmark](beast_benchmark)

```bash
$ wrk -c2 -t1 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:6000/ping
Running 10s test @ http://localhost:6000/ping
  1 threads and 2 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    20.80us   13.58us 413.00us   97.16%
    Req/Sec    41.63k     6.48k   54.91k    73.27%
  Latency Distribution
     50%   17.00us
     75%   30.00us
     90%   33.00us
     99%   50.00us
  418099 requests in 10.10s, 39.87MB read
  Socket errors: connect 0, read 418099, write 0, timeout 0
Requests/sec:  41397.00
Transfer/sec:      3.95MB
```
```bash
$ wrk -c400 -t2 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:6000/ping
Running 10s test @ http://localhost:6000/ping
  2 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.33ms  563.26us  10.10ms   77.69%
    Req/Sec    37.23k     2.68k   41.64k    74.50%
  Latency Distribution
     50%    5.20ms
     75%    5.39ms
     90%    6.05ms
     99%    6.45ms
  740671 requests in 10.01s, 70.64MB read
  Socket errors: connect 0, read 740671, write 0, timeout 0
Requests/sec:  73975.13
Transfer/sec:      7.05MB
```
```bash
$ wrk -c20000 -t4 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:6000/ping
Running 10s test @ http://localhost:6000/ping
  4 threads and 20000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    95.10ms  141.27ms   1.73s    90.00%
    Req/Sec    15.01k     8.27k   28.45k    62.82%
  Latency Distribution
     50%   51.59ms
     75%   52.46ms
     90%  255.04ms
     99%  709.07ms
  374703 requests in 10.04s, 35.73MB read
  Socket errors: connect 0, read 374703, write 0, timeout 223
Requests/sec:  37311.37
Transfer/sec:      3.56MB
```

### ExpressJS
```bash
$ cat express.js 
var express = require('express');
var app = express();

app.use(express.json())

app.post('/ping', function(req, res){
  res.json(req.body)
});

app.listen(3000);
console.log('Listening on port 3000');
```
```bash
$ wrk -c 2 -t1 -d 10s --latency -s ./wrk.lua http://localhost:3000/ping
Running 10s test @ http://localhost:3000/ping
  1 threads and 2 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   155.23us  183.51us   7.54ms   95.86%
    Req/Sec    15.18k     1.65k   16.62k    94.06%
  Latency Distribution
     50%  121.00us
     75%  132.00us
     90%  174.00us
     99%    1.04ms
  152519 requests in 10.10s, 36.07MB read
Requests/sec:  15101.66
Transfer/sec:      3.57MB
```
```bash
$ wrk -c20000 -t4 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:3000/ping
Running 10s test @ http://localhost:3000/ping
  4 threads and 20000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   462.34ms  168.32ms   1.38s    60.19%
    Req/Sec     3.34k     1.02k    7.21k    75.76%
  Latency Distribution
     50%  491.98ms
     75%  524.07ms
     90%  668.50ms
     99%    1.07s 
  131805 requests in 10.05s, 31.17MB read
  Socket errors: connect 0, read 0, write 0, timeout 3652
Requests/sec:  13118.38
Transfer/sec:      3.10MB
```

### Drogon

```bash
$ cmake -DCMAKE_BUILD_TYPE=Release -DUSE_COROUTINE=ON .. #building drogon
$ cat drogon.cpp
#include <drogon/drogon.h>

using namespace drogon;
int main()
{
    app()
        .setLogPath("./")
        .setLogLevel(trantor::Logger::kWarn)
        .addListener("0.0.0.0", 7770)
        .setThreadNum(1);

    app().registerHandler("/ping",
        [](const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) {
            auto reqjson = req->getJsonObject();
            Json::Value json;
            json["sequence"]=(*reqjson)["sequence"].asInt64();
            auto resp=HttpResponse::newHttpJsonResponse(json);
            callback(resp);
        },
        {Post});

    app().run();
}
```
```bash
$ wrk -c2 -t1 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:7770/ping
Running 10s test @ http://localhost:7770/ping
  1 threads and 2 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    15.48us   14.26us   1.28ms   99.32%
    Req/Sec   133.01k    17.28k  194.91k    91.09%
  Latency Distribution
     50%   15.00us
     75%   15.00us
     90%   16.00us
     99%   24.00us
  1336011 requests in 10.10s, 202.58MB read
Requests/sec: 132285.77
Transfer/sec:     20.06MB
```
```bash
$ wrk -c400 -t2 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:7770/ping
Running 10s test @ http://localhost:7770/ping
  2 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.00ms  140.69us   6.12ms   92.01%
    Req/Sec    66.88k     1.88k   68.83k    98.00%
  Latency Distribution
     50%    2.99ms
     75%    3.01ms
     90%    3.08ms
     99%    3.33ms
  1330911 requests in 10.01s, 201.81MB read
Requests/sec: 132936.53
Transfer/sec:     20.16MB
```
```bash
$ wrk -c20000 -t4 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:7770/ping
Running 10s test @ http://localhost:7770/ping
  4 threads and 20000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    64.98ms   11.16ms   1.98s    99.19%
    Req/Sec    30.07k     8.92k   66.73k    76.26%
  Latency Distribution
     50%   65.11ms
     75%   65.71ms
     90%   66.18ms
     99%   68.04ms
  1185278 requests in 10.07s, 179.73MB read
  Socket errors: connect 0, read 2094, write 0, timeout 120
Requests/sec: 117676.17
Transfer/sec:     17.84MB
```

# Golang fasthtpp
```bash
$ cat main.go
package main

import (
	"flag"
	"log"

	"github.com/goccy/go-json"
	"github.com/valyala/fasthttp"
)

var (
	addr     = flag.String("addr", ":9000", "TCP address to listen to")
)

type Sequence struct {
	Sequence uint64
}

func main() {
	flag.Parse()

	h := requestHandler

	if err := fasthttp.ListenAndServe(*addr, h); err != nil {
		log.Fatalf("Error in ListenAndServe: %v", err)
	}
}

func requestHandler(ctx *fasthttp.RequestCtx) {
	ctx.SetContentType("application/json; charset=utf8")
	var seq Sequence
	json.Unmarshal(ctx.Request.Body(), &seq)
	data, _ := json.Marshal(seq)
	ctx.SetBody(data)
}
```
```bash
$ GOMAXPROCS=1 ./fasthttp_bench &
$ wrk -c2 -t1 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:9000/ping
Running 10s test @ http://localhost:9000/ping
  1 threads and 2 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    12.43us   11.95us   1.04ms   99.32%
    Req/Sec   166.51k     6.24k  172.03k    95.05%
  Latency Distribution
     50%   12.00us
     75%   12.00us
     90%   13.00us
     99%   18.00us
  1673959 requests in 10.10s, 245.85MB read
Requests/sec: 165747.41
Transfer/sec:     24.34MB
```
```bash
$ wrk -c400 -t2 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:9000/ping
Running 10s test @ http://localhost:9000/ping
  2 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.36ms  582.61us  17.77ms   75.14%
    Req/Sec    85.68k     5.74k  106.14k    88.00%
  Latency Distribution
     50%    2.32ms
     75%    2.71ms
     90%    2.98ms
     99%    3.46ms
  1704231 requests in 10.02s, 250.29MB read
Requests/sec: 170028.45
Transfer/sec:     24.97MB
```
```bash
$ wrk -c20000 -t4 -d10s --latency -s ../benchmarks/wrk.lua http://localhost:9000/ping
Running 10s test @ http://localhost:9000/ping
  4 threads and 20000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   159.97ms   48.79ms 499.17ms   94.48%
    Req/Sec    31.58k     6.40k   38.00k    90.66%
  Latency Distribution
     50%  150.91ms
     75%  151.65ms
     90%  152.37ms
     99%  467.35ms
  1243939 requests in 10.05s, 182.69MB read
Requests/sec: 123741.19
Transfer/sec:     18.17MB
```
# Conclusions?

These benchmarks currently lead to the following characteristics for Ichor:
* creating services with dependencies overhead is likely O(N²).
* Starting services, stopping services overhead is likely O(N)
* event handling overhead is amortized O(1)
* Creating more threads is not 100% linearizable in all cases (pure event creation/handling seems to be, otherwise not really)
* Latency spikes while scheduling in user-space is in the order of 100's of microseconds, lower if a proper [realtime tuning guide](https://rigtorp.se/low-latency-guide/) is used (except if the queue goes empty and is forced to sleep)

Help with improving memory usage and the O(N²) behaviour would be appreciated.

Also, Golang's fasthttp + go-json is really fast. Together with Ichor + Boost.BEAST, they are the only two tested solutions here that can handle 20,000 connections without errors. 
Although Ichor "cheats" in the sense that there is an extra thread doing the user logic separate from the networking thread, where other implementations had to do everything with 1 thread.   

# Possible improvements

* Linux-only: Implement I/O uring event loop for decreased syscalls
* Use libuv and modify hiredis implementation to use that instead of polling @ 1000Hz
