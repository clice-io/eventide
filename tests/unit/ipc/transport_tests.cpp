#include <atomic>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "test_transport.h"
#include "../support/fd_helpers.h"
#include "kota/ipc/transport.h"
#include "kota/zest/zest.h"
#include "kota/async/async.h"

namespace kota::ipc {

using test::create_pipe;
using test::close_fd;
using test::write_fd;

namespace {

std::optional<std::string> payload_of(ReadEvent result) {
    if(auto* payload = std::get_if<std::string>(&result)) {
        return std::move(*payload);
    }
    return std::nullopt;
}

task<std::pair<ReadEvent, ReadEvent>> read_two_results(StreamTransport& transport) {
    auto first = co_await transport.read_message();
    auto second = co_await transport.read_message();
    event_loop::current().stop();
    co_return std::pair{std::move(first), std::move(second)};
}

TEST_SUITE(ipc_transport) {

TEST_CASE(consecutive_messages) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    const std::string first_payload =
        R"({"jsonrpc":"2.0","method":"example/note","params":{"text":"first"}})";
    const std::string second_payload = R"({"jsonrpc":"2.0","id":1,"result":{"sum":9}})";
    const auto payload = frame(first_payload) + frame(second_payload);

    ASSERT_EQ(write_fd(fds[1], payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = read_two_results(transport);
    loop.schedule(reader);
    loop.run();

    auto [first, second] = reader.result();
    auto first_read = payload_of(std::move(first));
    auto second_read = payload_of(std::move(second));
    ASSERT_TRUE(first_read.has_value());
    ASSERT_TRUE(second_read.has_value());
    EXPECT_EQ(*first_read, first_payload);
    EXPECT_EQ(*second_read, second_payload);
}

// 6.1 Content-Length: 0 → empty string payload
TEST_CASE(empty_payload) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    const std::string data = "Content-Length: 0\r\n\r\n";
    ASSERT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = [&]() -> task<std::optional<std::string>> {
        co_return payload_of(co_await transport.read_message());
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    auto result = read_task.result();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

// 6.2 Header exceeds 8KB limit → TransportClosed
TEST_CASE(header_too_large) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    // Build a header that exceeds 8KB before the \r\n\r\n marker
    std::string huge_header = "Content-Length: 5\r\n";
    huge_header += "X-Padding: ";
    huge_header.append(9000, 'A');
    huge_header += "\r\n\r\nhello";

    std::atomic<bool> writer_done = false;
    std::thread writer([&] {
        EXPECT_EQ(write_fd(fds[1], huge_header.data(), huge_header.size()),
                  static_cast<ssize_t>(huge_header.size()));
        EXPECT_EQ(close_fd(fds[1]), 0);
        writer_done.store(true, std::memory_order_release);
    });

    auto reader = [&]() -> task<ReadEvent> {
        co_return co_await transport.read_message();
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    writer.join();
    EXPECT_TRUE(writer_done.load(std::memory_order_acquire));

    EXPECT_TRUE(std::holds_alternative<TransportClosed>(read_task.result()));
}

// 6.4 Incomplete header (EOF before \r\n\r\n) → TransportClosed
TEST_CASE(incomplete_header) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    const std::string partial = "Content-Length: 10\r\n";
    ASSERT_EQ(write_fd(fds[1], partial.data(), partial.size()),
              static_cast<ssize_t>(partial.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = [&]() -> task<ReadEvent> {
        co_return co_await transport.read_message();
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    EXPECT_TRUE(std::holds_alternative<TransportClosed>(read_task.result()));
}

// 6.5 Content-Length > actual body (EOF mid-body) → TransportClosed
TEST_CASE(length_mismatch) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    // Claim 100 bytes but only provide 5
    const std::string data = "Content-Length: 100\r\n\r\nhello";
    ASSERT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = [&]() -> task<ReadEvent> {
        co_return co_await transport.read_message();
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    EXPECT_TRUE(std::holds_alternative<TransportClosed>(read_task.result()));
}

// 6.6 Rapid sequential writes → all correctly read
TEST_CASE(rapid_sequential) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    std::string combined;
    constexpr int count = 10;
    for(int i = 0; i < count; ++i) {
        combined += frame(R"({"i":)" + std::to_string(i) + "}");
    }

    ASSERT_EQ(write_fd(fds[1], combined.data(), combined.size()),
              static_cast<ssize_t>(combined.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = [&]() -> task<std::vector<std::string>> {
        std::vector<std::string> results;
        for(int i = 0; i < count; ++i) {
            auto msg = payload_of(co_await transport.read_message());
            if(!msg)
                break;
            results.push_back(std::move(*msg));
        }
        co_return results;
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    auto results = read_task.result();
    ASSERT_EQ(results.size(), static_cast<std::size_t>(count));
    for(int i = 0; i < count; ++i) {
        EXPECT_EQ(results[i], R"({"i":)" + std::to_string(i) + "}");
    }
}

// Regression: a single chunk containing header + large payload (> 8KB total)
// must not be rejected by the header size check.  The old code checked
// header.size() before searching for \r\n\r\n, which meant a chunk that
// included payload bytes beyond the header would trip the 8KB limit.
TEST_CASE(large_payload_single_chunk) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));

    // 10KB payload — total frame is well over 8KB, but the header is tiny.
    // Write from a thread because Windows pipe buffers are small (~4KB)
    // and a synchronous write would block before the event loop starts reading.
    std::string payload(10 * 1024, 'x');
    std::string data = frame(payload);

    std::thread writer([&] {
        EXPECT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
        EXPECT_EQ(close_fd(fds[1]), 0);
    });

    auto reader = [&]() -> task<std::optional<std::string>> {
        co_return payload_of(co_await transport.read_message());
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    writer.join();

    auto result = read_task.result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), payload.size());
}

// An oversized frame is drained and reported without losing framing:
// the message after it is delivered intact.
TEST_CASE(oversized_payload_dropped) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));
    transport.max_payload_bytes = 16;

    const std::string oversized(48, 'x');
    const std::string next_payload = R"({"ok":true})";
    const auto data = frame(oversized) + frame(next_payload);

    ASSERT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = read_two_results(transport);
    loop.schedule(reader);
    loop.run();

    const auto [first, second] = reader.result();
    auto* dropped = std::get_if<MessageDropped>(&first);
    ASSERT_TRUE(dropped != nullptr);
    EXPECT_EQ(dropped->announced_bytes, oversized.size());
    EXPECT_EQ(dropped->limit_bytes, std::size_t(16));
    ASSERT_TRUE(std::holds_alternative<std::string>(second));
    EXPECT_EQ(std::get<std::string>(second), next_payload);
}

// A payload of exactly the limit is delivered, not dropped.
TEST_CASE(payload_at_limit_delivered) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));
    transport.max_payload_bytes = 16;

    const std::string payload(16, 'x');
    const auto data = frame(payload);
    ASSERT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = [&]() -> task<std::optional<std::string>> {
        co_return payload_of(co_await transport.read_message());
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    auto result = read_task.result();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, payload);
}

// A drain larger than the 64KB ring buffer takes several read_chunk rounds.
TEST_CASE(oversized_multi_chunk_drain) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));
    transport.max_payload_bytes = 64 * 1024;

    const std::string oversized(200 * 1024, 'x');
    const std::string next_payload = R"({"ok":true})";
    const auto data = frame(oversized) + frame(next_payload);

    // Write from a thread: the pipe buffer is far smaller than the frame,
    // so a synchronous write would block before the loop starts draining.
    std::thread writer([&] {
        EXPECT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
        EXPECT_EQ(close_fd(fds[1]), 0);
    });

    auto reader = read_two_results(transport);
    loop.schedule(reader);
    loop.run();

    writer.join();

    const auto [first, second] = reader.result();
    auto* dropped = std::get_if<MessageDropped>(&first);
    ASSERT_TRUE(dropped != nullptr);
    EXPECT_EQ(dropped->announced_bytes, oversized.size());
    ASSERT_TRUE(std::holds_alternative<std::string>(second));
    EXPECT_EQ(std::get<std::string>(second), next_payload);
}

// EOF in the middle of a drain → TransportClosed, like EOF mid-body.
TEST_CASE(eof_mid_drain) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));
    transport.max_payload_bytes = 16;

    const std::string data = "Content-Length: 48\r\n\r\nonly-ten-b";
    ASSERT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = [&]() -> task<ReadEvent> {
        co_return co_await transport.read_message();
    };

    auto read_task = reader();
    loop.schedule(read_task);
    loop.run();

    EXPECT_TRUE(std::holds_alternative<TransportClosed>(read_task.result()));
}

// Exactly 4x the limit still drains; one byte past it is indistinguishable
// from framing corruption — draining could silently eat the stream — so it
// closes.
TEST_CASE(drain_ceiling_boundary) {
    event_loop loop;

    int fds[2] = {-1, -1};
    ASSERT_EQ(create_pipe(fds), 0);

    auto input = pipe::open(fds[0], pipe::options{}, loop);
    ASSERT_TRUE(input.has_value());

    StreamTransport transport(stream(std::move(*input)));
    transport.max_payload_bytes = 16;

    const std::string at_ceiling(64, 'x');
    const auto data = frame(at_ceiling) + "Content-Length: 65\r\n\r\n";
    ASSERT_EQ(write_fd(fds[1], data.data(), data.size()), static_cast<ssize_t>(data.size()));
    ASSERT_EQ(close_fd(fds[1]), 0);

    auto reader = read_two_results(transport);
    loop.schedule(reader);
    loop.run();

    const auto [first, second] = reader.result();
    auto* dropped = std::get_if<MessageDropped>(&first);
    ASSERT_TRUE(dropped != nullptr);
    EXPECT_EQ(dropped->announced_bytes, at_ceiling.size());
    EXPECT_TRUE(std::holds_alternative<TransportClosed>(second));
}

};  // TEST_SUITE(ipc_transport)

}  // namespace
}  // namespace kota::ipc
