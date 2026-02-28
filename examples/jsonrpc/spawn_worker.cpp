#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <utility>

#include "eventide/jsonrpc/peer.h"
#include "eventide/async/loop.h"
#include "eventide/async/process.h"

namespace et = eventide;
namespace jsonrpc = et::jsonrpc;

namespace {

struct BuildParams {
    std::string source;
    std::string header;
    std::string include_path;
};

struct BuildResult {
    std::string command_line;
    std::string resolved_header;
};

struct WorkerLog {
    std::string text;
};

et::task<jsonrpc::Result<BuildResult>> handle_build_request(jsonrpc::RequestContext& context,
                                                            const BuildParams& params) {
    auto log_status = context->send_notification(
        "worker/log",
        WorkerLog{.text = "preparing compile command for " + params.source});
    if(!log_status) {
        co_return std::unexpected(log_status.error());
    }

    co_return BuildResult{
        .command_line = "clang++ -c " + params.source + " -I" + params.include_path,
        .resolved_header = params.include_path + "/" + params.header,
    };
}

et::task<void> run_parent_session(jsonrpc::Peer& peer, et::process child, int& exit_code) {
    auto build_result =
        co_await peer.send_request<BuildResult>("worker/build",
                                                BuildParams{
                                                    .source = "src/main.cpp",
                                                    .header = "vector",
                                                    .include_path = "/opt/eventide/example/include",
                                                });
    if(!build_result) {
        std::println(stderr, "worker request failed: {}", build_result.error());
        exit_code = 1;
        et::event_loop::current().stop();
        co_return;
    }

    std::println("worker command: {}", build_result->command_line);
    std::println("resolved header: {}", build_result->resolved_header);
    auto close_status = peer.close_output();
    if(!close_status) {
        std::println(stderr, "closing worker output failed: {}", close_status.error());
        exit_code = 1;
        et::event_loop::current().stop();
        co_return;
    }

    auto child_status = co_await child.wait();
    if(!child_status) {
        std::println(stderr, "waiting for worker failed: {}", child_status.error().message());
        exit_code = 1;
        et::event_loop::current().stop();
        co_return;
    }

    if(child_status->status != 0 || child_status->term_signal != 0) {
        std::println(stderr,
                     "worker exited unexpectedly: status={} signal={}",
                     child_status->status,
                     child_status->term_signal);
        exit_code = 1;
        et::event_loop::current().stop();
        co_return;
    }

    exit_code = 0;
    et::event_loop::current().stop();
}

int run_worker() {
    jsonrpc::Peer peer;

    peer.on_request("worker/build", handle_build_request);

    return peer.start();
}

int run_parent(std::string self_path) {
    et::event_loop loop;

    et::process::options opts;
    opts.file = self_path;
    opts.args = {self_path, "--worker"};
    opts.streams = {
        et::process::stdio::pipe(true, false),
        et::process::stdio::pipe(false, true),
        et::process::stdio::inherit(),
    };

    auto spawned = et::process::spawn(opts, loop);
    if(!spawned) {
        std::println(stderr, "failed to spawn worker: {}", spawned.error().message());
        return 1;
    }

    auto transport = std::make_unique<jsonrpc::StreamTransport>(std::move(spawned->stdout_pipe),
                                                                std::move(spawned->stdin_pipe));
    jsonrpc::Peer peer(loop, std::move(transport));

    peer.on_notification("worker/log", [](const WorkerLog& params) {
        std::println(stderr, "[worker/log] {}", params.text);
    });

    int exit_code = 1;
    loop.schedule(run_parent_session(peer, std::move(spawned->proc), exit_code));

    auto loop_status = peer.start();
    if(loop_status != 0) {
        std::println(stderr, "parent loop exited with status {}", loop_status);
        return 1;
    }

    return exit_code;
}

}  // namespace

int main(int argc, char** argv) {
    if(argc > 1 && std::string_view(argv[1]) == "--worker") {
        return run_worker();
    }

    auto self_path = std::filesystem::absolute(argv[0]).string();
    return run_parent(std::move(self_path));
}
