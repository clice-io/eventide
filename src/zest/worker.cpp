#include "worker.h"

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "kota/async/io/loop.h"
#include "kota/async/io/process.h"

namespace kota::zest::detail {

namespace {

std::optional<TestState> parse_state(std::string_view s) {
    if(s == "passed")
        return TestState::Passed;
    if(s == "skipped")
        return TestState::Skipped;
    if(s == "failed")
        return TestState::Failed;
    if(s == "fatal")
        return TestState::Fatal;
    return std::nullopt;
}

/// Scan `buffer` for a complete, line-anchored result line. `scan_pos` tracks
/// how far previous calls have already looked, so repeated calls stay linear
/// in the total amount of output a test produces.
bool try_extract_result(const std::string& result_prefix,
                        std::string& buffer,
                        std::size_t& scan_pos,
                        WorkerResult& result) {
    auto pos = scan_pos;
    while(true) {
        pos = buffer.find(result_prefix, pos);
        if(pos == std::string::npos) {
            // Resume the next scan where a partial prefix could still begin.
            scan_pos = buffer.size() >= result_prefix.size()
                           ? buffer.size() - result_prefix.size() + 1
                           : 0;
            return false;
        }
        if(pos == 0 || buffer[pos - 1] == '\n') {
            break;
        }
        pos += 1;
    }

    auto newline = buffer.find('\n', pos + result_prefix.size());
    if(newline == std::string::npos) {
        scan_pos = pos;  // result line not complete yet
        return false;
    }

    auto line = std::string_view(buffer).substr(pos, newline - pos);
    if(!parse_result_line(result_prefix, line, result.state, result.duration)) {
        result.state = TestState::Fatal;
    }

    std::string output = buffer.substr(0, pos);
    while(!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    result.output = std::move(output);
    result.done = true;

    buffer.erase(0, newline + 1);
    scan_pos = 0;
    return true;
}

task<void> worker_coro(std::string_view program,
                       const std::vector<std::string>& base_args,
                       const std::string& result_prefix,
                       const std::string& worker_flag,
                       const std::vector<std::string>& test_names,
                       std::size_t& next_task,
                       std::vector<WorkerResult>& results) {
    // A test whose name was claimed but never reached a live child (the
    // previous test killed the process after flushing its result). It gets
    // one retry on a freshly spawned process before being reported Fatal.
    std::optional<std::size_t> carry;

    while(carry.has_value() || next_task < test_names.size()) {
        process::options opts;
        opts.file = std::string(program);
        opts.args.push_back(std::string(program));
        opts.args.push_back(worker_flag);
        for(const auto& arg: base_args) {
            opts.args.push_back(arg);
        }
        opts.streams = {
            process::stdio::pipe(true, false),
            process::stdio::pipe(false, true),
            process::stdio::inherit(),
        };

        auto spawn_res = process::spawn(opts);
        if(!spawn_res.has_value()) {
            // Leave unclaimed tests to sibling workers; run_parallel_workers
            // reports any test that no worker could reach.
            co_return;
        }

        std::string buffer;
        std::size_t scan_pos = 0;
        bool crashed = false;

        while(!crashed) {
            const bool is_retry = carry.has_value();
            std::size_t idx;
            if(is_retry) {
                idx = *carry;
            } else if(next_task < test_names.size()) {
                idx = next_task++;
            } else {
                break;
            }

            const auto& name = test_names[idx];
            auto cmd = name + "\n";
            auto wr = co_await spawn_res->stdin_pipe.write({cmd.data(), cmd.size()});
            if(wr.has_error()) {
                if(is_retry) {
                    results[idx] = WorkerResult{
                        .state = TestState::Fatal,
                        .output = "[worker] stdin write failed",
                        .done = true,
                    };
                    carry.reset();
                } else {
                    carry = idx;
                }
                crashed = true;
                break;
            }
            carry.reset();

            while(true) {
                if(try_extract_result(result_prefix, buffer, scan_pos, results[idx])) {
                    break;
                }

                auto data = co_await spawn_res->stdout_pipe.read();
                if(!data.has_value()) {
                    std::string output = std::move(buffer);
                    if(!output.empty()) {
                        output += '\n';
                    }
                    output += "[worker] process crashed";
                    results[idx] = WorkerResult{
                        .state = TestState::Fatal,
                        .output = std::move(output),
                        .done = true,
                    };
                    crashed = true;
                    break;
                }
                buffer += *data;
            }
        }

        // Close stdin (EOF) and reap the child before respawning or exiting.
        spawn_res->stdin_pipe = pipe{};
        // TODO: add timeout to detect hung worker processes
        co_await spawn_res->proc.wait();
    }
}

}  // namespace

std::string make_result_prefix(std::string_view token) {
    return std::format("{}{}:", result_marker, token);
}

std::string format_result_line(std::string_view result_prefix,
                               TestState state,
                               std::chrono::milliseconds duration) {
    const char* name = [&] {
        switch(state) {
            case TestState::Passed: return "passed";
            case TestState::Skipped: return "skipped";
            case TestState::Failed: return "failed";
            case TestState::Fatal: return "fatal";
        }
        return "fatal";  // unreachable
    }();
    return std::format("{}{}:{}", result_prefix, name, duration.count());
}

bool parse_result_line(std::string_view result_prefix,
                       std::string_view line,
                       TestState& state,
                       std::chrono::milliseconds& duration) {
    if(!line.starts_with(result_prefix)) {
        return false;
    }
    auto rest = line.substr(result_prefix.size());
    auto colon = rest.find(':');
    if(colon == std::string_view::npos) {
        return false;
    }

    auto parsed_state = parse_state(rest.substr(0, colon));
    if(!parsed_state.has_value()) {
        return false;
    }

    auto dur_str = rest.substr(colon + 1);
    while(!dur_str.empty() && (dur_str.back() == '\n' || dur_str.back() == '\r')) {
        dur_str.remove_suffix(1);
    }

    std::int64_t ms = 0;
    auto [ptr, ec] = std::from_chars(dur_str.data(), dur_str.data() + dur_str.size(), ms);
    if(ec != std::errc{} || ptr != dur_str.data() + dur_str.size()) {
        return false;
    }

    state = *parsed_state;
    duration = std::chrono::milliseconds(ms);
    return true;
}

void run_parallel_workers(std::string_view program,
                          const std::vector<std::string>& base_args,
                          unsigned num_workers,
                          const std::vector<std::string>& test_names,
                          std::string_view result_token,
                          std::vector<WorkerResult>& results) {
    results.assign(test_names.size(), WorkerResult{});

    const auto result_prefix = make_result_prefix(result_token);
    const auto worker_flag = std::format("--zest-worker={}", result_token);

    event_loop loop;
    // Safe: all worker coroutines run on a single-threaded event loop.
    std::size_t next_task = 0;

    std::vector<task<void>> tasks;
    tasks.reserve(num_workers);
    for(unsigned i = 0; i < num_workers; ++i) {
        tasks.push_back(worker_coro(program,
                                    base_args,
                                    result_prefix,
                                    worker_flag,
                                    test_names,
                                    next_task,
                                    results));
    }

    for(auto& t: tasks) {
        loop.schedule(t);
    }
    loop.run();

    // Tests no worker could reach (every spawn attempt failed).
    for(auto& result: results) {
        if(!result.done) {
            result.state = TestState::Fatal;
            result.output = "[worker] failed to spawn worker process";
        }
    }
}

}  // namespace kota::zest::detail
