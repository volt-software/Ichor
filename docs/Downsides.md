# Downsides

Any and all tech choices come with pros and cons. Here are some for Ichor:

* Inability to have custom defined constructors
* Using co_await messes up lifetime management
* Using anything getManager() bases in the no-arguments service constructor segfaults, as it is not available at that point.
* Creating a service that has its own thread and making interaction between Ichor and that thread thread-safe is difficult.
* Debugging issues in (core) Ichor is painful. Going outside of the existing best practices for Ichor will lead to painful debugging.