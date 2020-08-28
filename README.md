# What is this?

Cppelix is a rewrite of [Celix](https://github.com/apache/celix) in C++20, which itself is a port of [Felix](https://github.com/apache/felix).

### OSGI?

[OSGI](https://www.osgi.org/) is a modular approach aiming to make refactoring of code easier.
The framework [boasts](https://www.osgi.org/developer/benefits-of-using-osgi/) more benefits, but the Cppelix author's personal opinion is that the switch to interface-oriented programming mainly provides a way to reduce the size of refactors, such as using a different pubsub provider not requiring all the business logic classes to change at all.

# Why a rewrite?

Celix was started in 2010, when the newer standards of C++ weren't released yet. Over the years usage has increased and so have feature requests. A sizeable amount of usage in recent years has been in these newer C++ standards, but with Celix being in C this tends to pollute the code of users.

### Current desired advantages over Celix (regardless of using C++ wrapper or C core)

* Less magic configuration
    * code as configuration, as much as possible bundled in one place
* More type-safety than a C++ wrapper around Celix can offer
* Less multi-threading to prevent data races and similar issues
    * Use of an event loop
    * Where multi-threading is desired, provide easy to use abstractions to prevent issues
* Performance-oriented design in all-parts of the framework / making it easy to get high performance and low latency
* Fully utilise OOP, RAII and C++20 Concepts to steer users to using the framework correctly
* Implement business logic in the least amount of code possible 
* Hopefully this culminates and less error-prone code and better time to market

# Why C++20? It's not even fully implemented?

To consider putting in the effort to port a 100,000+ loc library, it has to be worth the effort. To figure out if it is, this port aims to use the most advanced C++ available at the time of writing, showcasing the benefit in the long-term. 

# Why not Rust?

There are not enough Rust users in industry to warrant such a transition. Feel free to create your own implementation of the OSGI standard in Rust though, shoot me a message if you need any help understanding the standard.

# Install

Ubuntu:

```
sudo apt install build-essential autoconf libtool pkg-config libgrpc++-dev libprotobuf-dev
```

# Unsupported features in Cppelix

With this rewrite comes an opportunity to remove some features that are not used often in Celix:

* Zip or Jar-based bundles/containers
* Dynamic loading of bundles
* Descriptor based serialization
* C support
    * No effort will be made to provided a C compatible layer
    * But, a [celix shim library](https://github.com/volt-software/cppelix-celix-shim) is in the works
* ...

# Todo

* Support parsing filter (LDAP) strings to type-safe filter currently used
* expand etcd support, currently only simply put/get supported
* Pubsub compatibility with celix
    * ZMQ
    * TCP
    * TCP iovec/zerocopy
    * websocket
    * UDP?
* Pubsub new interfaces
    * Kafka? Pulsar? Something COTS based.
* RSA compatibility with celix
* ShellCommand
* ...

# Requirements to build Cppelix

* Gcc 10 or newer
* Clang 10 or newer
* Cmake 3.12 or newer

# Preleminary benchmark results
These benchmarks are mainly used to identify bottlenecks, not to showcase the performance of the framework. Comparisons between Cppelix, Celix and Felix will be made at a later date.

Setup: AMD 3900X, 3600MHz@CL17 RAM, ubuntu 20.04
* Start best case scenario: 10,000 starts with dependency in ~690ms
* Start/stop best case scenario: 1,000,000 start/stops with dependency in ~800ms
* Serialization: Rapidjson: 1,000,000 messages serialized & deserialized in ~350 ms

# Support

Feel free to make issues/pull requests and I'm sometimes online on Discord, though there's no 24/7 support: https://discord.gg/r9BtesB

# License

Cppelix is licensed under the MIT license.
