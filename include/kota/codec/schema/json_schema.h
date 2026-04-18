#pragma once

#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "kota/meta/type_info.h"
#include "kota/support/naming.h"

namespace kota::codec::schema::json_schema {

class emitter {
    using tk = meta::type_kind;

public:
    std::string emit(const meta::type_info& root) {
        root_ti = unwrap(&root);
        if(root_ti->kind == tk::structure) {
            used_names.insert(kota::naming::normalize_identifier(root_ti->type_name));
        }

        out += '{';
        key("$schema");
        str("https://json-schema.org/draft/2020-12/schema");

        write_schema_fields(&root);

        if(!defs.empty()) {
            out += ',';
            key("$defs");
            out += '{';
            for(std::size_t i = 0; i < defs.size(); ++i) {
                if(i > 0) {
                    out += ',';
                }
                key(defs[i].first);
                out += defs[i].second;
            }
            out += '}';
        }

        out += '}';
        return std::move(out);
    }

private:
    static const meta::type_info* unwrap(const meta::type_info* ti) {
        while(ti->kind == tk::optional || ti->kind == tk::pointer) {
            ti = &static_cast<const meta::optional_type_info*>(ti)->inner();
        }
        return ti;
    }

    static void escape_json(std::string& buf, std::string_view sv) {
        buf += '"';
        for(char c: sv) {
            switch(c) {
                case '"': buf += "\\\""; break;
                case '\\': buf += "\\\\"; break;
                case '\b': buf += "\\b"; break;
                case '\f': buf += "\\f"; break;
                case '\n': buf += "\\n"; break;
                case '\r': buf += "\\r"; break;
                case '\t': buf += "\\t"; break;
                default:
                    if(static_cast<unsigned char>(c) < 0x20) {
                        std::format_to(std::back_inserter(buf),
                                       R"(\u{:04x})",
                                       static_cast<unsigned char>(c));
                    } else {
                        buf += c;
                    }
                    break;
            }
        }
        buf += '"';
    }

    void key(std::string_view k) {
        escape_json(out, k);
        out += ':';
    }

    void str(std::string_view s) {
        escape_json(out, s);
    }

    static std::string alternative_name(const meta::variant_type_info* vi, std::size_t i) {
        if(i < vi->alt_names.size()) {
            return std::string(vi->alt_names[i]);
        }
        return kota::naming::normalize_identifier(vi->alternatives[i]().type_name);
    }

    const std::string& canonical_name(const meta::type_info* ti) {
        auto it = def_names.find(ti);
        if(it != def_names.end()) {
            return it->second;
        }
        auto base = kota::naming::normalize_identifier(ti->type_name);
        auto name = base;
        for(std::size_t counter = 2; used_names.contains(name); ++counter) {
            name = std::format("{}_{}", base, counter);
        }
        used_names.insert(name);
        return def_names.emplace(ti, std::move(name)).first->second;
    }

    template <typename F>
    std::string render_fragment(F&& f) {
        auto saved = std::move(out);
        out.clear();
        f();
        auto fragment = std::move(out);
        out = std::move(saved);
        return fragment;
    }

    void write_schema(const meta::type_info* ti) {
        ti = unwrap(ti);

        switch(ti->kind) {
            case tk::null: write_type("null"); return;
            case tk::boolean: write_type("boolean"); return;
            case tk::character:
            case tk::string:
            case tk::bytes: write_type("string"); return;
            case tk::float32:
            case tk::float64: write_type("number"); return;
            case tk::int8:
                write_integer(std::numeric_limits<std::int8_t>::min(),
                              std::numeric_limits<std::int8_t>::max());
                return;
            case tk::int16:
                write_integer(std::numeric_limits<std::int16_t>::min(),
                              std::numeric_limits<std::int16_t>::max());
                return;
            case tk::int32:
                write_integer(std::numeric_limits<std::int32_t>::min(),
                              std::numeric_limits<std::int32_t>::max());
                return;
            case tk::int64:
                write_integer(std::numeric_limits<std::int64_t>::min(),
                              std::numeric_limits<std::int64_t>::max());
                return;
            case tk::uint8: write_unsigned(std::numeric_limits<std::uint8_t>::max()); return;
            case tk::uint16: write_unsigned(std::numeric_limits<std::uint16_t>::max()); return;
            case tk::uint32: write_unsigned(std::numeric_limits<std::uint32_t>::max()); return;
            case tk::uint64: write_unsigned(std::numeric_limits<std::uint64_t>::max()); return;
            case tk::enumeration: write_enum(ti); return;
            case tk::array:
            case tk::set: write_array(ti); return;
            case tk::map: write_map(ti); return;
            case tk::tuple: write_tuple(ti); return;
            case tk::structure: write_struct_ref(ti); return;
            case tk::variant: write_variant(ti); return;
            default: out += "{}"; return;
        }
    }

    void write_schema_fields(const meta::type_info* ti) {
        ti = unwrap(ti);
        if(ti->kind == tk::structure) {
            out += ',';
            write_struct_body(static_cast<const meta::struct_type_info*>(ti));
            return;
        }
        auto fragment = render_fragment([&] { write_schema(ti); });
        if(fragment.size() > 2) {
            out += ',';
            out.append(fragment, 1, fragment.size() - 2);
        }
    }

    void write_struct_body(const meta::struct_type_info* si) {
        key("type");
        str("object");
        out += ',';
        key("properties");
        write_properties(si);
        write_required(si);
        if(si->deny_unknown) {
            out += ',';
            key("additionalProperties");
            out += "false";
        }
    }

    void write_type(std::string_view type_name) {
        out += '{';
        key("type");
        str(type_name);
        out += '}';
    }

    void write_integer(std::int64_t min_val, std::int64_t max_val) {
        std::format_to(std::back_inserter(out),
                       R"({{"type":"integer","minimum":{},"maximum":{}}})",
                       min_val,
                       max_val);
    }

    void write_unsigned(std::uint64_t max_val) {
        std::format_to(std::back_inserter(out),
                       R"({{"type":"integer","minimum":0,"maximum":{}}})",
                       max_val);
    }

    void write_enum(const meta::type_info* ti) {
        auto* ei = static_cast<const meta::enum_type_info*>(ti);
        out += '{';
        key("enum");
        out += '[';
        for(std::size_t i = 0; i < ei->member_names.size(); ++i) {
            if(i > 0) {
                out += ',';
            }
            str(ei->member_names[i]);
        }
        out += ']';
        out += '}';
    }

    void write_array(const meta::type_info* ti) {
        auto* ai = static_cast<const meta::array_type_info*>(ti);
        out += '{';
        key("type");
        str("array");
        out += ',';
        key("items");
        write_schema(&ai->element());
        if(ti->kind == tk::set) {
            out += ',';
            key("uniqueItems");
            out += "true";
        }
        out += '}';
    }

    void write_map(const meta::type_info* ti) {
        auto* mi = static_cast<const meta::map_type_info*>(ti);
        out += '{';
        key("type");
        str("object");
        out += ',';
        key("additionalProperties");
        write_schema(&mi->value());
        out += '}';
    }

    void write_tuple(const meta::type_info* ti) {
        auto* tup = static_cast<const meta::tuple_type_info*>(ti);
        out += '{';
        key("type");
        str("array");
        out += ',';
        key("prefixItems");
        out += '[';
        for(std::size_t i = 0; i < tup->elements.size(); ++i) {
            if(i > 0) {
                out += ',';
            }
            write_schema(&tup->elements[i]());
        }
        out += ']';
        out += '}';
    }

    void write_struct_ref(const meta::type_info* ti) {
        if(ti == root_ti) {
            out += '{';
            key("$ref");
            str("#");
            out += '}';
            return;
        }
        const auto& name = canonical_name(ti);
        ensure_struct_def(ti);
        out += '{';
        key("$ref");
        str(std::format("#/$defs/{}", name));
        out += '}';
    }

    void ensure_struct_def(const meta::type_info* ti) {
        if(!emitted.insert(ti).second) {
            return;
        }
        auto* si = static_cast<const meta::struct_type_info*>(ti);
        auto body = render_fragment([&] {
            out += '{';
            write_struct_body(si);
            out += '}';
        });
        defs.emplace_back(canonical_name(ti), std::move(body));
    }

    void write_properties(const meta::struct_type_info* si) {
        out += '{';
        for(std::size_t i = 0; i < si->fields.size(); ++i) {
            if(i > 0) {
                out += ',';
            }
            key(si->fields[i].name);
            write_schema(&si->fields[i].type());
        }
        out += '}';
    }

    void write_required(const meta::struct_type_info* si) {
        bool first = true;
        for(const auto& f: si->fields) {
            const meta::type_info& ft = f.type();
            bool is_optional = f.has_default || ft.kind == tk::optional || ft.kind == tk::pointer;
            if(is_optional) {
                continue;
            }
            if(first) {
                out += ',';
                key("required");
                out += '[';
                first = false;
            } else {
                out += ',';
            }
            str(f.name);
        }
        if(!first) {
            out += ']';
        }
    }

    void write_tag_const(std::string_view tag_field, std::string_view alt_name) {
        out += '{';
        key("properties");
        out += '{';
        key(tag_field);
        out += '{';
        key("const");
        str(alt_name);
        out += '}';
        out += '}';
        out += ',';
        key("required");
        out += '[';
        str(tag_field);
        out += ']';
        out += '}';
    }

    void write_variant(const meta::type_info* ti) {
        auto* vi = static_cast<const meta::variant_type_info*>(ti);
        out += '{';
        key("oneOf");
        out += '[';

        for(std::size_t i = 0; i < vi->alternatives.size(); ++i) {
            if(i > 0) {
                out += ',';
            }

            switch(vi->tagging) {
                case meta::tag_mode::none: write_schema(&vi->alternatives[i]()); break;

                case meta::tag_mode::external: {
                    auto alt_name = alternative_name(vi, i);
                    out += '{';
                    key("type");
                    str("object");
                    out += ',';
                    key("properties");
                    out += '{';
                    key(alt_name);
                    write_schema(&vi->alternatives[i]());
                    out += '}';
                    out += ',';
                    key("required");
                    out += '[';
                    str(alt_name);
                    out += ']';
                    out += ',';
                    key("additionalProperties");
                    out += "false";
                    out += '}';
                    break;
                }

                case meta::tag_mode::internal: {
                    auto alt_name = alternative_name(vi, i);
                    out += '{';
                    key("allOf");
                    out += '[';
                    write_schema(&vi->alternatives[i]());
                    out += ',';
                    write_tag_const(vi->tag_field, alt_name);
                    out += ']';
                    out += '}';
                    break;
                }

                case meta::tag_mode::adjacent: {
                    auto alt_name = alternative_name(vi, i);
                    out += '{';
                    key("type");
                    str("object");
                    out += ',';
                    key("properties");
                    out += '{';
                    key(vi->tag_field);
                    out += '{';
                    key("const");
                    str(alt_name);
                    out += '}';
                    out += ',';
                    key(vi->content_field);
                    write_schema(&vi->alternatives[i]());
                    out += '}';
                    out += ',';
                    key("required");
                    out += '[';
                    str(vi->tag_field);
                    out += ',';
                    str(vi->content_field);
                    out += ']';
                    out += ',';
                    key("additionalProperties");
                    out += "false";
                    out += '}';
                    break;
                }
            }
        }

        out += ']';
        out += '}';
    }

    std::string out;
    std::vector<std::pair<std::string, std::string>> defs;
    std::unordered_map<const meta::type_info*, std::string> def_names;
    std::unordered_set<std::string> used_names;
    std::unordered_set<const meta::type_info*> emitted;
    const meta::type_info* root_ti = nullptr;
};

inline std::string render(const meta::type_info& root) {
    return emitter{}.emit(root);
}

}  // namespace kota::codec::schema::json_schema
