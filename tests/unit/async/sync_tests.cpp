#include <chrono>

#include "loop_fixture.h"
#include "kota/zest/zest.h"

namespace kota {

namespace {

using namespace std::chrono;

TEST_SUITE(sync, loop_fixture) {

TEST_CASE(mutex_try_lock) {
    mutex m;
    EXPECT_TRUE(m.try_lock());
    EXPECT_FALSE(m.try_lock());
    m.unlock();
    EXPECT_TRUE(m.try_lock());
    m.unlock();
}

TEST_CASE(mutex_lock_order) {
    mutex m;
    int step = 0;

    auto holder = [&]() -> task<> {
        co_await m.lock();
        EXPECT_EQ(step, 0);
        step = 1;
        co_await sleep(milliseconds{5}, loop);
        m.unlock();
    };

    auto waiter = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        co_await m.lock();
        EXPECT_EQ(step, 1);
        step = 2;
        m.unlock();
        loop.stop();
    };

    auto t1 = holder();
    auto t2 = waiter();
    schedule_all(t1, t2);

    EXPECT_EQ(step, 2);
}

TEST_CASE(event_set_wait) {
    event ev;
    int fired = 0;

    auto waiter = [&]() -> task<> {
        co_await ev.wait();
        fired = 1;
        loop.stop();
    };

    auto setter = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev.set();
    };

    auto t1 = waiter();
    auto t2 = setter();
    schedule_all(t1, t2);

    EXPECT_EQ(fired, 1);
}

TEST_CASE(manual_reset_all) {
    event ev(true);
    int count = 0;

    auto waiter = [&]() -> task<> {
        co_await ev.wait();
        count += 1;
        if(count == 2) {
            loop.stop();
        }
    };

    auto t1 = waiter();
    auto t2 = waiter();
    schedule_all(t1, t2);

    EXPECT_EQ(count, 2);
}

TEST_CASE(event_interrupt) {
    event ev;
    bool reached = false;

    auto waiter = [&]() -> task<> {
        co_await ev.wait();
        reached = true;
    };

    auto driver = [&]() -> task<> {
        auto result = co_await waiter().catch_cancel();
        EXPECT_FALSE(result.has_value());
        EXPECT_FALSE(reached);
        loop.stop();
    };

    auto intr = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev.interrupt();
    };

    auto t1 = driver();
    auto t2 = intr();
    schedule_all(t1, t2);
}

TEST_CASE(interrupt_many) {
    event ev;
    int cancelled = 0;

    auto waiter = [&]() -> task<> {
        auto result = co_await ev.wait().catch_cancel();
        EXPECT_FALSE(result.has_value());
        cancelled += 1;
        if(cancelled == 2) {
            loop.stop();
        }
    };

    auto intr = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev.interrupt();
    };

    auto t1 = waiter();
    auto t2 = waiter();
    auto t3 = intr();
    schedule_all(t1, t2, t3);

    EXPECT_EQ(cancelled, 2);
}

TEST_CASE(interrupt_snapshot) {
    event ev;
    int cancelled = 0;
    bool second_wait_cancelled = false;

    auto waiter = [&]() -> task<> {
        auto first = co_await ev.wait().catch_cancel();
        EXPECT_FALSE(first.has_value());
        cancelled += 1;

        auto second = co_await ev.wait().catch_cancel();
        second_wait_cancelled = !second.has_value();
        loop.stop();
    };

    auto intr = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        ev.interrupt();
    };

    auto setter = [&]() -> task<> {
        co_await sleep(milliseconds{2}, loop);
        ev.set();
    };

    auto t1 = waiter();
    auto t2 = intr();
    auto t3 = setter();
    schedule_all(t1, t2, t3);

    EXPECT_EQ(cancelled, 1);
    EXPECT_FALSE(second_wait_cancelled);
}

TEST_CASE(future_wait) {
    event ev;
    bool fired = false;

    ev.interrupt();

    auto waiter = [&]() -> task<> {
        co_await ev.wait();
        fired = true;
        loop.stop();
    };

    auto setter = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        EXPECT_FALSE(fired);
        ev.set();
    };

    auto t1 = waiter();
    auto t2 = setter();
    schedule_all(t1, t2);

    EXPECT_TRUE(fired);
}

TEST_CASE(signal_state) {
    event ev;
    EXPECT_FALSE(ev.is_set());
    ev.interrupt();
    EXPECT_FALSE(ev.is_set());

    event set_ev(true);
    EXPECT_TRUE(set_ev.is_set());
    set_ev.interrupt();
    EXPECT_TRUE(set_ev.is_set());
}

TEST_CASE(semaphore_acquire_release) {
    semaphore sem(1);
    int step = 0;

    auto first = [&]() -> task<> {
        co_await sem.acquire();
        step = 1;
        co_await sleep(milliseconds{5}, loop);
        sem.release();
    };

    auto second = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        co_await sem.acquire();
        EXPECT_EQ(step, 1);
        step = 2;
        sem.release();
        loop.stop();
    };

    auto t1 = first();
    auto t2 = second();
    schedule_all(t1, t2);

    EXPECT_EQ(step, 2);
}

TEST_CASE(condition_variable_wait) {
    mutex m;
    condition_variable cv;
    bool ready = false;
    int step = 0;

    auto waiter = [&]() -> task<> {
        co_await m.lock();
        step = 1;
        while(!ready) {
            co_await cv.wait(m);
        }
        step = 3;
        m.unlock();
        loop.stop();
    };

    auto notifier = [&]() -> task<> {
        co_await sleep(milliseconds{1}, loop);
        co_await m.lock();
        step = 2;
        ready = true;
        cv.notify_one();
        m.unlock();
    };

    auto t1 = waiter();
    auto t2 = notifier();
    schedule_all(t1, t2);

    EXPECT_EQ(step, 3);
}

};  // TEST_SUITE(sync)

// ============================================================================
// TEST_SUITE: deferred_resume — tests for the deferred sync resume mechanism
// ============================================================================

TEST_SUITE(deferred_resume, loop_fixture) {

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

};  // TEST_SUITE(deferred_resume)

}  // namespace

}  // namespace kota
