#pragma once

#include <array>
#include <string_view>

#include "runner.h"

namespace eventide::zest {

template <std::size_t N>
struct fixed_string : std::array<char, N + 1> {
    constexpr fixed_string(const char* str) {
        for(std::size_t i = 0; i < N; ++i) {
            this->data()[i] = str[i];
        }
        this->data()[N] = '\0';
    }

    template <std::size_t M>
    constexpr fixed_string(const char (&str)[M]) {
        for(std::size_t i = 0; i < N; ++i) {
            this->data()[i] = str[i];
        }
        this->data()[N] = '\0';
    }

    constexpr static auto size() {
        return N;
    }

    constexpr operator std::string_view() const {
        return std::string_view(this->data(), N);
    }
};

template <std::size_t M>
fixed_string(const char (&)[M]) -> fixed_string<M - 1>;

template <fixed_string TestName, typename Derived>
struct TestSuiteDef {
private:
    TestState state = TestState::Passed;

public:
    using Self = Derived;

    void failure() {
        state = TestState::Failed;
    }

    void pass() {
        state = TestState::Passed;
    }

    void skip() {
        state = TestState::Skipped;
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
        Runner::instance().add_suite(TestName.data(), &suites);
        return true;
    }();

    template <fixed_string case_name,
              auto test_body,
              fixed_string path,
              std::size_t line,
              TestAttrs attrs = {}>
    inline static bool _register_test_case = [] {
        auto run_test = +[] -> TestState {
            Derived test;
            if constexpr(requires { test.setup(); }) {
                test.setup();
            }

            (test.*test_body)();

            if constexpr(requires { test.teardown(); }) {
                test.teardown();
            }

            return test.state;
        };

        test_cases().emplace_back(case_name.data(), path.data(), line, attrs, run_test);
        return true;
    }();
};

}  // namespace eventide::zest
