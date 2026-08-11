#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "kota/support/expected_try.h"
#include "kota/support/naming.h"
#include "kota/meta/type_info.h"
#include "kota/codec/dyn/decode.h"
#include "kota/codec/dyn/document.h"
#include "kota/codec/dyn/encode.h"
#include "kota/codec/json/json.h"
#include "kota/codec/visit/config.h"

namespace kota::codec::json {

/// Config::enum_rename as a plain function: the schema emitter runs
/// type-erased, so the typed entry points pass apply_enum_rename<Config> down
/// as a pointer and the emitter applies it the way the encoder does.
using enum_rename_fn = std::string (*)(bool is_serialize, std::string_view name);

/// The config knobs that shape the documents the encoder emits — and thus the
/// schema. The typed entry points derive them from the codec config; the
/// type-erased ones default to the codec defaults.
struct schema_options {
    codec::enum_repr enums = default_config<>::enum_repr;
    enum_rename_fn rename = apply_enum_rename<void>;
    codec::nan_repr nan = default_config<>::nan_repr;
    /// A non-human-readable config bypasses variant tagging and encodes the
    /// underlying untagged variant (the is_human_readable gate in
    /// encode_value), so the schema must drop the tag shape the same way.
    bool human_readable = true;
};

namespace detail {

class SchemaEmitter {
    using tk = meta::type_kind;
    using result_t = std::expected<dyn::Value, error>;

public:
    explicit SchemaEmitter(const schema_options& opts) : opts(opts) {}

    result_t emit(const meta::type_info& root) {
        root_ti = unwrap(&root);
        if(root_ti->kind == tk::structure) {
            auto name = kota::naming::normalize_identifier(root_ti->type_name);
            used_names.insert(name);
            def_names.emplace(root_ti, std::move(name));
        }

        dyn::Object schema;
        schema.insert("$schema", "https://json-schema.org/draft/2020-12/schema");

        KOTA_EXPECTED_TRY(merge_schema_fields(schema, &root));

        if(!defs.empty()) {
            dyn::Object defs_obj;
            for(auto& [name, value]: defs) {
                defs_obj.insert(std::move(name), std::move(value));
            }
            schema.insert("$defs", std::move(defs_obj));
        }

        return dyn::Value(std::move(schema));
    }

private:
    const static meta::type_info* unwrap(const meta::type_info* ti) {
        while(ti->kind == tk::optional || ti->kind == tk::pointer) {
            ti = &static_cast<const meta::optional_type_info*>(ti)->inner();
        }
        return ti;
    }

    static std::string alternative_name(const meta::variant_type_info* vi, std::size_t i) {
        if(i < vi->alt_names.size()) {
            return std::string(vi->alt_names[i]);
        }
        return kota::naming::normalize_identifier(vi->alternatives[i]().type_name);
    }

    std::expected<std::string_view, error> def_name(const meta::type_info* ti) {
        auto it = def_names.find(ti);
        if(it != def_names.end()) {
            return std::string_view(it->second);
        }
        auto name = kota::naming::normalize_identifier(ti->type_name);
        if(!used_names.insert(name).second) {
            return std::unexpected(rich_error(
                std::format("duplicate $defs name '{}' from type '{}'", name, ti->type_name)));
        }
        auto [pos, _] = def_names.emplace(ti, std::move(name));
        return std::string_view(pos->second);
    }

    result_t make_nullable(const meta::type_info* ti) {
        auto* inner = unwrap(ti);
        KOTA_EXPECTED_TRY_V(auto schema, make_schema(inner));
        // anyOf, not oneOf: the inner schema may itself admit null (a
        // monostate alternative, an unconstrained any), and a null document
        // must not fail by matching both branches.
        return dyn::Value{
            {"anyOf", dyn::Array{std::move(schema), dyn::Value{{"type", "null"}}}},
        };
    }

    result_t make_schema(const meta::type_info* ti) {
        if(ti->kind == tk::optional || ti->kind == tk::pointer) {
            return make_nullable(ti);
        }

        switch(ti->kind) {
            case tk::null:
                return dyn::Value{
                    {"type", "null"}
                };
            case tk::boolean:
                return dyn::Value{
                    {"type", "boolean"}
                };
            case tk::character:
            case tk::string:
                return dyn::Value{
                    {"type", "string"}
                };
            case tk::bytes: {
                // visit_bytes emits an array of octets, not a string.
                KOTA_EXPECTED_TRY_V(auto items, make_integer(0, 255));
                return dyn::Value{
                    {"type",  "array"         },
                    {"items", std::move(items)},
                };
            }
            case tk::float32:
            case tk::float64: return make_float();
            case tk::int8:
            case tk::int16:
            case tk::int32:
            case tk::int64:
            case tk::uint8:
            case tk::uint16:
            case tk::uint32:
            case tk::uint64: return make_integer_kind(ti->kind);
            case tk::enumeration: return make_enum(ti);
            case tk::array:
            case tk::set: return make_array(ti);
            case tk::map: return make_map(ti);
            case tk::tuple: return make_tuple(ti);
            case tk::structure: return make_struct_ref(ti);
            case tk::variant: return make_variant(ti);
            case tk::any: return dyn::Value(dyn::Object{});
            default:
                return std::unexpected(
                    rich_error(std::format("unsupported type kind '{}' for JSON Schema generation",
                                           ti->type_name)));
        }
    }

    std::expected<void, error> merge_schema_fields(dyn::Object& target, const meta::type_info* ti) {
        ti = unwrap(ti);
        if(ti->kind == tk::structure) {
            return add_struct_body(target, static_cast<const meta::struct_type_info*>(ti));
        }
        KOTA_EXPECTED_TRY_V(auto schema, make_schema(ti));
        if(auto* obj = schema.get_object()) {
            for(auto& [k, v]: *obj) {
                target.insert(std::string(k), std::move(v));
            }
        }
        return {};
    }

    std::expected<void, error> add_struct_body(dyn::Object& target,
                                               const meta::struct_type_info* si) {
        target.insert("type", "object");
        KOTA_EXPECTED_TRY_V(auto props, make_properties(si));
        target.insert("properties", std::move(props));
        add_required(target, si);
        if(si->deny_unknown) {
            target.insert("additionalProperties", false);
        }
        return {};
    }

    static result_t make_integer(std::int64_t min_val, std::int64_t max_val) {
        return dyn::Value{
            {"type",    "integer"},
            {"minimum", min_val  },
            {"maximum", max_val  },
        };
    }

    static result_t make_unsigned(std::uint64_t max_val) {
        return dyn::Value{
            {"type",    "integer"       },
            {"minimum", std::uint64_t{0}},
            {"maximum", max_val         },
        };
    }

    static result_t make_integer_kind(meta::type_kind kind) {
        switch(kind) {
            case tk::int8:
                return make_integer(std::numeric_limits<std::int8_t>::min(),
                                    std::numeric_limits<std::int8_t>::max());
            case tk::int16:
                return make_integer(std::numeric_limits<std::int16_t>::min(),
                                    std::numeric_limits<std::int16_t>::max());
            case tk::int32:
                return make_integer(std::numeric_limits<std::int32_t>::min(),
                                    std::numeric_limits<std::int32_t>::max());
            case tk::int64:
                return make_integer(std::numeric_limits<std::int64_t>::min(),
                                    std::numeric_limits<std::int64_t>::max());
            case tk::uint8: return make_unsigned(std::numeric_limits<std::uint8_t>::max());
            case tk::uint16: return make_unsigned(std::numeric_limits<std::uint16_t>::max());
            case tk::uint32: return make_unsigned(std::numeric_limits<std::uint32_t>::max());
            case tk::uint64: return make_unsigned(std::numeric_limits<std::uint64_t>::max());
            default:
                // char/bool/extended-char underlying: still a number in the
                // document, but without portable bounds here.
                return dyn::Value{
                    {"type", "integer"}
                };
        }
    }

    /// The encoder's non-finite handling is part of the document contract:
    /// nan_repr::Null encodes NaN/Infinity as null — and so does Passthrough,
    /// which forwards the value to the JSON writer, whose only spelling for a
    /// non-finite number is null. nan_repr::String encodes one of three fixed
    /// strings. Only Error never emits a non-finite value, so it alone keeps
    /// the plain number.
    result_t make_float() const {
        dyn::Value number{
            {"type", "number"}
        };
        switch(opts.nan) {
            case nan_repr::Passthrough:
            case nan_repr::Null:
                return dyn::Value{
                    {"anyOf", dyn::Array{std::move(number), dyn::Value{{"type", "null"}}}},
                };
            case nan_repr::String:
                return dyn::Value{
                    {"anyOf",
                     dyn::Array{std::move(number),
                                dyn::Value{{"enum", dyn::Array{"NaN", "Infinity", "-Infinity"}}}}},
                };
            default: return number;  // Error: no non-finite document exists.
        }
    }

    result_t make_enum(const meta::type_info* ti) const {
        auto* ei = static_cast<const meta::enum_type_info*>(ti);
        if(opts.enums != enum_repr::String) {
            // enum_repr::Integer: the codec casts through the underlying type
            // without checking membership, and name reflection only covers a
            // limited scan range — values outside it still encode. The honest
            // constraint is the underlying integer's range, not a value list.
            return make_integer_kind(ei->underlying_kind);
        }
        // Exhaustive: a value without a reflected member name has no string
        // spelling, so the encoder rejects it instead of emitting one.
        dyn::Array values;
        for(const auto& name: ei->member_names) {
            values.push_back(dyn::Value(opts.rename(true, name)));
        }
        return dyn::Value{
            {"enum", std::move(values)}
        };
    }

    result_t make_array(const meta::type_info* ti) {
        auto* ai = static_cast<const meta::array_type_info*>(ti);
        KOTA_EXPECTED_TRY_V(auto items, make_schema(&ai->element()));
        dyn::Object obj{
            {"type",  "array"         },
            {"items", std::move(items)},
        };
        if(ti->kind == tk::set) {
            obj.insert("uniqueItems", true);
        }
        return dyn::Value(std::move(obj));
    }

    result_t make_map(const meta::type_info* ti) {
        auto* mi = static_cast<const meta::map_type_info*>(ti);
        KOTA_EXPECTED_TRY_V(auto val_schema, make_schema(&mi->value()));
        return dyn::Value{
            {"type",                 "object"             },
            {"additionalProperties", std::move(val_schema)},
        };
    }

    result_t make_tuple(const meta::type_info* ti) {
        auto* tup = static_cast<const meta::tuple_type_info*>(ti);
        dyn::Array items;
        for(std::size_t i = 0; i < tup->elements.size(); ++i) {
            KOTA_EXPECTED_TRY_V(auto elem, make_schema(&tup->elements[i]()));
            items.push_back(std::move(elem));
        }
        auto size = static_cast<std::uint64_t>(tup->elements.size());
        return dyn::Value{
            {"type",        "array"         },
            {"prefixItems", std::move(items)},
            {"items",       false           },
            {"minItems",    size            },
            {"maxItems",    size            },
        };
    }

    result_t make_struct_ref(const meta::type_info* ti) {
        if(ti == root_ti) {
            return dyn::Value{
                {"$ref", "#"}
            };
        }
        KOTA_EXPECTED_TRY_V(auto name, def_name(ti));
        KOTA_EXPECTED_TRY(ensure_struct_def(ti));
        return dyn::Value{
            {"$ref", std::format("#/$defs/{}", name)}
        };
    }

    std::expected<void, error> ensure_struct_def(const meta::type_info* ti) {
        if(!emitted.insert(ti).second) {
            return {};
        }
        auto* si = static_cast<const meta::struct_type_info*>(ti);
        KOTA_EXPECTED_TRY_V(auto name, def_name(ti));
        dyn::Object body;
        KOTA_EXPECTED_TRY(add_struct_body(body, si));
        defs.emplace_back(std::string(name), std::move(body));
        return {};
    }

    result_t make_properties(const meta::struct_type_info* si) {
        dyn::Object props;
        for(const auto& f: si->fields) {
            KOTA_EXPECTED_TRY_V(auto schema, make_schema(&f.type()));
            if(!f.description.empty()) {
                schema.get_object()->insert("description", f.description);
            }
            props.insert(std::string(f.name), std::move(schema));
        }
        return dyn::Value(std::move(props));
    }

    /// A field is required only when it always appears in the output: no decode
    /// default, no encode-side skip condition (built-in skip_when or custom
    /// predicate — the decoder accepts absence for both), not nullable. The
    /// declared field type decides nullability, not the representation: a repr or
    /// behavior attr with a nullable representation still rejects an absent
    /// property on decode.
    static bool is_required(const meta::field_info& f) {
        return !f.has_default && !f.has_skip_if && !f.nullable;
    }

    static void add_required(dyn::Object& target, const meta::struct_type_info* si) {
        dyn::Array required;
        for(const auto& f: si->fields) {
            if(is_required(f)) {
                required.push_back(dyn::Value(f.name));
            }
        }
        if(!required.empty()) {
            target.insert("required", std::move(required));
        }
    }

    static dyn::Value make_tag_const(std::string_view tag_field, std::string_view alt_name) {
        return {
            {"properties", {{std::string(tag_field), {{"const", alt_name}}}}},
            {"required",   dyn::Array{dyn::Value(tag_field)}                },
        };
    }

    result_t make_internal_tagged(const meta::type_info* ti,
                                  std::string_view tag_field,
                                  std::string_view alt_name) {
        ti = unwrap(ti);
        if(ti->kind == tk::structure) {
            auto* si = static_cast<const meta::struct_type_info*>(ti);
            KOTA_EXPECTED_TRY_V(auto props, make_properties(si));
            auto* props_obj = props.get_object();
            props_obj->insert(std::string(tag_field),
                              dyn::Value{
                                  {"const", alt_name}
            });
            dyn::Object obj;
            obj.insert("type", "object");
            obj.insert("properties", std::move(props));
            dyn::Array required;
            for(const auto& f: si->fields) {
                if(is_required(f)) {
                    required.push_back(dyn::Value(f.name));
                }
            }
            required.push_back(dyn::Value(tag_field));
            obj.insert("required", std::move(required));
            if(si->deny_unknown) {
                obj.insert("additionalProperties", false);
            }
            return dyn::Value(std::move(obj));
        }
        KOTA_EXPECTED_TRY_V(auto schema, make_schema(ti));
        return dyn::Value{
            {"allOf",
             dyn::Array{
                 std::move(schema),
                 make_tag_const(tag_field, alt_name),
             }},
        };
    }

    result_t make_variant(const meta::type_info* ti) {
        auto* vi = static_cast<const meta::variant_type_info*>(ti);
        auto tagging = opts.human_readable ? vi->tagging : meta::tag_mode::none;
        dyn::Array alts;

        for(std::size_t i = 0; i < vi->alternatives.size(); ++i) {
            switch(tagging) {
                case meta::tag_mode::none: {
                    KOTA_EXPECTED_TRY_V(auto schema, make_schema(&vi->alternatives[i]()));
                    alts.push_back(std::move(schema));
                    break;
                }

                case meta::tag_mode::external: {
                    auto alt_name = alternative_name(vi, i);
                    KOTA_EXPECTED_TRY_V(auto schema, make_schema(&vi->alternatives[i]()));
                    alts.push_back({
                        {"type",                 "object"                        },
                        {"properties",           {{alt_name, std::move(schema)}} },
                        {"required",             dyn::Array{dyn::Value(alt_name)}},
                        {"additionalProperties", false                           },
                    });
                    break;
                }

                case meta::tag_mode::internal: {
                    auto alt_name = alternative_name(vi, i);
                    KOTA_EXPECTED_TRY_V(
                        auto schema,
                        make_internal_tagged(&vi->alternatives[i](), vi->tag_field, alt_name));
                    alts.push_back(std::move(schema));
                    break;
                }

                case meta::tag_mode::adjacent: {
                    auto alt_name = alternative_name(vi, i);
                    KOTA_EXPECTED_TRY_V(auto schema, make_schema(&vi->alternatives[i]()));
                    alts.push_back({
                        {"type",                 "object"},
                        {"properties",
                         dyn::Object{
                             {std::string(vi->tag_field), {{"const", alt_name}}},
                             {std::string(vi->content_field), std::move(schema)},
                         }                               },
                        {"required",
                         dyn::Array{
                             dyn::Value(vi->tag_field),
                             dyn::Value(vi->content_field),
                         }                               },
                        {"additionalProperties", false   },
                    });
                    break;
                }
            }
        }

        // Untagged alternatives can overlap — a numeric enum's underlying
        // range next to an integer both match the same document — so only the
        // tagged forms, disjoint by their tag, claim exactly-one semantics.
        return dyn::Value{
            {tagging == meta::tag_mode::none ? "anyOf" : "oneOf", std::move(alts)},
        };
    }

    schema_options opts;
    std::vector<std::pair<std::string, dyn::Value>> defs;
    std::unordered_map<const meta::type_info*, std::string> def_names;
    std::unordered_set<std::string> used_names;
    std::unordered_set<const meta::type_info*> emitted;
    const meta::type_info* root_ti = nullptr;
};

/// Walks the emitted schema alongside the document a default-constructed
/// instance encodes to, annotating optional properties (those absent from
/// "required") with a `default` — the value decode leaves behind when the
/// property is absent. Required properties must always appear, so a default
/// annotation would be a lie; they are skipped, as are properties the default
/// document does not carry (encode-side skip conditions). Property-level
/// struct $refs recurse so defaults land on the leaf fields inside $defs; a
/// $def shared by several sites keeps the values of its first visit, while
/// each non-required ref site also carries its whole encoded object as its
/// own default, so per-site member initializers survive the sharing. A
/// tagged variant (a oneOf) descends into the branch whose tag constraints
/// the document satisfies — the encoded alternative — so its members keep
/// their decode defaults. An anyOf wrapper (optional/pointer field, untagged
/// variant) is a leaf: it takes the whole encoded value as its default, so a
/// $def reachable only through nullable fields stays unannotated.
class DefaultAnnotator {
public:
    explicit DefaultAnnotator(dyn::Object& root) {
        if(auto* d = root.find("$defs")) {
            defs = d->get_object();
        }
    }

    void annotate(dyn::Object& body, const dyn::Value& value) {
        // Every walked body pairs with an object document: a struct schema
        // with a struct encoding, a tagged variant whose tagged forms are
        // all objects — unless the root type is a nullable wrapper (the
        // emitter unwraps optional/pointer roots to the inner body, but a
        // default-constructed wrapper encodes to null). Null carries no
        // property values, so there is nothing to annotate from.
        const auto* doc = value.get_object();
        if(doc == nullptr) {
            return;
        }
        if(auto* props = body.find("properties")) {
            const dyn::Array* required = nullptr;
            if(const auto* r = body.find("required")) {
                required = r->get_array();
            }
            for(auto& [name, prop]: *props->get_object()) {
                if(const auto* v = doc->find(name)) {
                    annotate_property(prop, *v, is_required(required, name));
                }
            }
        }
        // A oneOf is a tagged variant: its branches are disjoint by tag, so
        // the branch accepting the document describes the encoded
        // alternative — descend so its members keep their decode defaults.
        if(auto* one_of = body.find("oneOf")) {
            auto& alts = *one_of->get_array();
            auto it = std::ranges::find_if(alts, [&](const dyn::Value& alt) {
                return branch_matches(*alt.get_object(), *doc);
            });
            if(it != alts.end()) {
                annotate(*it->get_object(), value);
            }
        }
        // An internal-tagged non-struct branch spreads its schema and tag
        // constraint over allOf parts; walk each against the same document.
        if(auto* all_of = body.find("allOf")) {
            for(auto& part: *all_of->get_array()) {
                annotate(*part.get_object(), value);
            }
        }
    }

private:
    static bool is_required(const dyn::Array* required, std::string_view name) {
        if(required == nullptr) {
            return false;
        }
        return std::ranges::any_of(*required,
                                   [&](const dyn::Value& v) { return v.get_string() == name; });
    }

    /// Whether a oneOf branch accepts the document: every required property
    /// present and every property `const` matched. These are exactly the
    /// constraints the emitter places on a tagged branch — external tagging
    /// requires the alternative's name, internal and adjacent tagging pin
    /// the tag property to a const — so the encoded alternative's branch,
    /// and only it, matches.
    static bool branch_matches(const dyn::Object& branch, const dyn::Object& doc) {
        if(const auto* r = branch.find("required")) {
            auto present = [&](const dyn::Value& name) {
                return doc.contains(*name.get_string());
            };
            if(!std::ranges::all_of(*r->get_array(), present)) {
                return false;
            }
        }
        if(const auto* props = branch.find("properties")) {
            for(const auto& [name, schema]: *props->get_object()) {
                if(const auto* c = schema.get_object()->find("const")) {
                    const auto* v = doc.find(name);
                    if(v == nullptr || *v != *c) {
                        return false;
                    }
                }
            }
        }
        if(const auto* all_of = branch.find("allOf")) {
            return std::ranges::all_of(*all_of->get_array(), [&](const dyn::Value& part) {
                return branch_matches(*part.get_object(), doc);
            });
        }
        return true;
    }

    /// A $ref recurses whether or not its property is required: the section
    /// itself may always appear while its leaves carry decode defaults — and
    /// so does any other schema shape, which annotate walks (a tagged
    /// variant through its encoded branch) or ignores (a leaf, an anyOf
    /// wrapper). A non-required property additionally takes the whole
    /// encoded value as its own default — the shared $def keeps first-visit
    /// leaf values, so the ref site is the only place a per-site member
    /// initializer survives.
    void annotate_property(dyn::Value& prop, const dyn::Value& value, bool required) {
        auto& obj = *prop.get_object();
        if(const auto* ref = obj.find("$ref")) {
            if(auto& target = resolve(*ref->get_string()); visited.insert(&target).second) {
                annotate(target, value);
            }
        } else {
            annotate(obj, value);
        }
        if(!required) {
            obj.insert("default", value);
        }
    }

    /// Only $defs references reach the walk at property level: a by-value
    /// self-reference ($ref "#") would make the type infinite, and
    /// pointer/optional self-references sit inside an anyOf wrapper, which
    /// the walk treats as a leaf. The emitter inserted "$defs" and the named
    /// body before it emitted the reference.
    dyn::Object& resolve(std::string_view ref) {
        constexpr std::string_view prefix = "#/$defs/";
        return *defs->find(ref.substr(prefix.size()))->get_object();
    }

    dyn::Object* defs = nullptr;
    std::unordered_set<const dyn::Object*> visited;
};

/// Metadata resolution config for schema generation: the user's config
/// (field_rename, deny_unknown_fields, ...) merged over defaults — the same
/// merge the codec dispatch applies — tagged with the JSON format so
/// format-scoped meta::repr specializations resolve the way to_string does.
template <typename Config>
struct schema_config : default_config<Config> {
    using format = json::format;
};

/// schema_options as a codec config declares them. No visitor participates
/// here, so is_human_readable sees only the config override — matching
/// to_string, whose ValueWriter is human-readable.
template <typename Config>
schema_options options_of() {
    using merged = default_config<Config>;
    return {
        .enums = merged::enum_repr,
        .rename = apply_enum_rename<merged>,
        .nan = merged::nan_repr,
        .human_readable = is_human_readable<merged, void>(),
    };
}

}  // namespace detail

inline std::expected<dyn::Value, error> schema(const meta::type_info& root,
                                               const schema_options& options = {}) {
    return detail::SchemaEmitter{options}.emit(root);
}

namespace detail {

inline std::expected<std::string, error> stringify(dyn::Value value, bool pretty) {
    KOTA_EXPECTED_TRY_V(auto compact, to_string(std::move(value)));
    if(!pretty) {
        return compact;
    }
    return prettify(compact);
}

}  // namespace detail

/// When T is default-initializable, the schema also carries `default`
/// annotations: a default-constructed instance is encoded through the real
/// JSON encoder under Config and parsed back into a document, so the values
/// match what to_string emits byte for byte (enum renames, nan handling,
/// format-scoped reprs included). Two consequences of riding the real
/// encoder: T{} must encode under Config — a default instance the encoder
/// rejects (a NaN member under nan_repr::Error, an enum value without a
/// reflected name under enum_repr::String) fails schema generation with that
/// error — and a T whose fields the codec cannot serialize fails to compile,
/// exactly like to_string itself. Only an opaque root — one whose
/// JSON-resolved representation still reflects as kind unknown, so a
/// repr-backed root joins the pass through its representation — skips it at
/// compile time and keeps reporting the emission error at runtime.
template <typename T, typename Config = void>
std::expected<dyn::Value, error> schema() {
    KOTA_EXPECTED_TRY_V(auto result,
                        schema(meta::type_info_of<T, detail::schema_config<Config>>(),
                               detail::options_of<Config>()));
    if constexpr(std::default_initializable<T> &&
                 meta::kind_of<meta::resolved_repr_t<T, format>>() != meta::type_kind::unknown) {
        KOTA_EXPECTED_TRY_V(auto text, to_string<Config>(T{}));
        KOTA_EXPECTED_TRY_V(auto doc, from_string<dyn::Value>(text));
        auto& root = *result.get_object();
        detail::DefaultAnnotator{root}.annotate(root, doc);
    }
    return result;
}

inline std::expected<std::string, error> schema_string(const meta::type_info& root,
                                                       bool pretty = false,
                                                       const schema_options& options = {}) {
    KOTA_EXPECTED_TRY_V(auto value, schema(root, options));
    return detail::stringify(std::move(value), pretty);
}

template <typename T, typename Config = void>
std::expected<std::string, error> schema_string(bool pretty = false) {
    KOTA_EXPECTED_TRY_V(auto value, (schema<T, Config>()));
    return detail::stringify(std::move(value), pretty);
}

}  // namespace kota::codec::json
