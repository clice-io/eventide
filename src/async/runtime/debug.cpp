#include "kota/async/runtime/debug.h"

#include <format>
#include <string>
#include <string_view>

#include "kota/async/runtime/walk.h"

namespace kota {

static std::string_view async_kind_name(async_node::NodeKind k) {
    switch(k) {
        case async_node::NodeKind::Task: return "Task";
        case async_node::NodeKind::MutexWaiter: return "MutexWaiter";
        case async_node::NodeKind::EventWaiter: return "EventWaiter";
        case async_node::NodeKind::WhenAll: return "WhenAll";
        case async_node::NodeKind::WhenAny: return "WhenAny";
        case async_node::NodeKind::TaskGroup: return "TaskGroup";
        case async_node::NodeKind::SystemIO: return "SystemIO";
    }
    return "Unknown";
}

static std::string_view state_name(async_node::State s) {
    switch(s) {
        case async_node::Pending: return "Pending";
        case async_node::Running: return "Running";
        case async_node::Cancelled: return "Cancelled";
        case async_node::Finished: return "Finished";
        case async_node::Failed: return "Failed";
    }
    return "Unknown";
}

static std::string_view sync_kind_name(sync_primitive::Kind k) {
    switch(k) {
        case sync_primitive::Kind::Mutex: return "Mutex";
        case sync_primitive::Kind::Event: return "Event";
        case sync_primitive::Kind::Semaphore: return "Semaphore";
        case sync_primitive::Kind::ConditionVariable: return "ConditionVariable";
    }
    return "Unknown";
}

static std::string node_id(const void* node) {
    return std::format("n{:x}", reinterpret_cast<std::uintptr_t>(node));
}

static std::string_view basename(const char* path) {
    if(!path || path[0] == '\0') {
        return {};
    }
    std::string_view sv(path);
    auto pos = sv.find_last_of(R"(/\)");
    return pos != std::string_view::npos ? sv.substr(pos + 1) : sv;
}

static void emit_async_node(const async_node* node, std::string& out) {
    auto file = basename(node->location.file_name());
    std::string label;
    if(!file.empty()) {
        label = std::format(R"({}
{}
{}:{})",
                            async_kind_name(node->kind),
                            state_name(node->state),
                            file,
                            node->location.line());
    } else {
        label = std::format(R"({}
{})",
                            async_kind_name(node->kind),
                            state_name(node->state));
    }

    std::string_view shape = "box";
    std::string_view color = "white";

    if(node->is_task_frame()) {
        switch(node->state) {
            case async_node::Running: color = R"("#90EE90")"; break;
            case async_node::Finished: color = R"("#D3D3D3")"; break;
            case async_node::Cancelled: color = R"("#FFB6C1")"; break;
            case async_node::Failed: color = R"("#FFA07A")"; break;
            default: break;
        }
    } else if(node->is_aggregate_op()) {
        shape = "diamond";
        color = R"("#D8BFD8")";
    } else if(node->kind == async_node::NodeKind::SystemIO) {
        color = R"("#FFFFE0")";
    } else if(node->is_wait_node()) {
        color = R"("#FFDAB9")";
    }

    std::format_to(std::back_inserter(out),
                   R"(  {} [label="{}", shape={}, style=filled, fillcolor={}];
)",
                   node_id(node),
                   label,
                   shape,
                   color);
}

static void emit_sync_node(const sync_primitive* resource, std::string& out) {
    auto file = basename(resource->location.file_name());
    std::string label;
    if(!file.empty()) {
        label = std::format(R"({}
{}:{})",
                            sync_kind_name(resource->kind),
                            file,
                            resource->location.line());
    } else {
        label = std::format("{}", sync_kind_name(resource->kind));
    }

    std::format_to(std::back_inserter(out),
                   R"(  {} [label="{}", shape=ellipse, style=filled, fillcolor="{}"];
)",
                   node_id(resource),
                   label,
                   "#ADD8E6");
}

struct dot_emitter : async_visitor<dot_emitter> {
    std::string out;

    bool visit_task(const task_frame& node) {
        emit_async_node(&node, out);
        return true;
    }

    bool visit_wait_node(const wait_node& node) {
        emit_async_node(&node, out);
        return true;
    }

    bool visit_aggregate(const aggregate_op& node) {
        emit_async_node(&node, out);
        return true;
    }

    bool visit_io(const io_op& node) {
        emit_async_node(&node, out);
        return true;
    }

    bool visit_sync(const sync_primitive& resource) {
        emit_sync_node(&resource, out);
        return true;
    }

    void visit_edge(const void* from, const void* to) {
        std::format_to(std::back_inserter(out),
                       R"(  {} -> {};
)",
                       node_id(from),
                       node_id(to));
    }
};

std::string dump_dot(const async_node& root) {
    const auto* node = &root;
    while(auto* p = get_parent(*node)) {
        node = p;
    }

    dot_emitter emitter;
    emitter.out += R"(digraph async_graph {
)";
    emitter.out += R"(  rankdir=TB;
)";
    emitter.out += R"(  node [fontname="Helvetica", fontsize=10];
)";

    emitter.walk_node(*node);

    emitter.out += R"(}
)";
    return std::move(emitter.out);
}

}  // namespace kota
