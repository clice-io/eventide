#pragma once

#include <deque>
#include <memory>

#include "task.h"

namespace eventide {

struct async_frame;

class event_loop {
public:
    event_loop();

    ~event_loop();

    struct impl;

    void* handle();

    int run();

    void stop();

    impl* operator->() {
        return self.get();
    }

    void schedule(async_frame& frame,
                  std::source_location location = std::source_location::current());

    template <typename T>
    void schedule(task<T>& task, std::source_location location = std::source_location::current()) {
        schedule(static_cast<async_frame&>(task.h.promise()), location);
    }

    static event_loop* current();

private:
    std::unique_ptr<impl> self;
};

}  // namespace eventide
