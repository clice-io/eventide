#include "language/json_rpc.h"

#include <memory>

namespace eventide::language::json_rpc {

namespace {

struct doc_deleter {
    void operator()(yyjson_doc* doc) const noexcept {
        yyjson_doc_free(doc);
    }
};

}  // namespace

result<message> parse(std::string_view payload) {
    yyjson_read_err err;
    yyjson_doc* raw =
        yyjson_read_opts(const_cast<char*>(payload.data()), payload.size(), 0, nullptr, &err);
    if(!raw) {
        return std::unexpected(error::invalid_argument);
    }

    std::shared_ptr<yyjson_doc> doc(raw, doc_deleter{});
    yyjson_val* root = yyjson_doc_get_root(raw);
    if(!root || !yyjson_is_obj(root)) {
        return std::unexpected(error::invalid_argument);
    }

    yyjson_val* method = yyjson_obj_get(root, "method");
    if(!method || !yyjson_is_str(method)) {
        return std::unexpected(error::invalid_argument);
    }

    message out{};
    out.method.assign(yyjson_get_str(method), yyjson_get_len(method));

    yyjson_val* id_val = yyjson_obj_get(root, "id");
    if(id_val && !yyjson_is_null(id_val)) {
        if(yyjson_is_int(id_val)) {
            out.request_id = id{static_cast<std::int64_t>(yyjson_get_sint(id_val))};
        } else if(yyjson_is_str(id_val)) {
            out.request_id = id{std::string(yyjson_get_str(id_val), yyjson_get_len(id_val))};
        } else {
            return std::unexpected(error::invalid_argument);
        }
    }

    out.params = yyjson_obj_get(root, "params");
    out.doc = std::move(doc);
    return out;
}

}  // namespace eventide::language::json_rpc
