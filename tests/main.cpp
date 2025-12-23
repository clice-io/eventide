#include <iostream>
#include <print>

#include "eventide/handle.h"
#include "eventide/loop.h"
#include "eventide/ringbuffer.h"
#include "eventide/stream.h"
#include "eventide/task.h"

namespace ev = eventide;

int main() {
    ev::pipe p;
    p.close();
    p.read({});
    return 0;
}
