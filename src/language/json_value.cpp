#include "language/json_value.h"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace eventide::language {

namespace {

rfl::Generic to_generic(yyjson_val* value) {
    if(value == nullptr || yyjson_is_null(value)) {
        return rfl::Generic(rfl::Generic::Null);
    }

    if(yyjson_is_bool(value)) {
        return rfl::Generic(yyjson_get_bool(value));
    }

    if(yyjson_is_int(value)) {
        return rfl::Generic(static_cast<std::int64_t>(yyjson_get_sint(value)));
    }

    if(yyjson_is_num(value)) {
        return rfl::Generic(yyjson_get_num(value));
    }

    if(yyjson_is_str(value)) {
        return rfl::Generic(std::string(yyjson_get_str(value), yyjson_get_len(value)));
    }

    if(yyjson_is_arr(value)) {
        rfl::Generic::Array out;
        yyjson_val* item = nullptr;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(value, &iter);
        while((item = yyjson_arr_iter_next(&iter))) {
            out.emplace_back(to_generic(item));
        }
        return rfl::Generic(std::move(out));
    }

    if(yyjson_is_obj(value)) {
        rfl::Generic::Object obj;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(value, &iter);
        yyjson_val* key = nullptr;
        while((key = yyjson_obj_iter_next(&iter))) {
            auto* val = yyjson_obj_iter_get_val(key);
            if(!val) {
                continue;
            }
            auto name = std::string(yyjson_get_str(key), yyjson_get_len(key));
            obj.insert(std::move(name), to_generic(val));
        }
        return rfl::Generic(std::move(obj));
    }

    return rfl::Generic(rfl::Generic::Null);
}

}  // namespace

json_value json_value::from_json_obj(rfl::json::Reader::InputVarType value) {
    auto* doc = yyjson_mut_doc_new(nullptr);
    if(!doc) {
        throw std::runtime_error("yyjson_mut_doc_new failed");
    }

    yyjson_mut_val* root = nullptr;
    if(!value.val_ || yyjson_is_null(value.val_)) {
        root = yyjson_mut_null(doc);
    } else {
        root = yyjson_val_mut_copy(doc, value.val_);
    }

    if(!root) {
        yyjson_mut_doc_free(doc);
        throw std::runtime_error("yyjson_val_mut_copy failed");
    }

    yyjson_mut_doc_set_root(doc, root);
    return json_value(std::shared_ptr<yyjson_mut_doc>(doc, yyjson_mut_doc_free), root);
}

rfl::Generic json_value::to_generic() const {
    return ::eventide::language::to_generic(reinterpret_cast<yyjson_val*>(value_));
}

std::string json_value::to_string() const {
    if(!doc_) {
        return {};
    }

    size_t len = 0;
    char* json = yyjson_mut_write(doc_.get(), 0, &len);
    if(!json) {
        return {};
    }

    std::string out(json, len);
    free(json);
    return out;
}

json_null json_null::from_json_obj(rfl::json::Reader::InputVarType value) {
    if(value.val_ && !yyjson_is_null(value.val_)) {
        throw std::runtime_error("expected null");
    }
    return {};
}

}  // namespace eventide::language
