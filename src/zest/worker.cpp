#include "worker.h"

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <cpptrace/cpptrace.hpp>

#include "kota/async/io/loop.h"
#include "kota/async/io/process.h"

namespace kota::zest::detail {

namespace {

cpptrace::frame_ptr parse_hex(std::string_view s) {
    if(s.starts_with("0x") || s.starts_with("0X")) {
        s.remove_prefix(2);
    }
    cpptrace::frame_ptr result = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result, 16);
    if(ec != std::errc{} || ptr != s.data() + s.size()) {
        return 0;
    }
    return result;
}

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

bool try_extract_result(std::string& buffer, const std::string& test_name, WorkerResult& result) {
    auto pos = buffer.rfind(result_prefix);
    if(pos == std::string::npos) {
        return false;
    }

    auto newline = buffer.find('\n', pos);
    if(newline == std::string::npos) {
        return false;
    }

    auto line = std::string_view(buffer).substr(pos, newline - pos);
    result.test_name = test_name;
    if(!parse_result_line(line, result.state, result.duration)) {
        result.state = TestState::Fatal;
    }

    std::string output = buffer.substr(0, pos);
    while(!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    result.output = std::move(output);

    buffer.erase(0, newline + 1);
    return true;
}

task<void> worker_coro(std::string_view program,
                       const std::vector<std::string>& base_args,
                       const std::vector<std::string>& test_names,
                       std::size_t& next_task,
                       std::vector<WorkerResult>& results) {
    while(next_task < test_names.size()) {
        process::options opts;
        opts.file = std::string(program);
        opts.args.push_back(std::string(program));
        opts.args.push_back("--zest-worker");
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
            while(next_task < test_names.size()) {
                auto idx = next_task++;
                results[idx] = WorkerResult{
                    .test_name = test_names[idx],
                    .state = TestState::Fatal,
                    .output = "[worker] failed to spawn process",
                };
            }
            co_return;
        }

        std::string buffer;
        bool crashed = false;

        while(true) {
            auto idx = next_task++;
            if(idx >= test_names.size()) {
                spawn_res->stdin_pipe = pipe{};
                // TODO: add timeout to detect hung worker processes
                co_await spawn_res->proc.wait();
                co_return;
            }

            const auto& name = test_names[idx];
            auto cmd = name + "\n";
            auto wr = co_await spawn_res->stdin_pipe.write({cmd.data(), cmd.size()});
            if(wr.has_error()) {
                results[idx] = WorkerResult{
                    .test_name = name,
                    .state = TestState::Fatal,
                    .output = "[worker] stdin write failed",
                };
                crashed = true;
                break;
            }

            while(true) {
                if(try_extract_result(buffer, name, results[idx])) {
                    break;
                }

                auto data = co_await spawn_res->stdout_pipe.read();
                if(!data.has_value()) {
                    auto output = resolve_crash_frames(buffer);
                    if(!output.empty()) {
                        output += '\n';
                    }
                    output += "[worker] process crashed";
                    results[idx] = WorkerResult{
                        .test_name = name,
                        .state = TestState::Fatal,
                        .output = std::move(output),
                    };
                    crashed = true;
                    break;
                }
                buffer += *data;
            }

            if(crashed) {
                break;
            }
        }

        spawn_res->stdin_pipe = pipe{};
        // TODO: add timeout to detect hung worker processes
        co_await spawn_res->proc.wait();
    }
}

}  // namespace

std::string format_result_line(TestState state, std::chrono::milliseconds duration) {
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

bool parse_result_line(std::string_view line,
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

std::string resolve_crash_frames(const std::string& output) {
    auto begin_pos = output.find(trace_begin_marker);
    if(begin_pos == std::string::npos) {
        return output;
    }

    auto end_pos = output.find(trace_end_marker, begin_pos);
    if(end_pos == std::string::npos) {
        return output;
    }

    std::string result = output.substr(0, begin_pos);

    auto block_start = begin_pos + trace_begin_marker.size();
    if(block_start < output.size() && output[block_start] == '\n') {
        ++block_start;
    }

    cpptrace::object_trace obj_trace;
    std::string_view block(output.data() + block_start, end_pos - block_start);

    while(!block.empty()) {
        auto nl = block.find('\n');
        auto line = (nl != std::string_view::npos) ? block.substr(0, nl) : block;
        block = (nl != std::string_view::npos) ? block.substr(nl + 1) : std::string_view{};

        if(!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if(!line.starts_with(frame_prefix)) {
            continue;
        }

        auto rest = line.substr(frame_prefix.size());
        auto colon1 = rest.find(':');
        if(colon1 == std::string_view::npos) {
            continue;
        }
        auto colon2 = rest.find(':', colon1 + 1);
        if(colon2 == std::string_view::npos) {
            continue;
        }

        auto raw_addr = parse_hex(rest.substr(0, colon1));
        auto obj_addr = parse_hex(rest.substr(colon1 + 1, colon2 - colon1 - 1));
        auto obj_path = rest.substr(colon2 + 1);

        obj_trace.frames.push_back(cpptrace::object_frame{
            .raw_address = raw_addr,
            .object_address = obj_addr,
            .object_path = std::string(obj_path),
        });
    }

    if(!obj_trace.frames.empty()) {
        auto resolved = obj_trace.resolve();
        for(const auto& frame: resolved.frames) {
            result += frame.to_string() + '\n';
        }
    }

    auto after_end = end_pos + trace_end_marker.size();
    if(after_end < output.size() && output[after_end] == '\n') {
        ++after_end;
    }
    if(after_end < output.size()) {
        result += output.substr(after_end);
    }

    return result;
}

void run_parallel_workers(std::string_view program,
                          const std::vector<std::string>& base_args,
                          unsigned num_workers,
                          const std::vector<std::string>& test_names,
                          std::vector<WorkerResult>& results) {
    results.resize(test_names.size());

    event_loop loop;
    // Safe: all worker coroutines run on a single-threaded event loop.
    std::size_t next_task = 0;

    std::vector<task<void>> tasks;
    tasks.reserve(num_workers);
    for(unsigned i = 0; i < num_workers; ++i) {
        tasks.push_back(worker_coro(program, base_args, test_names, next_task, results));
    }

    for(auto& t: tasks) {
        loop.schedule(t);
    }
    loop.run();
}

}  // namespace kota::zest::detail
