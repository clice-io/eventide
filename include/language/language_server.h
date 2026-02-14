#pragma once

#include <memory>
#include <utility>

#include "eventide/loop.h"
#include "eventide/move_only_function.h"
#include "eventide/stream.h"
#include "eventide/sync.h"
#include "eventide/task.h"
#include "language/lsp_types.h"

namespace eventide::language {

using namespace proto;

class language_server {
public:
    explicit language_server(stream io, event_loop& loop = event_loop::current());
    ~language_server();

#define EVENTIDE_LSP_UNWRAP(...) __VA_ARGS__

#define EVENTIDE_LSP_DECLARE_REQUEST(name, method, Params, Result)                                 \
    using name##_handler_t = eventide::move_only_function<task<EVENTIDE_LSP_UNWRAP Result>(        \
        EVENTIDE_LSP_UNWRAP Params)>;                                                              \
    void on_##name(name##_handler_t handler) {                                                     \
        name##_handler = std::move(handler);                                                       \
    }                                                                                              \
    name##_handler_t name##_handler{};

#define EVENTIDE_LSP_DECLARE_NOTIFICATION(name, method, Params)                                    \
    using name##_handler_t = eventide::move_only_function<task<>(EVENTIDE_LSP_UNWRAP Params)>;     \
    void on_##name(name##_handler_t handler) {                                                     \
        name##_handler = std::move(handler);                                                       \
    }                                                                                              \
    name##_handler_t name##_handler{};

#include "language/lsp_methods.inc"

    EVENTIDE_LSP_REQUESTS(EVENTIDE_LSP_DECLARE_REQUEST)
    EVENTIDE_LSP_NOTIFICATIONS(EVENTIDE_LSP_DECLARE_NOTIFICATION)

#undef EVENTIDE_LSP_DECLARE_REQUEST
#undef EVENTIDE_LSP_DECLARE_NOTIFICATION
#undef EVENTIDE_LSP_UNWRAP

    task<> run();

private:
    struct Self;
    std::unique_ptr<Self> self;
};

}  // namespace eventide::language
