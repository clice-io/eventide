#include "language/server.h"

#include "eventide/stream.h"

namespace language {

namespace et = eventide;

std::string_view trim(std::string_view value) {
    auto start = value.find_first_not_of(" \t");
    if(start == std::string_view::npos) {
        return {};
    }
    auto end = value.find_last_not_of(" \t");
    return value.substr(start, end - start + 1);
}

bool iequals(std::string_view lhs, std::string_view rhs) {
    if(lhs.size() != rhs.size()) {
        return false;
    }
    for(std::size_t i = 0; i < lhs.size(); ++i) {
        if(std::tolower(static_cast<unsigned char>(lhs[i])) !=
           std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<std::size_t> parse_content_length(std::string_view header) {
    std::size_t pos = 0;
    while(pos < header.size()) {
        auto end = header.find("\r\n", pos);
        if(end == std::string_view::npos) {
            break;
        }
        auto line = header.substr(pos, end - pos);
        pos = end + 2;
        auto sep = line.find(':');
        if(sep == std::string_view::npos) {
            continue;
        }
        auto name = trim(line.substr(0, sep));
        if(!iequals(name, "Content-Length")) {
            continue;
        }
        auto value = trim(line.substr(sep + 1));
        if(value.empty()) {
            return std::nullopt;
        }
        std::size_t length = 0;
        for(char ch: value) {
            if(ch < '0' || ch > '9') {
                return std::nullopt;
            }
            length = length * 10 + static_cast<std::size_t>(ch - '0');
        }
        return length;
    }
    return std::nullopt;
}

struct LanguageServer::Self {
    et::event_loop loop;
    et::stream io;

    et::task<std::optional<std::string>> read_message() {
        std::string header;
        std::optional<std::size_t> content_length;

        while(!content_length.has_value()) {
            auto chunk = co_await io.read_chunk();
            if(chunk.empty()) {
                co_return std::nullopt;
            }

            auto prior = header.size();
            header.append(chunk.data(), chunk.size());

            auto marker = header.find("\r\n\r\n");
            if(marker == std::string::npos) {
                io.consume(chunk.size());
                continue;
            }

            auto header_end = marker + 4;
            auto before_chunk = prior;
            auto header_from_chunk = header_end > before_chunk ? header_end - before_chunk : 0;
            io.consume(header_from_chunk);

            auto header_view = std::string_view(header.data(), header_end);
            content_length = parse_content_length(header_view);
            if(!content_length.has_value()) {
                co_return std::nullopt;
            }
            break;
        }

        std::string payload;
        payload.reserve(*content_length);

        while(payload.size() < *content_length) {
            auto chunk = co_await io.read_chunk();
            if(chunk.empty()) {
                co_return std::nullopt;
            }

            auto need = *content_length - payload.size();
            auto take = std::min<std::size_t>(need, chunk.size());
            payload.append(chunk.data(), take);
            io.consume(take);
        }

        co_return payload;
    }

    et::task<> main_loop() {
        while(true) {
            auto payload = co_await read_message();
            if(!payload.has_value()) {
                co_return;
            }

            /// lazy parse payload ...
            /// request? -->
            /// notification? -->
            /// response? -->

            /// create and schedule the task ...
        }
    }
};

int LanguageServer::start() {
    self->loop.schedule(self->main_loop());
    return self->loop.run();
}

}  // namespace language
