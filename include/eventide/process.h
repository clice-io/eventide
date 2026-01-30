#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "error.h"
#include "handle.h"
#include "stream.h"
#include "task.h"

namespace eventide {

class event_loop;

class process : public handle {
private:
    using handle::handle;

public:
    process(process&& other) noexcept;

    process& operator=(process&& other) noexcept;

    struct exit_status {
        /// Exit code reported by the child.
        int64_t status;

        /// Terminating signal number if signalled, 0 otherwise.
        int term_signal;
    };

    struct stdio {
        enum class kind { inherit, ignore, fd, pipe };

        /// How this stream should be configured for the child.
        kind type = kind::inherit;

        /// Descriptor to inherit when type == fd.
        int descriptor = -1;

        /// Child-readable flag when type == pipe.
        bool readable = false;  // from the child's perspective

        /// Child-writable flag when type == pipe.
        bool writable = false;  // from the child's perspective

        /// Inherit parent's descriptor (default).
        static stdio inherit();

        /// Discard this stream for the child.
        static stdio ignore();

        /// Duplicate the given descriptor into the child.
        static stdio from_fd(int fd);

        /// Create a pipe; flags are from the child's perspective.
        static stdio pipe(bool readable, bool writable);
    };

    struct options {
        /// Executable path.
        std::string file;

        /// argv (excluding argv[0], which is taken from `file`).
        std::vector<std::string> args;

        /// Environment variables in `KEY=VALUE` form; empty means inherit.
        std::vector<std::string> env;

        /// Working directory; empty means inherit.
        std::string cwd;

        /// Whether to detach the child process.
        bool detached = false;

        /// Hide window on platforms that support it.
        bool hide_window = false;

        /// Stdio config for stdin/stdout/stderr.
        std::array<stdio, 3> streams = {stdio::inherit(), stdio::inherit(), stdio::inherit()};
    };

    using wait_result = result<exit_status>;

    /// Launch the process; creates pipes as requested in options.
    struct spawn_result;

    /// Spawn a child process within the given loop.
    static result<spawn_result> spawn(event_loop& loop, const options& opts);

    /// Await process termination and fetch exit status.
    task<wait_result> wait();

    /// Retrieve OS pid for the process; -1 if not started.
    int pid() const noexcept;

    /// Send a signal to the process.
    error kill(int signum);

private:
    template <typename Tag>
    friend struct awaiter;

    async_node* waiter = nullptr;
    exit_status* active = nullptr;
    std::optional<exit_status> completed;
};

struct process::spawn_result {
    process proc;

    pipe stdin_pipe;

    pipe stdout_pipe;

    pipe stderr_pipe;
};

}  // namespace eventide
