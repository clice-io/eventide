#include "worker.h"

#include <charconv>
#include <format>
#include <string_view>

#ifndef _WIN32
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace kota::zest::detail {

namespace {

constexpr std::string_view state_names[] = {"passed", "skipped", "failed", "fatal"};

TestState parse_state(std::string_view s) {
    if(s == "passed") return TestState::Passed;
    if(s == "skipped") return TestState::Skipped;
    if(s == "failed") return TestState::Failed;
    return TestState::Fatal;
}

WorkerResult make_result(const std::string& test_name,
                         std::string output,
                         int exit_code,
                         bool signaled,
                         int signal_num) {
    WorkerResult result;
    result.test_name = test_name;

    auto pos = output.rfind(kResultPrefix);
    if(pos != std::string::npos) {
        auto line_start = pos;
        auto line_end = output.find('\n', pos);
        auto line = std::string_view(output).substr(
            line_start, line_end == std::string::npos ? std::string_view::npos : line_end - line_start);

        if(parse_result_line(line, result.state, result.duration)) {
            auto erase_start = (pos > 0 && output[pos - 1] == '\n') ? pos - 1 : pos;
            auto erase_end = (line_end == std::string::npos) ? output.size() : line_end + 1;
            output.erase(erase_start, erase_end - erase_start);
        }
    } else {
        result.state = TestState::Fatal;
        if(signaled) {
            output += std::format("\n[worker] killed by signal {}\n", signal_num);
        } else if(exit_code != 0) {
            output += std::format("\n[worker] exited with code {}\n", exit_code);
        } else {
            output += "\n[worker] no result reported\n";
        }
    }

    while(!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }

    result.output = std::move(output);
    return result;
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

#ifndef _WIN32

WorkerResult run_worker_process(const std::string& program,
                                const std::string& test_name,
                                const std::vector<std::string>& base_args) {
    int pipefd[2];
    if(pipe(pipefd) == -1) {
        return WorkerResult{
            .test_name = test_name,
            .state = TestState::Fatal,
            .output = std::format("[worker] pipe() failed: {}", std::strerror(errno)),
        };
    }

    pid_t pid = fork();
    if(pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return WorkerResult{
            .test_name = test_name,
            .state = TestState::Fatal,
            .output = std::format("[worker] fork() failed: {}", std::strerror(errno)),
        };
    }

    if(pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        std::vector<std::string> arg_storage;
        arg_storage.reserve(base_args.size() + 3);
        arg_storage.push_back(program);
        arg_storage.push_back("--zest-worker");
        arg_storage.push_back("--test-filter=" + test_name);
        for(const auto& arg: base_args) {
            arg_storage.push_back(arg);
        }

        std::vector<char*> argv;
        argv.reserve(arg_storage.size() + 1);
        for(auto& s: arg_storage) {
            argv.push_back(s.data());
        }
        argv.push_back(nullptr);

        execv(program.c_str(), argv.data());
        _exit(127);
    }

    close(pipefd[1]);

    std::string output;
    char buf[4096];
    ssize_t n;
    while((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        output.append(buf, static_cast<std::size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    return make_result(test_name,
                       std::move(output),
                       WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                       WIFSIGNALED(status),
                       WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}

#else  // _WIN32

namespace {

std::string build_worker_env_block() {
    LPCH env = GetEnvironmentStringsA();
    if(!env) {
        return std::string("KOTA_ZEST_WORKER=1\0\0", 21);
    }

    std::string block;
    LPCH p = env;
    while(*p) {
        auto len = std::strlen(p);
        block.append(p, len + 1);
        p += len + 1;
    }
    FreeEnvironmentStringsA(env);

    std::string_view var = "KOTA_ZEST_WORKER=1";
    block.append(var.data(), var.size() + 1);
    block.push_back('\0');
    return block;
}

std::string quote_arg(const std::string& arg) {
    if(arg.find_first_of(" \t\"") == std::string::npos) {
        return arg;
    }
    std::string result = "\"";
    for(auto c: arg) {
        if(c == '"') {
            result += "\\\"";
        } else {
            result += c;
        }
    }
    result += '"';
    return result;
}

}  // namespace

WorkerResult run_worker_process(const std::string& program,
                                const std::string& test_name,
                                const std::vector<std::string>& base_args) {
    std::string cmdline = quote_arg(program);
    cmdline += " --zest-worker";
    cmdline += " --test-filter=" + test_name;
    for(const auto& arg: base_args) {
        cmdline += " " + quote_arg(arg);
    }

    auto env_block = build_worker_env_block();

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = INVALID_HANDLE_VALUE;
    HANDLE write_pipe = INVALID_HANDLE_VALUE;
    if(!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return WorkerResult{
            .test_name = test_name,
            .state = TestState::Fatal,
            .output = "[worker] CreatePipe() failed",
        };
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    BOOL created = CreateProcessA(nullptr,
                                  cmdline.data(),
                                  nullptr,
                                  nullptr,
                                  TRUE,
                                  0,
                                  env_block.data(),
                                  nullptr,
                                  &si,
                                  &pi);

    CloseHandle(write_pipe);

    if(!created) {
        CloseHandle(read_pipe);
        return WorkerResult{
            .test_name = test_name,
            .state = TestState::Fatal,
            .output = "[worker] CreateProcess() failed",
        };
    }

    std::string output;
    char buf[4096];
    DWORD bytes_read;
    while(ReadFile(read_pipe, buf, sizeof(buf), &bytes_read, nullptr) && bytes_read > 0) {
        output.append(buf, bytes_read);
    }
    CloseHandle(read_pipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return make_result(test_name, std::move(output), static_cast<int>(exit_code), false, 0);
}

#endif

}  // namespace kota::zest::detail
