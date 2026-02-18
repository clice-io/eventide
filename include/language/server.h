#pragma once

#include "protocol.h"
#include "eventide/task.h"

namespace language {

class LanguageServer {
public:
    int start();

private:
    struct Self;
    std::unique_ptr<Self> self;
};

}  // namespace language
