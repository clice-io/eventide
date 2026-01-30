#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

#include "uv.h"
#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/stream.h"

namespace eventide {

namespace {

int pick_free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        ::close(fd);
        return -1;
    }

    int port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

task<std::string> read_from_pipe(pipe p) {
    auto out = co_await p.read();
    event_loop::current()->stop();
    co_return out;
}

task<std::expected<std::string, std::error_code>> accept_and_read(tcp_socket::acceptor acc) {
    auto conn_res = co_await acc.accept();
    if(!conn_res.has_value()) {
        event_loop::current()->stop();
        co_return std::unexpected(conn_res.error());
    }

    auto conn = std::move(*conn_res);
    auto data = co_await conn.read();

    event_loop::current()->stop();
    co_return data;
}

task<std::expected<tcp_socket, std::error_code>> accept_once(tcp_socket::acceptor& acc,
                                                             std::atomic<int>& done) {
    auto res = co_await acc.accept();
    if(done.fetch_add(1) + 1 == 2) {
        event_loop::current()->stop();
    }
    co_return res;
}

}  // namespace

TEST_SUITE(pipe_io) {

TEST_CASE(read_from_fd) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(::pipe(fds), 0);

    const std::string message = "eventide-pipe";
    ASSERT_EQ(::write(fds[1], message.data(), message.size()),
              static_cast<ssize_t>(message.size()));
    ::close(fds[1]);

    event_loop loop;
    auto pipe_res = pipe::open(loop, fds[0]);
    ASSERT_TRUE(pipe_res.has_value());

    auto reader = read_from_pipe(std::move(*pipe_res));

    loop.schedule(reader);
    loop.run();

    EXPECT_EQ(reader.result(), message);
}

};  // TEST_SUITE(pipe_io)

TEST_SUITE(tcp) {

TEST_CASE(accept_and_read) {
    int port = pick_free_port();
    ASSERT_TRUE(port > 0);

    event_loop loop;
    auto acc_res = tcp_socket::listen(loop, "127.0.0.1", port);
    ASSERT_TRUE(acc_res.has_value());

    auto server = accept_and_read(std::move(*acc_res));

    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(client_fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    ASSERT_EQ(::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    const std::string message = "eventide-tcp";
    ASSERT_EQ(::send(client_fd, message.data(), message.size(), 0),
              static_cast<ssize_t>(message.size()));
    ::close(client_fd);

    loop.schedule(server);
    loop.run();

    auto result = server.result();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, message);
}

TEST_CASE(accept_already_waiting) {
    int port = pick_free_port();
    ASSERT_TRUE(port > 0);

    event_loop loop;
    auto acc_res = tcp_socket::listen(loop, "127.0.0.1", port);
    ASSERT_TRUE(acc_res.has_value());

    auto acc = std::move(*acc_res);
    std::atomic<int> done{0};

    auto first = accept_once(acc, done);
    auto second = accept_once(acc, done);

    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(client_fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    ASSERT_EQ(::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    ::close(client_fd);

    loop.schedule(first);
    loop.schedule(second);
    loop.run();

    auto first_res = first.result();
    auto second_res = second.result();
    EXPECT_TRUE(first_res.has_value());
    EXPECT_FALSE(second_res.has_value());
    if(!second_res.has_value()) {
        EXPECT_EQ(second_res.error().value(), static_cast<int>(UV_EALREADY));
    }
}

};  // TEST_SUITE(tcp)

}  // namespace eventide
