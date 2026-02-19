#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "eventide/stream.h"
#include "eventide/task.h"

namespace language {

namespace et = eventide;

class Transport {
public:
    virtual ~Transport() = default;

    virtual et::task<std::optional<std::string>> read_message() = 0;

    virtual et::task<bool> write_message(std::string_view payload) = 0;
};

class StreamTransport : public Transport {
public:
    StreamTransport(et::stream input, et::stream output);
    explicit StreamTransport(et::stream stream);

    static std::expected<std::unique_ptr<StreamTransport>, std::string>
        open_stdio(et::event_loop& loop);

    static et::task<std::expected<std::unique_ptr<StreamTransport>, std::string>>
        connect_tcp(std::string_view host, int port, et::event_loop& loop);

    static std::expected<std::unique_ptr<StreamTransport>, std::string>
        open_tcp(int fd, et::event_loop& loop);

    et::task<std::optional<std::string>> read_message() override;

    et::task<bool> write_message(std::string_view payload) override;

private:
    et::stream read_stream;
    et::stream write_stream;
    bool shared_stream = false;
};

}  // namespace language
