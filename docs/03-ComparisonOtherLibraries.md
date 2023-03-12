# Comparison to other Dependency Injection Libraries (WIP)

Comparison made to the best of my knowledge

|                        | Ichor   | Hypodermic | Boost.DI                         | Google.Fruit                   | CppMicroservices |
|------------------------|---------|------------|----------------------------------|--------------------------------|------------------|
| Runtime/compile-time   | Runtime | Runtime    | Compile time (with some runtime) | Compile time (with some runtime) | Runtime          |
| Constructor Injection  | Yes     | Yes        | Yes                              | Yes                            | No               |
| Per-instance resolving | Yes     | No         | No                               | No                             | Yes              |
| Factories              | Yes     | No?        | Partial                          | Partial                        | Yes              |
| Coroutines             | Yes     | No         | No                               | No                             | No               |
| Optional dependencies  | Yes     | No         | No                               | No                             | Yes?             |
| Per-instance lifetime  | Yes     | No         | No                               | No                             | Yes              |
| Minimum c++            | 20      | 11         | 14                               | 11                             | 17               |

Obviously, Ichor is more than just Dependency Injection, as the following are not present in the other libraries:

* Event loop
* Various implementations such as HTTP server, Redis client, Loggers
* ...
