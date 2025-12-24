#pragma once

#include <deque>
#include <memory>

#include "task.h"

namespace eventide {

struct promise_base;

class event_loop {
public:
    event_loop();

    ~event_loop();

    struct impl;

    void* handle();

    int run();

    template <typename T>
    int run(task<T> task) {
        task.handle().promise().loop = self.get();
        task.schedule();
        return run();
    }

    void stop();

private:
    std::unique_ptr<impl> self;
};

}  // namespace eventide
