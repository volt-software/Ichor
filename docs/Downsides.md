# Downsides

Any and all tech choices come with pros and cons. Anyone who only tells you about the benefits of a piece of technology is withholding important information. Here are some for Ichor:

* Inability to have custom defined constructors (could maybe be improved?)
* Using co_await messes up lifetime management. E.g. using a timer, using co_await inside the handler and then changing the timer after the await may result in UB.
* Using anything getManager()-based in the no-arguments service constructor segfaults, as it is not available at that point.
* Creating a service that has its own thread and making interaction between Ichor and that thread thread-safe is difficult. It took a couple of iterations to get a safe http implementation.
* Debugging issues in Ichor itself is painful. Going outside of the existing best practices for Ichor will lead to painful debugging.
* Ichor relies on bleeding edge C++, which results in having to deal with compiler bugs.
* Ichor uses a lot of memory allocations. Mimalloc reduces the impact, but the most egregious example is Ichor's boost implementation: abstracting away http requests/responses requires a lot of memcpy/memset/moves of memory that would otherwise not be necessary.

Most of these can worked around, institutional knowledge can be built or can be attributed to downsides of C++ itself. Use this knowledge to make a decision yourself: is Ichor worth it for your use-case?
