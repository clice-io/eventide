#include "worker.h"

#include <charconv>
#include <format>
#include <string_view>

#ifdef KOTA_ZEST_PARALLEL
#include "kota/async/io/loop.h"
#include "kota/async/io/process.h"
#endif

namespace kota::zest::detail {

namespace {

constexpr std::string_view state_names[] = {"passed", "skipped", "failed", "fatal"};

TestState parse_state(std::string_view s) {
    if(s == "passed")
        return TestState::Passed;
    if(s == "skipped")
        return TestState::Skipped;
    if(s == "failed")
        return TestState::Failed;
    return TestState::Fatal;
}

}  // namespace

std::string format_result_line(TestState state, std::chrono::milliseconds duration) {
    auto idx = static_cast<std::size_t>(state);
    return std::format("{}{}{}{}", kResultPrefix, state_names[idx], ":", duration.count());
}

bool parse_result_line(std::string_view line,
                       TestState& state,
                       std::chrono::milliseconds& duration) {
    if(!line.starts_with(kResultPrefix)) {
        return false;
    }
    auto rest = line.substr(kResultPrefix.size());
    auto colon = rest.find(':');
    if(colon == std::string_view::npos) {
        return false;
    }

    state = parse_state(rest.substr(0, colon));
    auto dur_str = rest.substr(colon + 1);
    while(!dur_str.empty() && (dur_str.back() == '\n' || dur_str.back() == '\r')) {
        dur_str.remove_suffix(1);
    }

    std::int64_t ms = 0;
    auto [ptr, ec] = std::from_chars(dur_str.data(), dur_str.data() + dur_str.size(), ms);
    if(ec != std::errc{}) {
        return false;
    }

    duration = std::chrono::milliseconds(ms);
    return true;
}

#ifdef KOTA_ZEST_PARALLEL

namespace {

bool try_extract_result(std::string& buffer, const std::string& test_name, WorkerResult& result) {
    auto pos = buffer.rfind(kResultPrefix);
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

task<void> worker_coro(const std::string& program,
                       const std::vector<std::string>& base_args,
                       const std::vector<std::string>& test_names,
                       std::size_t& next_task,
                       std::vector<WorkerResult>& results) {
    while(next_task < test_names.size()) {
        process::options opts;
        opts.file = program;
        opts.args.push_back(program);
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
            auto idx = next_task++;
            if(idx < test_names.size()) {
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
                    auto output = std::move(buffer);
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

        co_await spawn_res->proc.wait();
    }
}

}  // namespace

void run_parallel_workers(const std::string& program,
                          const std::vector<std::string>& base_args,
                          unsigned num_workers,
                          const std::vector<std::string>& test_names,
                          std::vector<WorkerResult>& results) {
    results.resize(test_names.size());

    event_loop loop;
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

#endif  // KOTA_ZEST_PARALLEL

}  // namespace kota::zest::detail
