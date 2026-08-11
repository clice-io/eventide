#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "kota/support/expected_try.h"
#include "kota/support/naming.h"
#include "kota/support/type_list.h"
#include "kota/meta/schema.h"
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

/// An internal-tagged struct branch is the one schema site whose struct body
/// is inlined rather than $def'd (the tag property must merge into the
/// struct's own properties). When the typed entry point will run the
/// default-annotation pass, the emitter stamps each such branch with the
/// alternative's normalized type name under this key so the pass can pair
/// the branch with the fresh document of its type; the pass strips every
/// marker it meets, so no returned schema ever carries one.
constexpr inline std::string_view alternative_marker = "x-kota-alternative";

class SchemaEmitter {
    using tk = meta::type_kind;
    using result_t = std::expected<dyn::Value, error>;

public:
    explicit SchemaEmitter(const schema_options& opts, bool mark_alternatives = false) :
        opts(opts), mark_alternatives(mark_alternatives) {}

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
            if(mark_alternatives) {
                obj.insert(std::string(alternative_marker),
                           kota::naming::normalize_identifier(ti->type_name));
            }
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
    bool mark_alternatives;
    std::vector<std::pair<std::string, dyn::Value>> defs;
    std::unordered_map<const meta::type_info*, std::string> def_names;
    std::unordered_set<std::string> used_names;
    std::unordered_set<const meta::type_info*> emitted;
    const meta::type_info* root_ti = nullptr;
};

/// The fresh documents the default-annotation pass pairs schema bodies with:
/// for every struct reachable through the resolved type structure whose
/// decode-fresh value (decode_fresh_t — the declared representation of a
/// declarative repr chain, the value itself where an imperative link reads
/// it in place) is default-initializable, the document a value-initialized
/// instance of that value encodes to under the resolved config, keyed by the
/// normalized name the emitter gives the type's $def. collect_fresh mirrors
/// the emitter's reach — struct fields (flattened included, skipped
/// excluded), each under the config its slot's rename_all / deny_unknown
/// spec merges to, optional and pointer inners, sequence and set elements,
/// map values (keys encode as object keys and carry no schema), tuple
/// elements, variant alternatives. Field-level behavior attrs (`with`, `as`)
/// are not followed, and a fresh instance the encoder rejects (a NaN member
/// under nan_repr::Error, an unnamed enum value under enum_repr::String)
/// records no document: both leave their subtrees unannotated rather than
/// annotated wrongly. `conflicted` lists, per type, the properties whose
/// fresh value some in-place decode site disagrees with — the annotation
/// pass omits those defaults instead of publishing a value that is wrong at
/// one of the sites.
struct FreshDefaults {
    std::unordered_map<std::string, dyn::Value> docs;
    std::unordered_map<std::string, std::unordered_set<std::string>> conflicted;
    std::unordered_set<std::string> claimed;
    std::unordered_set<const meta::type_info*> seen;
};

/// Annotates the emitted schema with `default` values — the value decode
/// leaves behind when a property is absent. Two sources feed the pass,
/// matching the two ways decode arrives at a value it must default:
///
/// - Site defaults. Decode assigns struct fields in place on the enclosing
///   instance, so an absent optional property keeps whatever the enclosing
///   default instance carries — member initializers on the enclosing type
///   included. Each schema body is paired with a document of its own type
///   (the root body with the encoded fresh instance of the root's resolved
///   representation — what decode constructs and reads into) and every
///   non-required property copies its encoded value onto its schema as
///   `default`; a non-required struct $ref thus carries its whole encoded
///   object, so per-site member initializers survive the $def sharing.
///   Required properties must always appear, so a default would be a lie;
///   they are skipped, as are properties the document does not carry
///   (encode-side skip conditions, the null a nullable root encodes to).
///
/// - Fresh defaults. Decode value-initializes sequence and set elements, map
///   values, and selected variant alternatives before reading their fields,
///   so inside a shared $def the enclosing instance's contents are
///   meaningless — a member initializer that overrides an element's fields
///   must not leak into the element type's own defaults. Every $def body is
///   therefore paired with the document a freshly constructed instance of
///   its own type encodes to (FreshDefaults), and so is every
///   internal-tagged variant branch — the one schema site whose struct body
///   is inlined rather than $def'd — via the emitter's alternative marker,
///   which the sweep consumes and strips. A type that cannot be freshly
///   constructed and encoded leaves its body unannotated rather than
///   guessing.
///
/// A body shared between sites can still be contradicted by one of them:
/// struct fields, engaged optionals and pointers, and tuple elements decode
/// in place, so an enclosing initializer that overrides a nested field wins
/// over the nested type's own initializer at that site. Wherever
/// collect_fresh found such a disagreement (FreshDefaults::conflicted), the
/// disputed property's default is omitted — the shared body cannot tell the
/// truth for every site, so it says nothing.
class DefaultAnnotator {
public:
    explicit DefaultAnnotator(FreshDefaults fresh) : fresh(std::move(fresh)) {}

    void run(dyn::Object& root, const dyn::Value& root_doc, std::string_view root_name) {
        annotate_properties(root, root_doc, root_name);
        if(auto* defs = root.find("$defs")) {
            for(auto& [name, body]: *defs->get_object()) {
                if(auto it = fresh.docs.find(name); it != fresh.docs.end()) {
                    annotate_properties(*body.get_object(), it->second, name);
                }
            }
        }
        sweep(root);
    }

private:
    static bool is_required(const dyn::Array* required, std::string_view name) {
        if(required == nullptr) {
            return false;
        }
        return std::ranges::any_of(*required,
                                   [&](const dyn::Value& v) { return v.get_string() == name; });
    }

    void annotate_properties(dyn::Object& body,
                             const dyn::Value& doc_value,
                             std::string_view type_name) {
        // A non-struct root pairs with a non-object document (an array, a
        // scalar, the null a default-constructed nullable root encodes to);
        // it has no properties to annotate from.
        const auto* doc = doc_value.get_object();
        if(doc == nullptr) {
            return;
        }
        auto* props = body.find("properties");
        if(props == nullptr) {
            return;
        }
        const dyn::Array* required = nullptr;
        if(const auto* r = body.find("required")) {
            required = r->get_array();
        }
        const std::unordered_set<std::string>* disputed = nullptr;
        if(auto it = fresh.conflicted.find(std::string(type_name)); it != fresh.conflicted.end()) {
            disputed = &it->second;
        }
        for(auto& [name, prop]: *props->get_object()) {
            const auto* v = doc->find(name);
            if(v != nullptr && !is_required(required, name) &&
               (disputed == nullptr || !disputed->contains(name))) {
                prop.get_object()->insert("default", *v);
            }
        }
    }

    /// Walks every subschema position looking for marked internal-tagged
    /// branches; a marked branch takes its site defaults from the fresh
    /// document of its alternative type, and drops the marker either way.
    /// Only keys whose values are schemas are entered — never `default`,
    /// `enum`, or `const`, whose contents are documents, not schemas.
    void sweep(dyn::Object& schema) {
        if(const auto* marker = schema.find(alternative_marker)) {
            auto name = std::string(*marker->get_string());
            if(auto it = fresh.docs.find(name); it != fresh.docs.end()) {
                annotate_properties(schema, it->second, name);
            }
            schema.remove(alternative_marker);
        }
        for(std::string_view key: {"items", "additionalProperties"}) {
            if(auto* sub = schema.find(key)) {
                sweep_value(*sub);
            }
        }
        for(std::string_view key: {"prefixItems", "oneOf", "anyOf", "allOf"}) {
            if(auto* subs = schema.find(key)) {
                for(auto& sub: *subs->get_array()) {
                    sweep_value(sub);
                }
            }
        }
        for(std::string_view key: {"properties", "$defs"}) {
            if(auto* subs = schema.find(key)) {
                for(auto& [_, sub]: *subs->get_object()) {
                    sweep_value(sub);
                }
            }
        }
    }

    /// A tuple schema closes its prefix with "items": false — not a schema
    /// object, nothing to enter.
    void sweep_value(dyn::Value& sub) {
        if(auto* obj = sub.get_object()) {
            sweep(*obj);
        }
    }

    FreshDefaults fresh;
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

/// The config a slot's subtree resolves and encodes under: a rename_all /
/// deny_unknown spec on a reflectable struct field merges into the carried
/// config — the same gate the codec dispatch and meta's repr resolver apply
/// — and stays inert on every other slot kind.
template <typename Config, typename Slot>
using slot_config_t = std::conditional_t<meta::reflectable_class<typename Slot::raw_type>,
                                         meta::merged_config_t<Config, typename Slot::attrs>,
                                         Config>;

template <typename T>
constexpr auto resolve_decode_fresh();

/// One repr link of the chain, split by decode form the way repr_decode
/// splits it: a declarative link (from()) constructs its declared
/// representation, so the chain follows it; an imperative link hands the
/// caller's value to deserialize in place, so the chain stops at T.
template <typename T, typename Repr>
constexpr auto decode_fresh_link() {
    if constexpr(requires { Repr::from(std::declval<meta::declared_repr_t<Repr>>()); }) {
        return resolve_decode_fresh<meta::declared_repr_t<Repr>>();
    } else {
        return std::type_identity<T>{};
    }
}

/// The type whose fresh instance decode constructs and reads for T. A fresh
/// resolved_repr_t is only what decode visits when every link of the chain
/// is declarative, so this walk mirrors the decode dispatch instead — the
/// same behavior-attr precedence, `as` recursing into the target it
/// constructs and converts — and keeps the annotated wrapper at every stop,
/// so encoding the fresh instance re-applies the wrapper's attrs exactly
/// like the codec.
template <typename T>
constexpr auto resolve_decode_fresh() {
    if constexpr(meta::annotated_type<T>) {
        using attrs_t = typename T::attrs;
        if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::with>) {
            using adapter = typename tuple_find_spec_t<attrs_t, meta::behavior::with>::adapter;
            return decode_fresh_link<T, adapter>();
        } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::as>) {
            using target = typename tuple_find_spec_t<attrs_t, meta::behavior::as>::target;
            return resolve_decode_fresh<target>();
        } else if constexpr(tuple_has_spec_v<attrs_t, meta::behavior::enum_string>) {
            return std::type_identity<T>{};
        } else if constexpr(meta::has_repr<typename T::annotated_type, format>) {
            return decode_fresh_link<T, meta::repr_for<typename T::annotated_type, format>>();
        } else {
            return std::type_identity<T>{};
        }
    } else if constexpr(meta::has_repr<T, format>) {
        return decode_fresh_link<T, meta::repr_for<T, format>>();
    } else {
        return std::type_identity<T>{};
    }
}

template <typename T>
using decode_fresh_t = typename decltype(resolve_decode_fresh<T>())::type;

template <typename T, typename Config>
void collect_fresh(FreshDefaults& out);

template <typename Config, typename... Slots>
void collect_fresh_slots(FreshDefaults& out, kota::type_list<Slots...>) {
    (collect_fresh<typename Slots::raw_type, slot_config_t<Config, Slots>>(out), ...);
}

template <typename Config, typename... Ts>
void collect_fresh_alternatives(FreshDefaults& out, std::type_identity<std::variant<Ts...>>) {
    (collect_fresh<Ts, Config>(out), ...);
}

template <typename T, typename Config>
void compare_site(FreshDefaults& out, const dyn::Value& site);

/// Replays decode's in-place sites of one struct document: each field's
/// sub-document is the value decode leaves in the enclosing instance when
/// the nested properties are absent, so it is held against the field type's
/// own fresh document.
template <typename Resolved, typename Config>
void compare_slot_sites(FreshDefaults& out, const dyn::Object& doc) {
    using schema = meta::virtual_schema<Resolved, Config>;
    using slots = typename schema::slots;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (
            [&] {
                using slot_t = type_list_element_t<Is, slots>;
                if(const auto* site = doc.find(schema::fields[Is].name)) {
                    compare_site<typename slot_t::raw_type, slot_config_t<Config, slot_t>>(out,
                                                                                           *site);
                }
            }(),
            ...);
    }(std::make_index_sequence<type_list_size_v<slots>>{});
}

/// Decode reuses in-place sites — struct fields, engaged optionals and
/// pointers, tuple elements — so the value an absent property leaves behind
/// there is the enclosing instance's, not a fresh one. Walks one site
/// document against the fresh documents, recording every property whose
/// site value disagrees; the walk ends at fresh-constructing positions
/// (sequence and set elements, map values, variant alternatives), where
/// decode value-initializes and nothing site-specific survives.
template <typename T, typename Config>
void compare_site(FreshDefaults& out, const dyn::Value& site) {
    using tk = meta::type_kind;
    using resolved = meta::resolved_repr_t<T, format>;
    constexpr tk kind = meta::kind_of<resolved>();
    if constexpr(kind == tk::structure) {
        // A null site is a disengaged optional or pointer: decode emplaces a
        // fresh value there, which agrees with the fresh document by
        // definition.
        const auto* site_obj = site.get_object();
        if(site_obj == nullptr) {
            return;
        }
        auto name = kota::naming::normalize_identifier(meta::type_info_of<T, Config>().type_name);
        if(auto it = out.docs.find(name); it != out.docs.end()) {
            for(const auto& [key, fresh_value]: *it->second.get_object()) {
                const auto* site_value = site_obj->find(key);
                if(site_value == nullptr || *site_value != fresh_value) {
                    out.conflicted[name].insert(key);
                }
            }
        }
        compare_slot_sites<resolved, meta::resolved_config_t<T, Config>>(out, *site_obj);
    } else if constexpr(kind == tk::optional) {
        compare_site<typename resolved::value_type, Config>(out, site);
    } else if constexpr(kind == tk::pointer) {
        compare_site<typename resolved::element_type, Config>(out, site);
    } else if constexpr(kind == tk::tuple) {
        // A disengaged optional-of-tuple site encodes null: nothing decodes
        // in place there.
        if(const auto* arr = site.get_array()) {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (compare_site<std::tuple_element_t<Is, resolved>, Config>(out, (*arr)[Is]), ...);
            }(std::make_index_sequence<std::tuple_size_v<resolved>>{});
        }
    }
}

template <typename T, typename Config>
void collect_fresh(FreshDefaults& out) {
    using tk = meta::type_kind;
    using resolved = meta::resolved_repr_t<T, format>;
    constexpr tk kind = meta::kind_of<resolved>();
    if constexpr(kind == tk::structure) {
        const meta::type_info& ti = meta::type_info_of<T, Config>();
        if(!out.seen.insert(&ti).second) {
            return;
        }
        // The resolved config carries T's own structural annotations (an
        // annotated root or container element — slots merge theirs before
        // recursing), so the document keys and field names below match the
        // schema the emitter derives from the same resolution.
        using cfg = meta::resolved_config_t<T, Config>;
        using fresh_t = decode_fresh_t<T>;
        auto name = kota::naming::normalize_identifier(ti.type_name);
        if(!out.claimed.insert(name).second) {
            // Two distinct types under one normalized name: the emitter
            // rejects the collision for $def'd types, but an inlined
            // internal-tagged alternative shares the namespace silently —
            // annotate neither rather than pair one with the other's
            // document.
            out.docs.erase(name);
        } else if constexpr(std::default_initializable<fresh_t>) {
            // Encode the value decode constructs and reads into — the
            // declared representation of a declarative repr chain
            // (repr_decode value-initializes it before visiting), T itself
            // when an imperative link reads it in place: Repr::to(T{}) may
            // legally disagree with a fresh representation, and decode's
            // absent-field values come from whichever instance decode
            // actually visits.
            if(auto text = to_string<cfg>(fresh_t{})) {
                if(auto doc = from_string<dyn::Value>(*text)) {
                    out.docs.emplace(name, std::move(*doc));
                }
            }
        }
        collect_fresh_slots<cfg>(out, typename meta::virtual_schema<resolved, cfg>::slots{});
        // With the whole subtree collected, replay this struct's own
        // in-place sites against it (the collision branch above may have
        // dropped the document — nothing to compare then, and the emitter
        // rejects the $def collision anyway).
        if(auto it = out.docs.find(name); it != out.docs.end()) {
            compare_slot_sites<resolved, cfg>(out, *it->second.get_object());
        }
    } else if constexpr(kind == tk::optional) {
        collect_fresh<typename resolved::value_type, Config>(out);
    } else if constexpr(kind == tk::pointer) {
        collect_fresh<typename resolved::element_type, Config>(out);
    } else if constexpr(kind == tk::array || kind == tk::set) {
        collect_fresh<std::ranges::range_value_t<resolved>, Config>(out);
    } else if constexpr(kind == tk::map) {
        collect_fresh<typename std::ranges::range_value_t<resolved>::second_type, Config>(out);
    } else if constexpr(kind == tk::tuple) {
        [&out]<std::size_t... Is>(std::index_sequence<Is...>) {
            (collect_fresh<std::tuple_element_t<Is, resolved>, Config>(out), ...);
        }(std::make_index_sequence<std::tuple_size_v<resolved>>{});
    } else if constexpr(kind == tk::variant) {
        collect_fresh_alternatives<Config>(out, std::type_identity<resolved>{});
    }
    // Scalars, enums, bytes, any: leaves without annotatable structure.
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

/// When the value decode reads for T is default-initializable, the schema
/// also carries `default` annotations: fresh instances of the values decode
/// actually constructs and reads into are encoded through the real JSON
/// encoder under Config and parsed back into documents, so the values match
/// what to_string emits byte for byte (enum renames, nan handling,
/// format-scoped reprs, structural annotations on the root itself included)
/// — and, for a repr-backed type, describe the decode side per repr form:
/// a declarative chain ends at the representation repr_decode
/// value-initializes before visiting, not at what Repr::to(T{}) happens to
/// encode, while an imperative link hands the caller's value to deserialize
/// in place, so the fresh instance is the value's own type. Root properties
/// take the values of the root's fresh instance; each $def and inlined
/// variant branch takes the values of a freshly constructed instance of its
/// own type — what decode's fresh constructions (sequence elements, map
/// values, emplaced alternatives) actually produce — while a nested type
/// that cannot be freshly constructed or encoded stays unannotated, and a
/// default an in-place site contradicts is omitted (see DefaultAnnotator).
/// Two consequences of riding the real encoder: the root's fresh instance
/// must encode under Config — an instance the encoder rejects (a NaN member
/// under nan_repr::Error, an enum value without a reflected name under
/// enum_repr::String) fails schema generation with that error — and a T
/// whose fields the codec cannot serialize fails to compile, exactly like
/// to_string itself. Only an opaque root — one whose JSON-resolved
/// representation still reflects as kind unknown — skips the pass at
/// compile time and keeps reporting the emission error at runtime.
template <typename T, typename Config = void>
std::expected<dyn::Value, error> schema() {
    using resolved = meta::resolved_repr_t<T, format>;
    using fresh_t = detail::decode_fresh_t<T>;
    constexpr bool annotate_defaults = std::default_initializable<fresh_t> &&
                                       meta::kind_of<resolved>() != meta::type_kind::unknown;
    KOTA_EXPECTED_TRY_V(
        auto result,
        (detail::SchemaEmitter{detail::options_of<Config>(), annotate_defaults}.emit(
            meta::type_info_of<T, detail::schema_config<Config>>())));
    if constexpr(annotate_defaults) {
        using cfg = meta::resolved_config_t<T, detail::schema_config<Config>>;
        KOTA_EXPECTED_TRY_V(auto text, to_string<cfg>(fresh_t{}));
        KOTA_EXPECTED_TRY_V(auto doc, from_string<dyn::Value>(text));
        detail::FreshDefaults fresh;
        detail::collect_fresh<T, detail::schema_config<Config>>(fresh);
        detail::DefaultAnnotator{std::move(fresh)}.run(
            *result.get_object(),
            doc,
            kota::naming::normalize_identifier(
                meta::type_info_of<T, detail::schema_config<Config>>().type_name));
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
