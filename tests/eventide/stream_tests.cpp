#include <array>
#include <atomic>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <BaseTsd.h>
#include <fcntl.h>
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "zest/zest.h"
#include "eventide/loop.h"
#include "eventide/stream.h"

namespace eventide {

#ifdef _WIN32
struct wsa_init_guard {
    wsa_init_guard() {
        WSADATA data{};
        (void)WSAStartup(MAKEWORD(2, 2), &data);
    }

    ~wsa_init_guard() {
        WSACleanup();
    }
};

static wsa_init_guard wsa_guard;
using ssize_t = SSIZE_T;
#endif

namespace {

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t invalid_socket = INVALID_SOCKET;
constexpr int socket_error = SOCKET_ERROR;

inline int close_socket(socket_t sock) {
    return ::closesocket(sock);
}

inline int close_fd(int fd) {
    return _close(fd);
}

inline ssize_t write_fd(int fd, const char* data, size_t len) {
    return _write(fd, data, static_cast<unsigned int>(len));
}

inline int create_pipe(int fds[2]) {
    return _pipe(fds, 4096, _O_BINARY);
}
#else
using socket_t = int;
constexpr socket_t invalid_socket = -1;
constexpr int socket_error = -1;

inline int close_socket(socket_t sock) {
    return ::close(sock);
}

inline int close_fd(int fd) {
    return ::close(fd);
}

inline ssize_t write_fd(int fd, const char* data, size_t len) {
    return ::write(fd, data, len);
}

inline int create_pipe(int fds[2]) {
    return ::pipe(fds);
}
#endif

int pick_free_port() {
    socket_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd == invalid_socket) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        close_socket(fd);
        return -1;
    }

    int port = ntohs(addr.sin_port);
    close_socket(fd);
    return port;
}

task<std::string> read_from_pipe(pipe p) {
    auto out = co_await p.read();
    event_loop::current()->stop();
    co_return out;
}

task<result<std::string>> accept_and_read(tcp_socket::acceptor acc) {
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

task<result<tcp_socket>> accept_once(tcp_socket::acceptor& acc, std::atomic<int>& done) {
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
    ASSERT_EQ(create_pipe(fds), 0);

    const std::string message = "eventide-pipe";
    ASSERT_EQ(write_fd(fds[1], message.data(), message.size()),
              static_cast<ssize_t>(message.size()));
    close_fd(fds[1]);

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

    socket_t client_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(client_fd != invalid_socket);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    ASSERT_EQ(::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    const std::string message = "eventide-tcp";
    ASSERT_EQ(::send(client_fd, message.data(), message.size(), 0),
              static_cast<ssize_t>(message.size()));
    close_socket(client_fd);

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

    socket_t client_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_TRUE(client_fd != invalid_socket);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    ASSERT_EQ(::connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    close_socket(client_fd);

    loop.schedule(first);
    loop.schedule(second);
    loop.run();

    auto first_res = first.result();
    auto second_res = second.result();
    EXPECT_TRUE(first_res.has_value());
    EXPECT_FALSE(second_res.has_value());
    if(!second_res.has_value()) {
        EXPECT_EQ(second_res.error().value(), error::connection_already_in_progress.value());
    }
}

};  // TEST_SUITE(tcp)

}  // namespace eventide
