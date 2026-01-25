#pragma once

#include <cstdint>
#include <expected>
#include <system_error>

#include "handle.h"
#include "task.h"

namespace eventide {

class event_loop;

template <typename Tag>
struct awaiter;

class timer : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static std::expected<timer, std::error_code> create(event_loop& loop);

    std::error_code start(std::uint64_t timeout_ms, std::uint64_t repeat_ms = 0);

    std::error_code stop();

    task<std::error_code> wait();

private:
    async_node* waiter = nullptr;
    std::error_code* active = nullptr;
    int pending = 0;
};

class idle : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static std::expected<idle, std::error_code> create(event_loop& loop);

    std::error_code start();

    std::error_code stop();

    task<std::error_code> wait();

private:
    async_node* waiter = nullptr;
    std::error_code* active = nullptr;
    int pending = 0;
};

class prepare : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static std::expected<prepare, std::error_code> create(event_loop& loop);

    std::error_code start();

    std::error_code stop();

    task<std::error_code> wait();

private:
    async_node* waiter = nullptr;
    std::error_code* active = nullptr;
    int pending = 0;
};

class check : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static std::expected<check, std::error_code> create(event_loop& loop);

    std::error_code start();

    std::error_code stop();

    task<std::error_code> wait();

private:
    async_node* waiter = nullptr;
    std::error_code* active = nullptr;
    int pending = 0;
};

class signal : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static std::expected<signal, std::error_code> create(event_loop& loop);

    std::error_code start(int signum);

    std::error_code stop();

    task<std::error_code> wait();

private:
    async_node* waiter = nullptr;
    std::error_code* active = nullptr;
    int pending = 0;
};

}  // namespace eventide
