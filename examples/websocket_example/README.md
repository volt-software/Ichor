# Websocket Example

This example starts up a websocket server and a websocket client that connects to said server. The client sends a JSON message to which the server responds and then quits.

This demonstrates the following concepts:
* Defining and using serializers
* How to set up a websocket server
* How to set up a websocket client using the `ClientFactory`
* Using the advanced parameter `Spinlock` in the `MultimapQueue`.

## Command Line Arguments

```shell
-a, --address, "Address to bind to, e.g. 127.0.0.1"
-v, --verbosity, "Increase logging for each -v"
-t, --threads, "Number of threads to use for I/O, default: 1"
-p, --spinlock, "Spinlock 10ms before going to sleep, improves latency in high workload cases at the expense of CPU usage"
-s, --silent, "No output"
```

