#pragma once

#include <chrono>
#include <cstdint>

#include "error.h"
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
    static timer create(event_loop& loop);

    void start(std::chrono::milliseconds timeout, std::chrono::milliseconds repeat = {});

    void stop();

    task<> wait();

private:
    async_node* waiter = nullptr;
    int pending = 0;
};

class idle : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static idle create(event_loop& loop);

    void start();

    void stop();

    task<> wait();

private:
    async_node* waiter = nullptr;
    int pending = 0;
};

class prepare : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static prepare create(event_loop& loop);

    void start();

    void stop();

    task<> wait();

private:
    async_node* waiter = nullptr;
    int pending = 0;
};

class check : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static check create(event_loop& loop);

    void start();

    void stop();

    task<> wait();

private:
    async_node* waiter = nullptr;
    int pending = 0;
};

class signal : public handle {
private:
    using handle::handle;

    template <typename Tag>
    friend struct awaiter;

public:
    static result<signal> create(event_loop& loop);

    error start(int signum);

    error stop();

    task<error> wait();

private:
    async_node* waiter = nullptr;
    error* active = nullptr;
    int pending = 0;
};

task<> sleep(event_loop& loop, std::chrono::milliseconds timeout);

}  // namespace eventide
