# What is this?

Cppelix is a rewrite of [Celix](https://github.com/apache/celix) in C++20, which itself is a port of [Felix](https://github.com/apache/felix).

# Why?

Celix was started in 2010, when the newer standards of C++ weren't released yet. Over the years usage has increased and so have feature requests. A sizeable amount of usage in recent years has been in these newer C++ standards, but with Celix being in C this tends to pollute the code of users.

# Why C++20? It's not even released?

To consider putting in the effort to port a 100,000+ loc library, it has to be worth the effort. To figure out if it is, this port aims to use the most advanced C++ available at the time of writing, showcasing the benefit in the long-term. 

# Why not Rust?

There are not enough Rust users in industry to warrant such a transition. Feel free to create your own implementation of the OSGI standard in Rust though, shoot me a message if you need any help understanding the standard.

# Unsupported features in Cppelix

With this rewrite comes an opportunity to remove some features that are not used often in Celix:

* Zip or Jar-based bundles/containers
* ...

# Requirements to build Cppelix

* Gcc 10 or newer
* Clang 10 or newer
* Cmake 3.12 or newer

# License

Cppelix is licensed under the MIT license.
