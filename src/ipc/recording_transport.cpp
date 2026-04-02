#include "eventide/ipc/recording_transport.h"

#include <format>
#include <utility>

namespace eventide::ipc {

RecordingTransport::RecordingTransport(std::unique_ptr<Transport> inner, std::string path) :
    inner_(std::move(inner)), file_(std::move(path), std::ios::binary | std::ios::trunc) {}

RecordingTransport::~RecordingTransport() = default;

task<std::optional<std::string>> RecordingTransport::read_message() {
    auto msg = co_await inner_->read_message();
    if(msg.has_value()) {
        write_framed(*msg);
    }
    co_return msg;
}

task<void, Error> RecordingTransport::write_message(std::string_view payload) {
    co_await inner_->write_message(payload);
}

Result<void> RecordingTransport::close_output() {
    return inner_->close_output();
}

Result<void> RecordingTransport::close() {
    file_.close();
    return inner_->close();
}

void RecordingTransport::write_framed(std::string_view payload) {
    auto header = std::format("Content-Length: {}\r\n\r\n", payload.size());
    file_.write(header.data(), static_cast<std::streamsize>(header.size()));
    file_.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    file_.flush();
}

}  // namespace eventide::ipc
