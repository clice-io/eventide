#pragma once

#include <deque>

#include "atom.h"
#include "task.h"

namespace eventide {

struct promise_base;

class event_loop : public uv_layout<event_loop> {
public:
    event_loop();

    ~event_loop();

    int run();

    template<typename T>
    int run(task<T> task){
        task.handle().promise().loop = this;
        task.schedule();
        return run();
    }

    void stop();

public:
    bool idle_running = false;
    uv_layout<idle> idle_h;
    std::deque<promise_base*> tasks;
};

}  // namespace eventide
