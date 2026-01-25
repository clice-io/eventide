#pragma once

#include <memory>
#include <source_location>

namespace eventide {

class async_node;

template <typename T>
class task;

template <typename T>
class shared_task;

class event_loop {
public:
    event_loop();

    ~event_loop();

    static event_loop* current();

    struct self;

    self* operator->() {
        return self.get();
    }

    friend class async_node;

public:
    void* handle();

    int run();

    void stop();

    template <typename T>
    void schedule(task<T> task, std::source_location location = std::source_location::current()) {
        schedule(static_cast<async_node&>(task.h.promise()), location);
    }

    template <typename T>
    void schedule(shared_task<T> task,
                  std::source_location location = std::source_location::current()) {
        schedule(static_cast<async_node&>(task.h.promise()), location);
    }

private:
    void schedule(async_node& frame, std::source_location location);

    std::unique_ptr<self> self;
};

}  // namespace eventide
