#pragma once

#include <deque>
#include <memory>

#include "task.h"

namespace eventide {

class event_loop {
public:
    event_loop();

    ~event_loop();

    struct impl;

    void* handle();

    int run();

    void stop();

    impl* operator->(){
        return self.get();
    }

    static event_loop* current();

private:
    std::unique_ptr<impl> self;
};

}  // namespace eventide
