// TEST_SUITE(when_errors): structured error propagation (co_await fail) through
// when_all/when_any — first error cancels siblings, immediate errors, success
// without false errors, mixed error types, range overloads, error-vs-cancel
// priority, and error beating an external cancel. C++ exception propagation
// lives in exceptions.cpp; cancellation semantics in cancel.cpp.
#include "../../loop_fixture.h"
#include "../../support.h"
#include "kota/zest/zest.h"
#include "kota/async/async.h"

namespace kota {

TEST_SUITE(when_errors) {

TEST_CASE(all_error_cancels_siblings) {
    int slow_done = 0;

    auto failing = [&]() -> task<int, error> {
        co_await sleep(1);
        co_await fail(error::connection_refused);
    };

    auto slow = [&]() -> task<int, error> {
        co_await sleep(50);
        slow_done += 1;
        co_return 42;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(failing(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
    EXPECT_EQ(slow_done, 0);
}

TEST_CASE(all_error_immediate) {
    auto failing = []() -> task<int, error> {
        co_await fail(error::connection_refused);
    };

    auto normal = []() -> task<int, error> {
        co_return 42;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(failing(), normal());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    run(combined());
}

TEST_CASE(all_success_no_false_error) {
    auto a = []() -> task<int, error> {
        co_return 1;
    };

    auto b = []() -> task<int, error> {
        co_return 2;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(a(), b());
        EXPECT_TRUE(res.has_value());
        auto [ra, rb] = *res;
        EXPECT_EQ(ra, 1);
        EXPECT_EQ(rb, 2);
    };

    run(combined());
}

TEST_CASE(all_mixed_error_and_void) {
    auto failing = []() -> task<int, error> {
        co_await fail(error::connection_refused);
    };

    auto void_task = []() -> task<> {
        co_return;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(failing(), void_task());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    run(combined());
}

TEST_CASE(all_operation_aborted) {
    int slow_done = 0;

    auto aborting = [&]() -> task<int, error> {
        co_await fail(error::operation_aborted);
    };

    auto slow = [&]() -> task<int, error> {
        co_await sleep(1);
        slow_done += 1;
        co_return 42;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(aborting(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::operation_aborted);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
    EXPECT_EQ(slow_done, 0);
}

TEST_CASE(all_eof_error) {
    int slow_done = 0;

    auto eof_task = [&]() -> task<int, error> {
        co_await fail(error::end_of_file);
    };

    auto slow = [&]() -> task<int, error> {
        co_await sleep(1);
        slow_done += 1;
        co_return 99;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(eof_task(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::end_of_file);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
    EXPECT_EQ(slow_done, 0);
}

TEST_CASE(any_error_cancels_siblings) {
    int slow_done = 0;

    auto failing = [&]() -> task<int, error> {
        co_await sleep(1);
        co_await fail(error::connection_refused);
    };

    auto slow = [&]() -> task<int, error> {
        co_await sleep(50);
        slow_done += 1;
        co_return 42;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_any(failing(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
    EXPECT_EQ(slow_done, 0);
}

TEST_CASE(all_range_error) {
    auto combined = [&]() -> task<> {
        small_vector<task<int, error>> tasks;
        tasks.emplace_back(delayed_return_error(1, error::connection_refused));
        tasks.emplace_back(delayed_return_value(50, 42));
        auto res = co_await when_all(std::move(tasks));
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
}

TEST_CASE(any_range_error) {
    auto combined = [&]() -> task<> {
        small_vector<task<int, error>> tasks;
        tasks.emplace_back(delayed_return_error(1, error::connection_refused));
        tasks.emplace_back(delayed_return_value(50, 42));
        auto res = co_await when_any(std::move(tasks));
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
}

TEST_CASE(direct_co_await_returns_error) {
    auto failing = []() -> task<int, error> {
        co_await fail(error::connection_refused);
    };

    auto parent = [&]() -> task<int, error> {
        co_return co_await failing();
    };

    auto [res] = run(parent());
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->has_error());
    EXPECT_EQ(res->error(), error::connection_refused);
}

TEST_CASE(nested_manual_propagation) {
    auto failing = [&]() -> task<int, error> {
        co_await sleep(1);
        co_await fail(error::connection_refused);
    };

    auto parent = [&]() -> task<int, error> {
        auto res = co_await when_all(failing(), delayed_return_value(10, 42));
        if(!res) {
            co_await fail(std::move(res).error());
        }
        auto [a, b] = *res;
        co_return a + b;
    };

    auto [res] = run(parent());
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->has_error());
    EXPECT_EQ(res->error(), error::connection_refused);
}

TEST_CASE(with_token_returns_error) {
    cancellation_source source;

    auto failing = [&]() -> task<int, error> {
        co_await sleep(1);
        co_await fail(error::connection_refused);
    };

    auto wrapped = with_token(failing(), source.token());
    run(wrapped);

    auto res = wrapped.result();
    EXPECT_TRUE(res.has_error());
    EXPECT_EQ(res.error(), error::connection_refused);
}

TEST_CASE(with_token_cancels_error_task) {
    cancellation_source source;

    auto slow = [&]() -> task<int, error> {
        co_await sleep(50);
        co_return 42;
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(1);
        source.cancel();
    };

    auto wrapped = with_token(slow(), source.token());
    auto cancel_task = canceler();
    run(wrapped, cancel_task);

    EXPECT_TRUE(wrapped.result().is_cancelled());
}

TEST_CASE(all_mixed_error_types) {
    int slow_done = 0;

    auto failing = [&]() -> task<int, error> {
        co_await sleep(1);
        co_await fail(error::connection_refused);
    };

    auto slow = [&]() -> task<int, custom_error> {
        co_await sleep(50);
        slow_done += 1;
        co_return 42;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(failing(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(std::get<error>(res.error()), error::connection_refused);
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
    EXPECT_EQ(slow_done, 0);
}

TEST_CASE(any_mixed_error_types) {
    int slow_done = 0;

    auto failing = [&]() -> task<int, custom_error> {
        co_await sleep(1);
        co_await fail(custom_error{7});
    };

    auto slow = [&]() -> task<int, error> {
        co_await sleep(50);
        slow_done += 1;
        co_return 42;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_any(failing(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(std::get<custom_error>(res.error()), custom_error{7});
    };

    auto t = combined();
    run(t);

    EXPECT_TRUE(t->is_finished());
    EXPECT_EQ(slow_done, 0);
}

TEST_CASE(any_sync_all_error) {
    auto fail_a = []() -> task<int, error> {
        co_await fail(error::connection_refused);
    };

    auto fail_b = []() -> task<int, error> {
        co_await fail(error::end_of_file);
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_any(fail_a(), fail_b());
        EXPECT_TRUE(res.has_error());
        // first child to complete wins — both are sync, so it's the first in order
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    run(combined());
}

TEST_CASE(error_vs_cancel_priority) {
    auto failing = []() -> task<int, error, cancellation> {
        co_await fail(error::connection_refused);
    };

    auto canceling = []() -> task<int, error, cancellation> {
        co_await cancel();
        co_return 0;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(failing(), canceling());
        // error outranks cancel
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
    };

    run(combined());
}

// trio semantics: a racing external cancel never masks a child error. The
// failing child cancels the whole scope synchronously and then fails; the
// error must survive both the scope cancellation and the child's own
// cancelled state.
TEST_CASE(all_error_beats_external_cancel) {
    async_node* combined_node = nullptr;
    bool checked = false;

    auto failing = [&]() -> task<int, error, cancellation> {
        co_await sleep(1);
        combined_node->cancel();
        co_await fail(error::connection_refused);
    };

    auto slow = [&]() -> task<int, error, cancellation> {
        co_await sleep(50);
        co_return 1;
    };

    auto combined = [&]() -> task<> {
        auto res = co_await when_all(failing(), slow());
        EXPECT_TRUE(res.has_error());
        EXPECT_EQ(res.error(), error::connection_refused);
        checked = true;
    };

    auto t = combined();
    combined_node = t.operator->();
    run(t);

    EXPECT_TRUE(checked);
    EXPECT_TRUE(t->is_cancelled());
}

TEST_CASE(all_range_success_no_false_error) {
    auto combined = [&]() -> task<> {
        small_vector<task<int, error>> tasks;
        tasks.emplace_back(return_value(1));
        tasks.emplace_back(return_value(2));
        tasks.emplace_back(return_value(3));
        auto res = co_await when_all(std::move(tasks));
        EXPECT_TRUE(res.has_value());
        auto& vals = *res;
        EXPECT_EQ(vals.size(), 3);
        EXPECT_EQ(vals[0], 1);
        EXPECT_EQ(vals[1], 2);
        EXPECT_EQ(vals[2], 3);
    };

    run(combined());
}

};  // TEST_SUITE(when_errors)

}  // namespace kota
