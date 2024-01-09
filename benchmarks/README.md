# Preliminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Proper throughput and latency benchmarks are TBD.

Setup: AMD 7950X, 6000MHz@CL38 RAM, ubuntu 22.04, gcc 11.3.
Listed numbers are for the multi-threaded runs where contention is likely.

| Compile Options        |     coroutines      |                  events |                   start |         start & stop | 
|------------------------|:-------------------:|------------------------:|------------------------:|---------------------:|
| std alloc              | 929,438 µs<br/>6MB  | 1,790,241 µs<br/>5765MB | 20,659,654 µs<br/>411MB | 1,963,093 µs<br/>6MB |
| mimalloc               | 884,401 µs<br/>7MB  |   818,337 µs<br/>4361MB | 20,633,802 µs<br/>408MB | 1,629,301 µs<br/>7MB |
| mimalloc, no hardening | 768,800 µs<br/>7MB  |   800,319 µs<br/>4480MB | 20,573,036 µs<br/>407MB | 1,522,690 µs<br/>7MB |

Raspberry Pi Model 4B, Debian 12 Bookworm, gcc 12.2.
Listed numbers are for the multi-threaded runs where contention is likely. Note that the benchmarks use 8 threads and the Pi Model 4B only has 4 cores.

| Compile Options        |      coroutines       | single-threaded events |                    start |          start & stop | 
|------------------------|:---------------------:|-----------------------:|-------------------------:|----------------------:|
| mimalloc               | 11,166,479 µs<br/>6MB | 6,249,102 µs<br/>565MB | 160,831,445 µs<br/>407MB | 25,342,246 µs<br/>6MB |

The most interesting observation here is that disabling hardening does not bring any performance gains larger than run-to-run variance.

Detailed data 7950X:
```text
../std_std/ichor_coroutine_benchmark single threaded ran for 540,057 µs with 6,029,312 peak memory usage 9,258,281 coroutines/s
../std_std/ichor_coroutine_benchmark multi threaded ran for 929,438 µs with 6,291,456 peak memory usage 43,036,759 coroutines/s
../std_std/ichor_event_benchmark single threaded ran for 1,152,095 µs with 645,922,816 peak memory usage 4,339,919 events/s
../std_std/ichor_event_benchmark multi threaded ran for 1,790,241 µs with 5,764,808,704 peak memory usage 22,343,360 events/s
../std_std/ichor_serializer_benchmark single threaded glaze ran for 177,602 µs with 6,029,312 peak memory usage 872 MB/s
../std_std/ichor_serializer_benchmark multi threaded glaze ran for 187,264 µs with 6,291,456 peak memory usage 6,621 MB/s
../std_std/ichor_start_benchmark single threaded advanced injection ran for 3,233,842 µs with 41,537,536 peak memory usage
../std_std/ichor_start_benchmark multi threaded advanced injection ran for 20,744,909 µs with 323,313,664 peak memory usage
../std_std/ichor_start_benchmark single threaded constructor injection ran for 3,539,603 µs with 323,313,664 peak memory usage
../std_std/ichor_start_benchmark multi threaded constructor injection ran for 20,659,654 µs with 411,222,016 peak memory usage
../std_std/ichor_start_stop_benchmark single threaded ran for 1,330,531 µs with 6,029,312 peak memory usage 751,579 start & stop /s
../std_std/ichor_start_stop_benchmark multi threaded ran for 1,963,093 µs with 6,291,456 peak memory usage 4,075,201 start & stop /s
../std_mimalloc/ichor_coroutine_benchmark single threaded ran for 392,161 µs with 6,815,744 peak memory usage 12,749,865 coroutines/s
../std_mimalloc/ichor_coroutine_benchmark multi threaded ran for 884,401 µs with 6,815,744 peak memory usage 45,228,352 coroutines/s
../std_mimalloc/ichor_event_benchmark single threaded ran for 613,459 µs with 568,590,336 peak memory usage 8,150,503 events/s
../std_mimalloc/ichor_event_benchmark multi threaded ran for 818,337 µs with 4,360,503,296 peak memory usage 48,879,618 events/s
../std_mimalloc/ichor_serializer_benchmark single threaded glaze ran for 164,604 µs with 6,815,744 peak memory usage 941 MB/s
../std_mimalloc/ichor_serializer_benchmark multi threaded glaze ran for 169,835 µs with 6,815,744 peak memory usage 7,301 MB/s
../std_mimalloc/ichor_start_benchmark single threaded advanced injection ran for 3,199,495 µs with 40,370,176 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded advanced injection ran for 20,534,538 µs with 310,624,256 peak memory usage
../std_mimalloc/ichor_start_benchmark single threaded constructor injection ran for 3,447,383 µs with 310,624,256 peak memory usage
../std_mimalloc/ichor_start_benchmark multi threaded constructor injection ran for 20,633,802 µs with 408,350,720 peak memory usage
../std_mimalloc/ichor_start_stop_benchmark single threaded ran for 1,020,544 µs with 6,815,744 peak memory usage 979,869 start & stop /s
../std_mimalloc/ichor_start_stop_benchmark multi threaded ran for 1,629,301 µs with 6,815,744 peak memory usage 4,910,081 start & stop /s
../std_no_hardening/ichor_coroutine_benchmark single threaded ran for 354,885 µs with 6,815,744 peak memory usage 14,089,071 coroutines/s
../std_no_hardening/ichor_coroutine_benchmark multi threaded ran for 768,800 µs with 6,815,744 peak memory usage 52,029,136 coroutines/s
../std_no_hardening/ichor_event_benchmark single threaded ran for 587,957 µs with 568,590,336 peak memory usage 8,504,023 events/s
../std_no_hardening/ichor_event_benchmark multi threaded ran for 800,319 µs with 4,479,516,672 peak memory usage 49,980,070 events/s
../std_no_hardening/ichor_serializer_benchmark single threaded glaze ran for 150,209 µs with 6,815,744 peak memory usage 1,031 MB/s
../std_no_hardening/ichor_serializer_benchmark multi threaded glaze ran for 168,869 µs with 6,815,744 peak memory usage 7,342 MB/s
../std_no_hardening/ichor_start_benchmark single threaded advanced injection ran for 3,147,797 µs with 40,370,176 peak memory usage
../std_no_hardening/ichor_start_benchmark multi threaded advanced injection ran for 20,622,724 µs with 310,763,520 peak memory usage
../std_no_hardening/ichor_start_benchmark single threaded constructor injection ran for 3,370,720 µs with 322,035,712 peak memory usage
../std_no_hardening/ichor_start_benchmark multi threaded constructor injection ran for 20,573,036 µs with 406,839,296 peak memory usage
../std_no_hardening/ichor_start_stop_benchmark single threaded ran for 914,654 µs with 6,815,744 peak memory usage 1,093,309 start & stop /s
../std_no_hardening/ichor_start_stop_benchmark multi threaded ran for 1,522,690 µs with 6,815,744 peak memory usage 5,253,859 start & stop /s
```

Detailed data Raspberry Pi 4B:
```text
../bin/ichor_coroutine_benchmark single threaded ran for 5,260,358 µs with 3,641,344 peak memory usage 950,505 coroutines/s
../bin/ichor_coroutine_benchmark multi threaded ran for 11,166,479 µs with 5,623,808 peak memory usage 3,582,149 coroutines/s
../bin/ichor_event_benchmark single threaded ran for 6,249,102 µs with 565,805,056 peak memory usage 800,114 events/s
# multithreaded event benchmark skipped due to not having enough memory
../bin/ichor_serializer_benchmark single threaded glaze ran for 1,578,962 µs with 2,486,272 peak memory usage 98 MB/s
../bin/ichor_serializer_benchmark multi threaded glaze ran for 3,083,435 µs with 5,722,112 peak memory usage 402 MB/s
../bin/ichor_start_benchmark single threaded advanced injection ran for 20,680,096 µs with 37,597,184 peak memory usage
../bin/ichor_start_benchmark multi threaded advanced injection ran for 162,277,929 µs with 308,760,576 peak memory usage
../bin/ichor_start_benchmark single threaded constructor injection ran for 20,891,884 µs with 308,760,576 peak memory usage
../bin/ichor_start_benchmark multi threaded constructor injection ran for 160,831,445 µs with 407,212,032 peak memory usage
../bin/ichor_start_stop_benchmark single threaded ran for 12,108,020 µs with 4,055,040 peak memory usage 82,589 start & stop /s
../bin/ichor_start_stop_benchmark multi threaded ran for 25,342,246 µs with 5,763,072 peak memory usage 315,678 start & stop /s
```

Realtime example on a vanilla linux:
```text
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
$ mkdir build && cd build && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DICHOR_ARCH_OPTIMIZATION=X86_64_AVX2 -DICHOR_USE_BOOST_BEAST=ON -DICHOR_USE_SANITIZERS=OFF .. && ninja
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
