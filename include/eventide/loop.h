#pragma once

#include "atom.h"

namespace eventide {

class event_loop : non_moveable {
public:
    event_loop();

    ~event_loop();

    int run();

    void stop();

private:
    alignas(layout<event_loop>::align) char storage[layout<event_loop>::size];
};

}  // namespace eventide
