// TEST_SUITE(sync_deferred): the deferred sync-resume mechanism. When a sync
// primitive (mutex/semaphore/cv/event) hands off ownership to a waiter, the
// resume is deferred; these tests cover a cancelled waiter abandoning the grant
// (slot/lock recovery), same-tick and chained deferred resumes, and interrupt
// after cancel. Happy-path primitive behavior lives in sync_tests.cpp.
#include <chrono>

#include "../loop_fixture.h"
#include "kota/zest/zest.h"

namespace kota {

namespace {

using namespace std::chrono;

TEST_SUITE(sync_deferred, loop_fixture) {

// when_any: a holds mutex, sleeps, unlocks (defers b's resume), then co_returns
// (winner). when_any cancels b synchronously. The deferred resume fires on a
// cancelled task — abandon_fn must release the transferred mutex lock.
TEST_CASE(any_cancel_abandons_deferred_mutex_grant) {
    mutex m;

    auto a = [&]() -> task<int> {
        co_await m.lock();
        co_await sleep(milliseconds{1}, loop);
        m.unlock();
        co_return 1;
    };

    auto b = [&]() -> task<int> {
        co_await m.lock();
        co_return 2;
    };

    auto combined = [&]() -> task<> {
        auto winner = co_await when_any(a(), b());
        EXPECT_EQ(winner.index(), 0U);
        EXPECT_EQ(std::get<0>(winner), 1);
    };

    auto t = combined();
    schedule_all(t);

    // Mutex must be usable — b's deferred lock grant was abandoned
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

// when_any: b releases semaphore (defers a's resume), then b co_returns (winner).
// when_any cancels a — abandon_fn must release the semaphore slot.
TEST_CASE(any_cancel_abandons_deferred_semaphore_grant) {
    semaphore sem(0);

    auto a = [&]() -> task<int> {
        co_await sem.acquire();
        co_return 1;
    };

    auto b = [&]() -> task<int> {
        co_await sleep(milliseconds{1}, loop);
        sem.release();
        co_return 2;
    };

    auto combined = [&]() -> task<> {
        auto winner = co_await when_any(a(), b());
        EXPECT_EQ(winner.index(), 1U);
        EXPECT_EQ(std::get<1>(winner), 2);
    };

    auto t = combined();
    schedule_all(t);

    // Semaphore slot must be recovered
    EXPECT_TRUE(sem.try_acquire());
}

// Mutex unlock + cancel race: both defer in the same tick. Regardless of which
// fires first, the mutex must remain usable afterwards.
TEST_CASE(mutex_cancel_and_deferred_resume_race) {
    mutex m;
    cancellation_source source;

    auto holder = [&]() -> task<> {
        co_await m.lock();
        co_await sleep(milliseconds{5}, loop);
        m.unlock();
        source.cancel();
    };

    auto waiter = [&]() -> task<int> {
        co_await m.lock();
        m.unlock();
        co_return 1;
    };

    auto holder_task = holder();
    auto guarded = with_token(waiter(), source.token());
    schedule_all(holder_task, guarded);

    // Regardless of outcome, the mutex must be usable
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

// When all waiters on a semaphore are cancelled before release, the slot must
// not be lost.
TEST_CASE(semaphore_slot_recovery_all_waiters_cancelled) {
    semaphore sem(0);
    cancellation_source source;
    int acquired_count = 0;

    auto waiter = [&]() -> task<int> {
        co_await sem.acquire();
        acquired_count += 1;
        co_return 1;
    };

    auto canceler = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        source.cancel();
        sem.release();
    };

    auto g1 = with_token(waiter(), source.token());
    auto g2 = with_token(waiter(), source.token());
    auto cancel_task = canceler();
    schedule_all(g1, g2, cancel_task);

    EXPECT_EQ(acquired_count, 0);
    EXPECT_FALSE(g1.value().has_value());
    EXPECT_FALSE(g2.value().has_value());

    // The released slot must still be available
    EXPECT_TRUE(sem.try_acquire());
}

// semaphore::release with active waiters that are all already cancelled —
// the slot goes back to the count.
TEST_CASE(semaphore_release_with_cancelled_waiters_recovers_slot) {
    semaphore sem(0);
    cancellation_source source;

    auto waiter = [&]() -> task<int> {
        co_await sem.acquire();
        co_return 1;
    };

    auto driver = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        source.cancel();
        co_await sleep(milliseconds{1}, loop);
        sem.release(2);
    };

    auto g1 = with_token(waiter(), source.token());
    auto g2 = with_token(waiter(), source.token());
    auto driver_task = driver();
    schedule_all(g1, g2, driver_task);

    EXPECT_FALSE(g1.value().has_value());
    EXPECT_FALSE(g2.value().has_value());

    EXPECT_TRUE(sem.try_acquire());
    EXPECT_TRUE(sem.try_acquire());
    EXPECT_FALSE(sem.try_acquire());
}

// Multiple sync primitives defer resumes in the same tick — all should fire.
TEST_CASE(multiple_deferred_resumes_in_same_tick) {
    event ev1;
    event ev2;
    int done_count = 0;

    auto waiter1 = [&]() -> task<> {
        co_await ev1.wait();
        done_count += 1;
    };

    auto waiter2 = [&]() -> task<> {
        co_await ev2.wait();
        done_count += 1;
    };

    auto signaler = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev1.set();
        ev2.set();
    };

    auto t1 = waiter1();
    auto t2 = waiter2();
    auto t3 = signaler();
    schedule_all(t1, t2, t3);

    EXPECT_EQ(done_count, 2);
}

// A deferred resume triggers code that itself defers another resume (chained).
// Both must complete within the same drain cycle.
TEST_CASE(chained_deferred_resumes) {
    event ev1;
    event ev2;
    int step = 0;

    auto chain_end = [&]() -> task<> {
        co_await ev2.wait();
        step = 2;
        loop.stop();
    };

    auto chain_middle = [&]() -> task<> {
        co_await ev1.wait();
        step = 1;
        ev2.set();
    };

    auto trigger = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev1.set();
    };

    auto t1 = chain_end();
    auto t2 = chain_middle();
    auto t3 = trigger();
    schedule_all(t1, t2, t3);

    EXPECT_EQ(step, 2);
}

// Three events chained: A → B → C. Verifies the drain while-loop processes
// items queued by earlier iterations.
TEST_CASE(triple_chained_deferred_resumes) {
    event ev1;
    event ev2;
    event ev3;
    int step = 0;

    auto c = [&]() -> task<> {
        co_await ev3.wait();
        step = 3;
        loop.stop();
    };

    auto b = [&]() -> task<> {
        co_await ev2.wait();
        step = 2;
        ev3.set();
    };

    auto a = [&]() -> task<> {
        co_await ev1.wait();
        step = 1;
        ev2.set();
    };

    auto trigger = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev1.set();
    };

    auto t1 = c();
    auto t2 = b();
    auto t3 = a();
    auto t4 = trigger();
    schedule_all(t1, t2, t3, t4);

    EXPECT_EQ(step, 3);
}

// event::interrupt() defers cancel for each waiter. If a waiter is already
// cancelled via cancellation_source, the interrupt should be safe.
TEST_CASE(interrupt_after_cancel_is_safe) {
    event ev;
    cancellation_source source;

    auto waiter = [&]() -> task<int> {
        co_await ev.wait();
        co_return 42;
    };

    auto driver = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        source.cancel();
        co_await sleep(milliseconds{1}, loop);
        ev.interrupt();
    };

    auto guarded = with_token(waiter(), source.token());
    auto driver_task = driver();
    schedule_all(guarded, driver_task);

    EXPECT_FALSE(guarded.value().has_value());
}

// Multiple deferred resumes from the same unlock: a mutex with multiple waiters,
// all should eventually acquire and release.
TEST_CASE(mutex_multiple_waiters_all_resume) {
    mutex m;
    int acquired_count = 0;

    auto holder = [&]() -> task<> {
        co_await m.lock();
        co_await sleep(milliseconds{1}, loop);
        m.unlock();
    };

    auto waiter = [&]() -> task<> {
        co_await m.lock();
        acquired_count += 1;
        m.unlock();
    };

    auto w1 = waiter();
    auto w2 = waiter();
    auto w3 = waiter();
    auto h = holder();
    schedule_all(h, w1, w2, w3);

    EXPECT_EQ(acquired_count, 3);
}

// when_all with deferred resume: both children complete via deferred resume.
// Verifies the aggregate settles correctly when completions arrive from drain.
TEST_CASE(all_both_children_deferred) {
    event ev1;
    event ev2;
    int a_done = 0;
    int b_done = 0;

    auto a = [&]() -> task<> {
        co_await ev1.wait();
        a_done = 1;
    };

    auto b = [&]() -> task<> {
        co_await ev2.wait();
        b_done = 1;
    };

    auto signaler = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev1.set();
        ev2.set();
    };

    auto combined = [&]() -> task<> {
        co_await when_all(a(), b());
    };

    auto c = combined();
    auto s = signaler();
    schedule_all(c, s);

    EXPECT_EQ(a_done, 1);
    EXPECT_EQ(b_done, 1);
}

// CV notify inside when_any: notifier defers waiter's resume, then co_returns.
// when_any cancels the waiter. The mutex held by the CV waiter must be released
// properly (cv.wait re-acquires the mutex before resuming the waiter).
TEST_CASE(any_cancel_deferred_cv_wait) {
    mutex m;
    condition_variable cv;

    auto a = [&]() -> task<int> {
        co_await m.lock();
        co_await cv.wait(m);
        m.unlock();
        co_return 1;
    };

    auto b = [&]() -> task<int> {
        co_await sleep(milliseconds{1}, loop);
        cv.notify_one();
        co_return 2;
    };

    auto combined = [&]() -> task<> {
        auto winner = co_await when_any(a(), b());
        EXPECT_EQ(winner.index(), 1U);
        EXPECT_EQ(std::get<1>(winner), 2);
    };

    auto t = combined();
    schedule_all(t);

    // Mutex must be usable after the cancelled CV wait
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

};  // TEST_SUITE(sync_deferred)

}  // namespace

}  // namespace kota
