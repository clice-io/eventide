# eventide

Both [clice](https://github.com/clice-io/clice.git) and [catter](https://github.com/clice-io/catter.git) need to communicate with other processes. We've decided to use [libuv](https://github.com/libuv/libuv) as our event library. It's lightweight, cross-platform, and powerful, and it covers all our needs.

However, the raw C API can be hard to use, so we're using [C++20 coroutines](https://en.cppreference.com/w/cpp/language/coroutines.html) to build a simple wrapper around it for easier use.

For now, this project primarily serves clice and catter, and the use cases are relatively simple: a single main thread with one event loop is sufficient. All coroutine resumption happens on the main thread as well, so we don't need to explicitly use synchronization primitives such as mutexes.
