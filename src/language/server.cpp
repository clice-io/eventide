#include "eventide/language/server.h"

namespace eventide::language {

LanguageServer::LanguageServer() = default;

LanguageServer::LanguageServer(std::unique_ptr<Transport> transport) :
    jsonrpc::Peer(std::move(transport)) {}

LanguageServer::~LanguageServer() = default;

}  // namespace eventide::language
