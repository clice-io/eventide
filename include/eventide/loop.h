#pragma once

#include <memory>
#include <source_location>
#include <tuple>

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

    static event_loop& current();

    struct self;

    self* operator->() {
        return self.get();
    }

    friend class async_node;

public:
    void* handle();

    int run();

    void stop();

    template <typename Task>
    void schedule(Task&& task, std::source_location location = std::source_location::current()) {
        if constexpr(std::is_rvalue_reference_v<Task&&>) {
            task.h.promise().root = true;
        }

        schedule(static_cast<async_node&>(task.h.promise()), location);
    }

private:
    void schedule(async_node& frame, std::source_location location);

    std::unique_ptr<self> self;
};

template <typename... Tasks>
auto run(Tasks&&... tasks) {
    event_loop loop;
    (loop.schedule(tasks), ...);
    loop.run();
    return std::tuple(std::move(tasks.result())...);
}

}  // namespace eventide
