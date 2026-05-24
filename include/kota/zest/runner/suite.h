#pragma once

#include "kota/zest/runner/registry.h"
#include "kota/zest/snapshot/snapshot.h"
#include "kota/meta/name.h"

namespace kota::zest {

/// Strip the "test_" prefix from a test case name, if present.
constexpr std::string_view strip_test_prefix(std::string_view name) {
    if(name.starts_with("test_")) {
        name.remove_prefix(5);
    }
    return name;
}

/// Merge suite-level and case-level test attributes.
/// Case-level flags override suite defaults when explicitly set to true.
constexpr TestAttrs merge_attrs(TestAttrs suite, TestAttrs test_case) {
    return {
        .skip = suite.skip || test_case.skip,
        .focus = suite.focus || test_case.focus,
        .serial = suite.serial || test_case.serial,
    };
}

template <typename Derived>
struct TestSuiteDef {
    using Self = Derived;

    constexpr static auto _suite_name() {
        auto name = meta::type_name<Derived>();
        if(name.ends_with("TEST")) {
            name = name.drop_back(4);
        }
        return name;
    }

    constexpr inline static auto& test_cases() {
        static std::vector<TestCase> instance;
        return instance;
    }

    constexpr inline static auto suites() {
        return std::move(test_cases());
    }

    template <typename T = void>
    inline static bool _register_suites = [] {
        auto sn = _suite_name();
        Runner::instance().add_suite(std::string_view(sn.data(), sn.size()), &suites);
        return true;
    }();

    template <auto test_body, const char* path, std::size_t line, TestAttrs attrs = {}>
    inline static bool _register_test_case = [] {
        constexpr auto effective_attrs = [] {
            if constexpr(requires { Derived::suite_attrs; }) {
                return merge_attrs(Derived::suite_attrs, attrs);
            } else {
                return attrs;
            }
        }();

        constexpr auto case_name_ref = meta::member_name<test_body>();

        auto run_test = +[] -> TestState {
            current_test_state() = TestState::Passed;
            constexpr auto sn = _suite_name();
            constexpr auto cn = meta::member_name<test_body>();
            auto cn_sv = strip_test_prefix(std::string_view(cn.data(), cn.size()));
            reset_snapshot_context(std::string_view(sn.data(), sn.size()), cn_sv, path);
            Derived test;
            if constexpr(requires { test.setup(); }) {
                test.setup();
            }

            (test.*test_body)();

            if constexpr(requires { test.teardown(); }) {
                test.teardown();
            }

            return current_test_state();
        };

        auto cn = strip_test_prefix(std::string_view(case_name_ref.data(), case_name_ref.size()));
        test_cases().emplace_back(std::string(cn), path, line, effective_attrs, run_test);
        return true;
    }();
};

}  // namespace kota::zest
