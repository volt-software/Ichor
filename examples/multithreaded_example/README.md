# Multithreaded Example

This example shows how to use multiple DependencyManagers on multiple threads and how to communicate safely between the two.

This demonstrates the following concepts:
* Creating multiple queues and DependencyManagers
* How to use the `CommunicationChannel` to communicate between different queues
* How to tell a different DependencyManager/queue to quit
