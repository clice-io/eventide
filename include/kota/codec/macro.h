#pragma once

// This header intentionally includes nothing: it is a pure macro sheet that
// can be included from anywhere at zero transitive cost. The entities the
// macros reference (::kota::meta::make_spec, ::kota::meta::dsl,
// ::kota::meta::annotate) only need to be visible at the point of use.

#define KOTATSU_CONCAT_IMPL(a, b) a##b
#define KOTATSU_CONCAT(a, b) KOTATSU_CONCAT_IMPL(a, b)

/// Annotates the next field declaration with serde attributes:
///
///     KOTATSU_ANNOTATE(rename = "compileCommands",
///                      alias = {"compile_commands"},
///                      description = "Path to the compilation database.")
///     <std::string> compile_commands;
///
/// The attribute strings live in one constexpr spec object per annotation;
/// only the short generated tag type appears in template arguments, so
/// mangled names stay small no matter how long the strings are. Type-valued
/// attributes ride along as `as = type<Target>`, `with = type<Adapter>`,
/// `skip_if = type<Pred>`, `enum_string = type<Policy>`, or as explicit
/// trailing template arguments after the field type.
#define KOTATSU_ANNOTATE_AT(tag, ...)                                                              \
    struct tag {                                                                                   \
        constexpr static auto spec = [] {                                                          \
            using namespace ::kota::meta::dsl;                                                     \
            return ::kota::meta::make_spec(__VA_ARGS__);                                           \
        }();                                                                                       \
    };                                                                                             \
    ::kota::meta::annotate<tag>::type

#define KOTATSU_ANNOTATE(...)                                                                      \
    KOTATSU_ANNOTATE_AT(KOTATSU_CONCAT(kotatsu_annotation_, __LINE__), __VA_ARGS__)
