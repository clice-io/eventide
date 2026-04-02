#include "eventide/ipc/recording_transport.h"

#include <cassert>
#include <format>
#include <utility>

namespace eventide::ipc {

/// Escape a JSON string value (double-quotes and backslashes).
static void escape_json_string(std::string& out, std::string_view sv) {
    out.push_back('"');
    for(char c: sv) {
        switch(c) {
            case '"': out.append(R"(\")"); break;
            case '\\': out.append(R"(\\)"); break;
            case '\n': out.append(R"(\n)"); break;
            case '\r': out.append(R"(\r)"); break;
            case '\t': out.append(R"(\t)"); break;
            default: out.push_back(c); break;
        }
    }
    out.push_back('"');
}

RecordingTransport::RecordingTransport(std::unique_ptr<Transport> transport, std::string path) :
    inner(std::move(transport)), file(std::move(path), std::ios::binary | std::ios::trunc),
    start(std::chrono::steady_clock::now()) {
    assert(inner && "RecordingTransport requires a non-null inner transport");
    assert(file.is_open() && "RecordingTransport failed to open trace output file");
}

RecordingTransport::~RecordingTransport() = default;

task<std::optional<std::string>> RecordingTransport::read_message() {
    auto msg = co_await inner->read_message();
    if(msg.has_value()) {
        write_record(*msg);
    }
    co_return msg;
}

task<void, Error> RecordingTransport::write_message(std::string_view payload) {
    co_await inner->write_message(payload);
}

Result<void> RecordingTransport::close_output() {
    return inner->close_output();
}

Result<void> RecordingTransport::close() {
    file.close();
    return inner->close();
}

void RecordingTransport::write_record(std::string_view payload) {
    if(!file) {
        return;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::string line;
    line.reserve(payload.size() + 64);
    line.append(std::format(R"({{"ts":{},"msg":)", ms));
    escape_json_string(line, payload);
    line.append("}\n");

    file.write(line.data(), static_cast<std::streamsize>(line.size()));
    file.flush();
}

}  // namespace eventide::ipc
