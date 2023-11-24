# Optional Dependency Example

This example shows how to use optional dependencies, including multiple of the same type. 

This demonstrates the following concepts:
* Requesting optional dependencies, separate from the lifecycle of the requesting service
  * Normally, a service does not start until all of its required dependencies are met. Optional dependencies can come and go without starting/stopping the requesting service
* How to handle multiple dependencies of the same type
