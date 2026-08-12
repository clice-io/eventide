#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "kota/ipc/codec.h"
#include "kota/async/async.h"

namespace kota::ipc {

/// A well-framed incoming message whose announced payload exceeded the
/// receive limit.  The payload bytes were drained off the stream and
/// discarded, so subsequent messages remain readable.
struct MessageDropped {
    std::size_t announced_bytes;
    std::size_t limit_bytes;
};

/// The stream ended (EOF, read error) or the framing was unrecoverably
/// violated.
struct TransportClosed {};

using ReadEvent = std::variant<std::string, MessageDropped, TransportClosed>;

class Transport {
public:
    virtual ~Transport() = default;

    virtual task<ReadEvent> read_message() = 0;

    virtual task<void, Error> write_message(std::string_view payload) = 0;

    virtual Result<void> close_output();

    /// Close both input and output, aborting any pending read.
    virtual Result<void> close();
};

class StreamTransport : public Transport {
public:
    StreamTransport(stream input, stream output);
    explicit StreamTransport(stream stream);

    static Result<std::unique_ptr<StreamTransport>> open_stdio(event_loop& loop);

    static task<std::unique_ptr<StreamTransport>, Error> connect_tcp(std::string_view host,
                                                                     int port,
                                                                     event_loop& loop);

    static Result<std::unique_ptr<StreamTransport>> open_tcp(int fd, event_loop& loop);

    /// Receive cap for a single frame's payload, applied per read.  A frame
    /// announcing more is drained and reported as MessageDropped; one
    /// announcing more than 4x this limit is indistinguishable from framing
    /// corruption and closes the stream instead.
    std::size_t max_payload_bytes = 64 * 1024 * 1024;

    task<ReadEvent> read_message() override;

    task<void, Error> write_message(std::string_view payload) override;

    Result<void> close_output() override;

    Result<void> close() override;

private:
    stream read_stream;
    stream write_stream;
    bool shared_stream = false;
};

}  // namespace kota::ipc
