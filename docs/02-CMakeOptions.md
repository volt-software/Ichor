# CMake Options

## BUILD_EXAMPLES

Turned on by default. Builds the examples in the [examples directory](../examples). Takes some time to compile.

## BUILD_BENCHMARKS

Turned on by default. Builds the benchmarks in the [benchmarks directory](../benchmarks).

## BUILD_TESTS

Turned on by default. Builds the tests in the [test directory](../test).

## ICHOR_ENABLE_INTERNAL_DEBUGGING

Enables verbose logging at various points in the Ichor framework. Recommended only when trying to figure out if you've encountered a bug in Ichor.

## ICHOR_ENABLE_INTERNAL_COROUTINE_DEBUGGING

Enables verbose logging for coroutines in the Ichor framework. Recommended only when trying to figure out if you've encountered a bug in Ichor.

## ICHOR_USE_SANITIZERS

Turned on by default. Compiles everything (including the optionally enabled submodules) with the AddressSanitizer and UndefinedBehaviourSanitizer. Recommended when debugging. Cannot be combined with the ThreadSanitizer

## ICHOR_USE_THREAD_SANITIZER

Compiles everything (including the optionally enabled submodules) with the ThreadSanitizer. Recommended when debugging. Cannot be combined with the AddressSanitizer. Note that compiler bugs may exist, like for [gcc](https://gcc.gnu.org/bugzilla//show_bug.cgi?id=101978).

## ICHOR_USE_UGLY_HACK_EXCEPTION_CATCHING

Debugging Boost.Asio and Boost.BEAST is difficult, this hack enables catching exceptions directly at the source, rather than at the last point of being rethrown. Requires gcc.

## ICHOR_REMOVE_SOURCE_NAMES

Ichor's logging macros by default adds the current filename and line number to each log statement. This option disables that.

## ICHOR_USE_HARDENING

Turned on by default. Uses compiler-specific flags which add stack protection and similar features, as well as adding safety checks in Ichor itself.

## ICHOR_USE_SPDLOG (optional dependency)

Enables the use of the [spdlog submodule](../external/spdlog). If examples or benchmarks are enabled, these then use the spdlog variants of loggers instead of cout.

## ICHOR_USE_ETCD (optional dependency)

Used for the etcd examples and benchmarks. May not work, not used much.

## ICHOR_USE_BOOST_BEAST (optional dependency)

Requires Boost.BEAST to be installed as a system dependency (version >= 1.70). Used for websocket and http server/client implementations. All examples require `ICHOR_SERIALIZATION_FRAMEWORK` to be set as well.

## ICHOR_USE_MOLD (optional compile-time dependency)

For clang compilers, add the `-fuse-ld=mold` linker flag. This speeds up the linking stage.
Usage with gcc 12+ is technically possible, but throws off [Catch2 unit test detection](https://github.com/catchorg/Catch2/issues/2507).

## ICHOR_USE_SDEVENT (optional dependency)

Enables the use of the [sdevent event queue](../include/ichor/event_queues/SdeventQueue.h). Requires having sdevent headers and libraries installed on your system to compile.

## ICHOR_USE_ABSEIL (optional dependency)

Enables the use of the abseil containers in Ichor. Requires having abseil headers and libraries installed on your system to compile.

## ICHOR_USE_HIREDIS (optional dependency)

Enables the use of the Hiredis library and exposes the Redis service. Requires having Hiredis libraries and headers installed on your system to compile.

## ICHOR_USE_BACKWARD (optional dependency)

Adds and installs the backward header. Useful for showing backtraces on-demand or if the program crashes.

## ICHOR_DISABLE_RTTI

Turned on by default. Disables `dynamic_cast<>()` in most cases as well as `typeid`. Ichor is an opinionated piece of software and we strongly encourage you to disable RTTI. We believe `dynamic_cast<>()` is wrong in almost all instances. Virtual methods and double dispatch should be used instead. If, however, you really want to use RTTI, use this option to re-enable it.

## ICHOR_USE_MIMALLOC

If `ICHOR_USE_SANITIZERS` is turned OFF, Ichor by default compiles itself with mimalloc, speeding up the runtime a lot and reducing peak memory usage.

## ICHOR_USE_SYSTEM_MIMALLOC

If `ICHOR_USE_MIMALLOC` is turned ON, this option can be used to use the system installed mimalloc instead of the Ichor provided one.

## ICHOR_MUSL

This option results in statically linking libgcc and libstdc++ as well as turning off some glibc-specific pthread usages.

## ICHOR_AARCH64

Control flow protection is not available on Aarch64, enable this to allow (cross-)compiling for that architecture.
