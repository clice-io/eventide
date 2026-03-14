#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "eventide/zest/zest.h"
#include "eventide/serde/serde/schema/kind.h"
#include "eventide/serde/serde/schema/descriptor.h"
#include "eventide/serde/serde/schema/match.h"

namespace eventide::serde {

namespace {

namespace sk = schema;

// ─── Test types: plain aggregates without annotation wrappers ───────────────
// Annotation-wrapped members (flatten<>, skip<>, rename<>) cause the
// reflection field_count to mis-count via brace elision.  Behavior with
// annotations is covered by the backend-specific serde roundtrip tests.

struct simple_struct {
    int x = 0;
    std::string y;
    bool z = false;
};

struct with_optional {
    int id = 0;
    std::optional<std::string> note;
};

struct inner_flat {
    int a = 0;
    double b = 0.0;
};

struct empty_struct {};

struct single_field {
    int only = 0;
};

enum class color : std::uint8_t { red, green, blue };

struct with_vector {
    std::vector<int> items;
};

struct with_map {
    std::map<std::string, int> kv;
};

struct object_a {
    int x = 0;
    std::string y;
};

struct object_b {
    double p = 0.0;
    std::string q;
    int r = 0;
};

struct object_c {
    std::string y;
    bool w = false;
};

struct multi_type {
    int id = 0;
    std::string name;
    double score = 0.0;
    bool active = false;
};

struct camel_fields {
    int user_id = 0;
    std::string first_name;
};

struct camel_cfg {
    using field_rename = rename_policy::lower_camel;
};

struct upper_snake_cfg {
    using field_rename = rename_policy::upper_snake;
};

struct upper_camel_cfg {
    using field_rename = rename_policy::upper_camel;
};

struct lower_snake_cfg {
    using field_rename = rename_policy::lower_snake;
};

// ═══════════════════════════════════════════════════════════════════════════
//  kind.h
// ═══════════════════════════════════════════════════════════════════════════

static_assert(sk::kind_of<std::monostate>() == sk::type_kind::null_like);
static_assert(sk::kind_of<std::nullptr_t>() == sk::type_kind::null_like);
static_assert(sk::kind_of<bool>() == sk::type_kind::boolean);
static_assert(sk::kind_of<int>() == sk::type_kind::integer);
static_assert(sk::kind_of<std::int64_t>() == sk::type_kind::integer);
static_assert(sk::kind_of<unsigned>() == sk::type_kind::integer);
static_assert(sk::kind_of<std::uint8_t>() == sk::type_kind::integer);
static_assert(sk::kind_of<float>() == sk::type_kind::floating);
static_assert(sk::kind_of<double>() == sk::type_kind::floating);
static_assert(sk::kind_of<char>() == sk::type_kind::string);
static_assert(sk::kind_of<std::string>() == sk::type_kind::string);
static_assert(sk::kind_of<std::string_view>() == sk::type_kind::string);
static_assert(sk::kind_of<std::vector<int>>() == sk::type_kind::array);
static_assert(sk::kind_of<std::array<int, 3>>() == sk::type_kind::array);
static_assert(sk::kind_of<std::tuple<int, double>>() == sk::type_kind::array);
static_assert(sk::kind_of<std::pair<int, int>>() == sk::type_kind::array);
static_assert(sk::kind_of<std::map<std::string, int>>() == sk::type_kind::object);
static_assert(sk::kind_of<simple_struct>() == sk::type_kind::object);
static_assert(sk::kind_of<std::variant<int, std::string>>() == sk::type_kind::variant);
static_assert(sk::kind_of<color>() == sk::type_kind::integer);
static_assert(sk::kind_of<std::optional<int>>() == sk::type_kind::integer);
static_assert(sk::kind_of<std::optional<std::string>>() == sk::type_kind::string);

static_assert(sk::is_optional_type<std::optional<int>>());
static_assert(!sk::is_optional_type<int>());
static_assert(!sk::is_optional_type<std::string>());

static_assert(sk::hint_to_kind(0x40) == sk::type_kind::object);
static_assert(sk::hint_to_kind(0x20) == sk::type_kind::array);
static_assert(sk::hint_to_kind(0x10) == sk::type_kind::string);
static_assert(sk::hint_to_kind(0x08) == sk::type_kind::floating);
static_assert(sk::hint_to_kind(0x04) == sk::type_kind::integer);
static_assert(sk::hint_to_kind(0x02) == sk::type_kind::boolean);
static_assert(sk::hint_to_kind(0x01) == sk::type_kind::null_like);
static_assert(sk::hint_to_kind(0x00) == sk::type_kind::any);
static_assert(sk::hint_to_kind(0x40 | 0x10) == sk::type_kind::object);
static_assert(sk::hint_to_kind(0x04 | 0x08) == sk::type_kind::floating);

// ═══════════════════════════════════════════════════════════════════════════
//  descriptor.h
// ═══════════════════════════════════════════════════════════════════════════

static_assert(sk::detail::effective_field_count<simple_struct>() == 3);
static_assert(sk::detail::effective_field_count<with_optional>() == 2);
static_assert(sk::detail::effective_field_count<empty_struct>() == 0);
static_assert(sk::detail::effective_field_count<single_field>() == 1);
static_assert(sk::detail::effective_field_count<inner_flat>() == 2);
static_assert(sk::detail::effective_field_count<multi_type>() == 4);
static_assert(sk::detail::effective_field_count<std::monostate>() == 0);

// ═══════════════════════════════════════════════════════════════════════════
//  match.h: compile-time invariants
// ═══════════════════════════════════════════════════════════════════════════

static_assert(sk::detail::kind_compatible(sk::type_kind::integer, sk::type_kind::integer));
static_assert(sk::detail::kind_compatible(sk::type_kind::integer, sk::type_kind::floating));
static_assert(sk::detail::kind_compatible(sk::type_kind::floating, sk::type_kind::integer));
static_assert(sk::detail::kind_compatible(sk::type_kind::any, sk::type_kind::string));
static_assert(sk::detail::kind_compatible(sk::type_kind::boolean, sk::type_kind::any));
static_assert(!sk::detail::kind_compatible(sk::type_kind::string, sk::type_kind::integer));
static_assert(!sk::detail::kind_compatible(sk::type_kind::boolean, sk::type_kind::string));
static_assert(!sk::detail::kind_compatible(sk::type_kind::object, sk::type_kind::array));
static_assert(!sk::detail::kind_compatible(sk::type_kind::null_like, sk::type_kind::boolean));

static_assert(sk::detail::count_match_entries<simple_struct>() == 3);
static_assert(sk::detail::count_match_entries<empty_struct>() == 0);
static_assert(sk::detail::count_match_entries<single_field>() == 1);
static_assert(sk::detail::count_match_entries<multi_type>() == 4);

static_assert(sk::detail::count_required_fields<simple_struct>() == 3);
static_assert(sk::detail::count_required_fields<with_optional>() == 1);
static_assert(sk::detail::count_required_fields<empty_struct>() == 0);
static_assert(sk::detail::count_required_fields<multi_type>() == 4);

TEST_SUITE(serde_schema) {

// ─── descriptor.h: describe() ───────────────────────────────────────────────

TEST_CASE(describe_simple_struct) {
    constexpr auto desc = sk::describe<simple_struct>();
    static_assert(desc.field_count() == 3);
    static_assert(desc.fields[0].name == "x");
    static_assert(desc.fields[0].kind == sk::type_kind::integer);
    static_assert(desc.fields[0].required == true);
    static_assert(desc.fields[1].name == "y");
    static_assert(desc.fields[1].kind == sk::type_kind::string);
    static_assert(desc.fields[1].required == true);
    static_assert(desc.fields[2].name == "z");
    static_assert(desc.fields[2].kind == sk::type_kind::boolean);
    static_assert(desc.fields[2].required == true);
    EXPECT_TRUE(true);
}

TEST_CASE(describe_with_optional) {
    constexpr auto desc = sk::describe<with_optional>();
    static_assert(desc.field_count() == 2);
    static_assert(desc.fields[0].name == "id");
    static_assert(desc.fields[0].kind == sk::type_kind::integer);
    static_assert(desc.fields[0].required == true);
    static_assert(desc.fields[1].name == "note");
    static_assert(desc.fields[1].kind == sk::type_kind::string);
    static_assert(desc.fields[1].required == false);
    EXPECT_TRUE(true);
}

TEST_CASE(describe_inner_flat) {
    constexpr auto desc = sk::describe<inner_flat>();
    static_assert(desc.field_count() == 2);
    static_assert(desc.fields[0].name == "a");
    static_assert(desc.fields[0].kind == sk::type_kind::integer);
    static_assert(desc.fields[1].name == "b");
    static_assert(desc.fields[1].kind == sk::type_kind::floating);
    EXPECT_TRUE(true);
}

TEST_CASE(describe_multi_type) {
    constexpr auto desc = sk::describe<multi_type>();
    static_assert(desc.field_count() == 4);
    static_assert(desc.fields[0].name == "id");
    static_assert(desc.fields[0].kind == sk::type_kind::integer);
    static_assert(desc.fields[1].name == "name");
    static_assert(desc.fields[1].kind == sk::type_kind::string);
    static_assert(desc.fields[2].name == "score");
    static_assert(desc.fields[2].kind == sk::type_kind::floating);
    static_assert(desc.fields[3].name == "active");
    static_assert(desc.fields[3].kind == sk::type_kind::boolean);
    EXPECT_TRUE(true);
}

TEST_CASE(schema_of_convenience) {
    constexpr auto& desc = sk::schema_of<simple_struct>::value;
    static_assert(desc.field_count() == 3);
    static_assert(desc.fields[0].name == "x");
    EXPECT_TRUE(true);
}

// ─── match.h: name_buf ──────────────────────────────────────────────────────

TEST_CASE(name_buf_from_and_view) {
    constexpr auto buf = sk::detail::name_buf::from("hello");
    static_assert(buf.view() == "hello");
    static_assert(buf.len == 5);

    constexpr auto empty_buf = sk::detail::name_buf::from("");
    static_assert(empty_buf.view() == "");
    static_assert(empty_buf.len == 0);
    static_assert(empty_buf.empty());
    EXPECT_TRUE(true);
}

TEST_CASE(name_buf_push_back) {
    constexpr auto buf = [] {
        sk::detail::name_buf b;
        b.push_back('a');
        b.push_back('b');
        b.push_back('c');
        return b;
    }();
    static_assert(buf.view() == "abc");
    static_assert(buf.len == 3);
    static_assert(buf.back() == 'c');
    EXPECT_TRUE(true);
}

TEST_CASE(name_buf_truncates_long_strings) {
    constexpr auto buf = sk::detail::name_buf::from(
        "this_is_a_very_long_field_name_that_exceeds_the_sixty_three_char"
        "acter_limit_by_quite_a_bit");
    static_assert(buf.len == 63);
    EXPECT_TRUE(true);
}

// ─── match.h: constexpr rename policies ─────────────────────────────────────

TEST_CASE(normalize_to_lower_snake_cx) {
    static_assert(sk::detail::normalize_to_lower_snake_cx("camelCase").view() == "camel_case");
    static_assert(sk::detail::normalize_to_lower_snake_cx("PascalCase").view() == "pascal_case");
    static_assert(sk::detail::normalize_to_lower_snake_cx("lower_snake").view() == "lower_snake");
    static_assert(sk::detail::normalize_to_lower_snake_cx("UPPER_SNAKE").view() == "upper_snake");
    static_assert(sk::detail::normalize_to_lower_snake_cx("XMLParser").view() == "xml_parser");
    static_assert(
        sk::detail::normalize_to_lower_snake_cx("getHTTPSUrl").view() == "get_https_url");
    static_assert(sk::detail::normalize_to_lower_snake_cx("simple").view() == "simple");
    static_assert(sk::detail::normalize_to_lower_snake_cx("A").view() == "a");
    static_assert(sk::detail::normalize_to_lower_snake_cx("a").view() == "a");
    static_assert(sk::detail::normalize_to_lower_snake_cx("abc123Def").view() == "abc123_def");
    EXPECT_TRUE(true);
}

TEST_CASE(snake_to_camel_cx) {
    static_assert(sk::detail::snake_to_camel_cx("user_name", false).view() == "userName");
    static_assert(sk::detail::snake_to_camel_cx("user_name", true).view() == "UserName");
    static_assert(sk::detail::snake_to_camel_cx("request_id", false).view() == "requestId");
    static_assert(sk::detail::snake_to_camel_cx("request_id", true).view() == "RequestId");
    static_assert(sk::detail::snake_to_camel_cx("x", false).view() == "x");
    static_assert(sk::detail::snake_to_camel_cx("x", true).view() == "X");
    static_assert(sk::detail::snake_to_camel_cx("some_value", false).view() == "someValue");
    static_assert(sk::detail::snake_to_camel_cx("a_b_c", false).view() == "aBC");
    static_assert(sk::detail::snake_to_camel_cx("a_b_c", true).view() == "ABC");
    EXPECT_TRUE(true);
}

TEST_CASE(snake_to_upper_cx) {
    static_assert(sk::detail::snake_to_upper_cx("user_name").view() == "USER_NAME");
    static_assert(sk::detail::snake_to_upper_cx("requestId").view() == "REQUEST_ID");
    static_assert(sk::detail::snake_to_upper_cx("simple").view() == "SIMPLE");
    static_assert(sk::detail::snake_to_upper_cx("a_b_c").view() == "A_B_C");
    EXPECT_TRUE(true);
}

TEST_CASE(apply_rename_cx_identity) {
    using P = rename_policy::identity;
    static_assert(sk::detail::apply_rename_cx<P>("hello").view() == "hello");
    static_assert(sk::detail::apply_rename_cx<P>("user_id").view() == "user_id");
    EXPECT_TRUE(true);
}

TEST_CASE(apply_rename_cx_lower_camel) {
    using P = rename_policy::lower_camel;
    static_assert(sk::detail::apply_rename_cx<P>("user_id").view() == "userId");
    static_assert(sk::detail::apply_rename_cx<P>("first_name").view() == "firstName");
    EXPECT_TRUE(true);
}

TEST_CASE(apply_rename_cx_upper_camel) {
    using P = rename_policy::upper_camel;
    static_assert(sk::detail::apply_rename_cx<P>("user_id").view() == "UserId");
    static_assert(sk::detail::apply_rename_cx<P>("first_name").view() == "FirstName");
    EXPECT_TRUE(true);
}

TEST_CASE(apply_rename_cx_upper_snake) {
    using P = rename_policy::upper_snake;
    static_assert(sk::detail::apply_rename_cx<P>("user_id").view() == "USER_ID");
    static_assert(sk::detail::apply_rename_cx<P>("first_name").view() == "FIRST_NAME");
    EXPECT_TRUE(true);
}

TEST_CASE(apply_rename_cx_lower_snake) {
    using P = rename_policy::lower_snake;
    static_assert(sk::detail::apply_rename_cx<P>("userId").view() == "user_id");
    static_assert(sk::detail::apply_rename_cx<P>("PascalCase").view() == "pascal_case");
    EXPECT_TRUE(true);
}

// ─── match.h: kind_compatible ───────────────────────────────────────────────

TEST_CASE(kind_compatible_symmetric_and_special) {
    using K = sk::type_kind;
    static_assert(sk::detail::kind_compatible(K::integer, K::integer));
    static_assert(sk::detail::kind_compatible(K::string, K::string));
    static_assert(sk::detail::kind_compatible(K::boolean, K::boolean));
    static_assert(sk::detail::kind_compatible(K::floating, K::floating));
    static_assert(sk::detail::kind_compatible(K::object, K::object));
    static_assert(sk::detail::kind_compatible(K::array, K::array));
    static_assert(sk::detail::kind_compatible(K::null_like, K::null_like));

    static_assert(sk::detail::kind_compatible(K::integer, K::floating));
    static_assert(sk::detail::kind_compatible(K::floating, K::integer));

    static_assert(sk::detail::kind_compatible(K::any, K::integer));
    static_assert(sk::detail::kind_compatible(K::any, K::string));
    static_assert(sk::detail::kind_compatible(K::integer, K::any));
    static_assert(sk::detail::kind_compatible(K::any, K::any));

    static_assert(!sk::detail::kind_compatible(K::string, K::integer));
    static_assert(!sk::detail::kind_compatible(K::boolean, K::string));
    static_assert(!sk::detail::kind_compatible(K::object, K::array));
    static_assert(!sk::detail::kind_compatible(K::null_like, K::boolean));
    static_assert(!sk::detail::kind_compatible(K::string, K::object));
    EXPECT_TRUE(true);
}

// ─── match.h: build_field_entries ───────────────────────────────────────────

TEST_CASE(build_field_entries_simple) {
    using cfg = config::default_config;
    constexpr auto entries = sk::detail::build_field_entries<simple_struct, cfg>();
    static_assert(entries.size() == 3);

    static_assert(entries[0].wire_name.len == 1);
    static_assert(entries[1].wire_name.len == 1);
    static_assert(entries[2].wire_name.len == 1);
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_with_camel_config) {
    constexpr auto entries = sk::detail::build_field_entries<camel_fields, camel_cfg>();
    static_assert(entries.size() == 2);

    static_assert([] {
        auto e = sk::detail::build_field_entries<camel_fields, camel_cfg>();
        bool a = false, b = false;
        for(std::size_t i = 0; i < e.size(); i++) {
            if(e[i].wire_name.view() == "userId") a = true;
            if(e[i].wire_name.view() == "firstName") b = true;
        }
        return a && b;
    }());
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_sorted_by_length) {
    using cfg = config::default_config;
    static_assert([] {
        auto e = sk::detail::build_field_entries<multi_type, cfg>();
        for(std::size_t i = 1; i < e.size(); i++) {
            if(e[i].wire_name.len < e[i - 1].wire_name.len) return false;
        }
        return true;
    }());
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_upper_snake_config) {
    static_assert([] {
        auto e = sk::detail::build_field_entries<camel_fields, upper_snake_cfg>();
        bool a = false, b = false;
        for(std::size_t i = 0; i < e.size(); i++) {
            if(e[i].wire_name.view() == "USER_ID") a = true;
            if(e[i].wire_name.view() == "FIRST_NAME") b = true;
        }
        return a && b;
    }());
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_upper_camel_config) {
    static_assert([] {
        auto e = sk::detail::build_field_entries<camel_fields, upper_camel_cfg>();
        bool a = false, b = false;
        for(std::size_t i = 0; i < e.size(); i++) {
            if(e[i].wire_name.view() == "UserId") a = true;
            if(e[i].wire_name.view() == "FirstName") b = true;
        }
        return a && b;
    }());
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_preserves_kind) {
    using cfg = config::default_config;
    static_assert([] {
        auto e = sk::detail::build_field_entries<multi_type, cfg>();
        for(std::size_t i = 0; i < e.size(); i++) {
            if(e[i].wire_name.view() == "id" && e[i].kind != sk::type_kind::integer) return false;
            if(e[i].wire_name.view() == "name" && e[i].kind != sk::type_kind::string) return false;
            if(e[i].wire_name.view() == "score" && e[i].kind != sk::type_kind::floating)
                return false;
            if(e[i].wire_name.view() == "active" && e[i].kind != sk::type_kind::boolean)
                return false;
        }
        return true;
    }());
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_required_flag) {
    using cfg = config::default_config;
    static_assert([] {
        auto e = sk::detail::build_field_entries<with_optional, cfg>();
        for(std::size_t i = 0; i < e.size(); i++) {
            if(e[i].wire_name.view() == "id" && !e[i].required) return false;
            if(e[i].wire_name.view() == "note" && e[i].required) return false;
        }
        return true;
    }());
    EXPECT_TRUE(true);
}

TEST_CASE(build_field_entries_default_config_identity) {
    using cfg = config::default_config;
    static_assert([] {
        auto e = sk::detail::build_field_entries<camel_fields, cfg>();
        bool a = false, b = false;
        for(std::size_t i = 0; i < e.size(); i++) {
            if(e[i].wire_name.view() == "user_id") a = true;
            if(e[i].wire_name.view() == "first_name") b = true;
        }
        return a && b;
    }());
    EXPECT_TRUE(true);
}

// ─── match.h: match_key ────────────────────────────────────────────────────

TEST_CASE(match_key_simple_hit) {
    using cfg = config::default_config;
    auto r = sk::detail::match_key<simple_struct, cfg>("x");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(static_cast<int>(r->kind), static_cast<int>(sk::type_kind::integer));
    EXPECT_TRUE(r->required);
}

TEST_CASE(match_key_simple_miss) {
    using cfg = config::default_config;
    auto r = sk::detail::match_key<simple_struct, cfg>("nonexistent");
    EXPECT_FALSE(r.has_value());
}

TEST_CASE(match_key_all_fields) {
    using cfg = config::default_config;
    auto rx = sk::detail::match_key<simple_struct, cfg>("x");
    auto ry = sk::detail::match_key<simple_struct, cfg>("y");
    auto rz = sk::detail::match_key<simple_struct, cfg>("z");
    ASSERT_TRUE(rx.has_value());
    ASSERT_TRUE(ry.has_value());
    ASSERT_TRUE(rz.has_value());
    EXPECT_EQ(static_cast<int>(rx->kind), static_cast<int>(sk::type_kind::integer));
    EXPECT_EQ(static_cast<int>(ry->kind), static_cast<int>(sk::type_kind::string));
    EXPECT_EQ(static_cast<int>(rz->kind), static_cast<int>(sk::type_kind::boolean));
}

TEST_CASE(match_key_with_camel_config) {
    auto r1 = sk::detail::match_key<camel_fields, camel_cfg>("userId");
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(static_cast<int>(r1->kind), static_cast<int>(sk::type_kind::integer));

    auto r2 = sk::detail::match_key<camel_fields, camel_cfg>("firstName");
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(static_cast<int>(r2->kind), static_cast<int>(sk::type_kind::string));

    auto miss = sk::detail::match_key<camel_fields, camel_cfg>("user_id");
    EXPECT_FALSE(miss.has_value());
}

TEST_CASE(match_key_with_upper_snake_config) {
    auto r1 = sk::detail::match_key<camel_fields, upper_snake_cfg>("USER_ID");
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(static_cast<int>(r1->kind), static_cast<int>(sk::type_kind::integer));

    auto r2 = sk::detail::match_key<camel_fields, upper_snake_cfg>("FIRST_NAME");
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(static_cast<int>(r2->kind), static_cast<int>(sk::type_kind::string));
}

TEST_CASE(match_key_empty_struct) {
    using cfg = config::default_config;
    auto r = sk::detail::match_key<empty_struct, cfg>("anything");
    EXPECT_FALSE(r.has_value());
}

TEST_CASE(match_key_length_mismatch_fast_path) {
    using cfg = config::default_config;
    auto r = sk::detail::match_key<simple_struct, cfg>("xx");
    EXPECT_FALSE(r.has_value());

    auto r2 = sk::detail::match_key<simple_struct, cfg>("");
    EXPECT_FALSE(r2.has_value());
}

TEST_CASE(match_key_optional_field_not_required) {
    using cfg = config::default_config;
    auto r = sk::detail::match_key<with_optional, cfg>("note");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(static_cast<int>(r->kind), static_cast<int>(sk::type_kind::string));
    EXPECT_FALSE(r->required);
}

TEST_CASE(match_key_multi_type_all_fields) {
    using cfg = config::default_config;
    auto r_id = sk::detail::match_key<multi_type, cfg>("id");
    auto r_name = sk::detail::match_key<multi_type, cfg>("name");
    auto r_score = sk::detail::match_key<multi_type, cfg>("score");
    auto r_active = sk::detail::match_key<multi_type, cfg>("active");

    ASSERT_TRUE(r_id.has_value());
    ASSERT_TRUE(r_name.has_value());
    ASSERT_TRUE(r_score.has_value());
    ASSERT_TRUE(r_active.has_value());

    EXPECT_EQ(static_cast<int>(r_id->kind), static_cast<int>(sk::type_kind::integer));
    EXPECT_EQ(static_cast<int>(r_name->kind), static_cast<int>(sk::type_kind::string));
    EXPECT_EQ(static_cast<int>(r_score->kind), static_cast<int>(sk::type_kind::floating));
    EXPECT_EQ(static_cast<int>(r_active->kind), static_cast<int>(sk::type_kind::boolean));
}

// ─── match.h: match_variant_by_keys ─────────────────────────────────────────

TEST_CASE(variant_match_by_keys_basic) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming = {
        {"x", sk::type_kind::integer},
        {"y", sk::type_kind::string},
    };

    auto result = sk::match_variant_by_keys<cfg, object_a, object_b>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
}

TEST_CASE(variant_match_by_keys_second_alt) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming = {
        {"p", sk::type_kind::floating},
        {"q", sk::type_kind::string},
        {"r", sk::type_kind::integer},
    };

    auto result = sk::match_variant_by_keys<cfg, object_a, object_b>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1u);
}

TEST_CASE(variant_match_no_match) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming = {
        {"unknown1", sk::type_kind::string},
        {"unknown2", sk::type_kind::integer},
    };

    auto result = sk::match_variant_by_keys<cfg, object_a, object_b>(incoming);
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(variant_match_type_mismatch_rejects) {
    using cfg = config::default_config;
    // object_a expects x:integer — incoming provides x:string → type mismatch
    // object_c expects w:bool — missing → required field missing
    std::vector<sk::incoming_field> incoming = {
        {"x", sk::type_kind::string},
        {"y", sk::type_kind::string},
    };

    auto result = sk::match_variant_by_keys<cfg, object_a, object_c>(incoming);
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(variant_match_picks_higher_score) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming = {
        {"p", sk::type_kind::floating},
        {"q", sk::type_kind::string},
        {"r", sk::type_kind::integer},
        {"extra", sk::type_kind::string},
    };

    auto result = sk::match_variant_by_keys<cfg, object_a, object_b>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1u);
}

TEST_CASE(variant_match_with_non_reflectable_alts) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming = {
        {"x", sk::type_kind::integer},
        {"y", sk::type_kind::string},
    };

    auto result = sk::match_variant_by_keys<cfg, int, std::string, object_a>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 2u);
}

TEST_CASE(variant_match_empty_incoming) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming;

    auto result = sk::match_variant_by_keys<cfg, object_a, object_b>(incoming);
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(variant_match_with_optional_fields) {
    using cfg = config::default_config;
    // with_optional: id(int, required), note(string, optional)
    // simple_struct: x(int, required), y(string, required), z(bool, required)
    std::vector<sk::incoming_field> incoming = {
        {"id", sk::type_kind::integer},
    };

    auto result = sk::match_variant_by_keys<cfg, with_optional, simple_struct>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
}

TEST_CASE(variant_match_first_wins_on_tie) {
    using cfg = config::default_config;
    // Both object_a and object_c match 2 fields, so first wins
    std::vector<sk::incoming_field> incoming = {
        {"x", sk::type_kind::integer},
        {"y", sk::type_kind::string},
        {"w", sk::type_kind::boolean},
    };

    auto result = sk::match_variant_by_keys<cfg, object_a, object_c>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
}

TEST_CASE(variant_match_required_fields_missing) {
    using cfg = config::default_config;
    // simple_struct needs x, y, z — only provide y
    std::vector<sk::incoming_field> incoming = {
        {"y", sk::type_kind::string},
    };

    auto result = sk::match_variant_by_keys<cfg, simple_struct>(incoming);
    EXPECT_FALSE(result.has_value());
}

TEST_CASE(variant_match_kind_any_is_flexible) {
    using cfg = config::default_config;
    std::vector<sk::incoming_field> incoming = {
        {"items", sk::type_kind::any},
    };

    auto result = sk::match_variant_by_keys<cfg, with_vector>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
}

TEST_CASE(variant_match_integer_float_compatible) {
    using cfg = config::default_config;
    // inner_flat has a:int, b:double — provide a as floating, b as integer
    // kind_compatible allows int↔float
    std::vector<sk::incoming_field> incoming = {
        {"a", sk::type_kind::floating},
        {"b", sk::type_kind::integer},
    };

    auto result = sk::match_variant_by_keys<cfg, inner_flat>(incoming);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0u);
}

// ─── match.h: build_kind_to_index_map ───────────────────────────────────────

TEST_CASE(kind_to_index_map_basic) {
    constexpr auto map = sk::build_kind_to_index_map<int, std::string, bool>();
    constexpr auto int_ki = static_cast<std::size_t>(sk::type_kind::integer);
    constexpr auto str_ki = static_cast<std::size_t>(sk::type_kind::string);
    constexpr auto bool_ki = static_cast<std::size_t>(sk::type_kind::boolean);

    static_assert(map.count[int_ki] == 1);
    static_assert(map.indices[int_ki][0] == 0);
    static_assert(map.count[str_ki] == 1);
    static_assert(map.indices[str_ki][0] == 1);
    static_assert(map.count[bool_ki] == 1);
    static_assert(map.indices[bool_ki][0] == 2);
    EXPECT_TRUE(true);
}

TEST_CASE(kind_to_index_map_multiple_same_kind) {
    constexpr auto map = sk::build_kind_to_index_map<int, std::string, std::uint64_t>();
    constexpr auto int_ki = static_cast<std::size_t>(sk::type_kind::integer);

    static_assert(map.count[int_ki] == 2);
    static_assert(map.indices[int_ki][0] == 0);
    static_assert(map.indices[int_ki][1] == 2);
    EXPECT_TRUE(true);
}

TEST_CASE(kind_to_index_map_with_objects) {
    constexpr auto map = sk::build_kind_to_index_map<simple_struct, int, object_a>();
    constexpr auto obj_ki = static_cast<std::size_t>(sk::type_kind::object);
    constexpr auto int_ki = static_cast<std::size_t>(sk::type_kind::integer);

    static_assert(map.count[obj_ki] == 2);
    static_assert(map.indices[obj_ki][0] == 0);
    static_assert(map.indices[obj_ki][1] == 2);
    static_assert(map.count[int_ki] == 1);
    static_assert(map.indices[int_ki][0] == 1);
    EXPECT_TRUE(true);
}

TEST_CASE(kind_to_index_map_empty_kinds) {
    constexpr auto map = sk::build_kind_to_index_map<int>();
    constexpr auto str_ki = static_cast<std::size_t>(sk::type_kind::string);
    constexpr auto bool_ki = static_cast<std::size_t>(sk::type_kind::boolean);
    constexpr auto arr_ki = static_cast<std::size_t>(sk::type_kind::array);

    static_assert(map.count[str_ki] == 0);
    static_assert(map.count[bool_ki] == 0);
    static_assert(map.count[arr_ki] == 0);
    EXPECT_TRUE(true);
}

TEST_CASE(kind_to_index_map_arrays_and_tuples) {
    constexpr auto map =
        sk::build_kind_to_index_map<std::vector<int>, std::tuple<int, double>, int>();
    constexpr auto arr_ki = static_cast<std::size_t>(sk::type_kind::array);
    constexpr auto int_ki = static_cast<std::size_t>(sk::type_kind::integer);

    static_assert(map.count[arr_ki] == 2);
    static_assert(map.indices[arr_ki][0] == 0);
    static_assert(map.indices[arr_ki][1] == 1);
    static_assert(map.count[int_ki] == 1);
    static_assert(map.indices[int_ki][0] == 2);
    EXPECT_TRUE(true);
}

TEST_CASE(kind_to_index_map_all_primitive_kinds) {
    constexpr auto map = sk::build_kind_to_index_map<std::monostate, bool, int, double,
                                                     std::string, std::vector<int>, object_a>();
    constexpr auto null_ki = static_cast<std::size_t>(sk::type_kind::null_like);
    constexpr auto bool_ki = static_cast<std::size_t>(sk::type_kind::boolean);
    constexpr auto int_ki = static_cast<std::size_t>(sk::type_kind::integer);
    constexpr auto float_ki = static_cast<std::size_t>(sk::type_kind::floating);
    constexpr auto str_ki = static_cast<std::size_t>(sk::type_kind::string);
    constexpr auto arr_ki = static_cast<std::size_t>(sk::type_kind::array);
    constexpr auto obj_ki = static_cast<std::size_t>(sk::type_kind::object);

    static_assert(map.count[null_ki] == 1 && map.indices[null_ki][0] == 0);
    static_assert(map.count[bool_ki] == 1 && map.indices[bool_ki][0] == 1);
    static_assert(map.count[int_ki] == 1 && map.indices[int_ki][0] == 2);
    static_assert(map.count[float_ki] == 1 && map.indices[float_ki][0] == 3);
    static_assert(map.count[str_ki] == 1 && map.indices[str_ki][0] == 4);
    static_assert(map.count[arr_ki] == 1 && map.indices[arr_ki][0] == 5);
    static_assert(map.count[obj_ki] == 1 && map.indices[obj_ki][0] == 6);
    EXPECT_TRUE(true);
}

};  // TEST_SUITE(serde_schema)

}  // namespace

}  // namespace eventide::serde
