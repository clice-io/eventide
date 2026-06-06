#include "loop_fixture.h"
#include "kota/zest/zest.h"

namespace kota {

namespace {

TEST_SUITE(dispatch, loop_fixture) {

// When task A holds a mutex and task B is waiting on it, A's unlock()
// calls dispatch() for B while A is still being dispatched. The
// reentrancy guard must defer B's resume and drain it afterward.
TEST_CASE(nested_dispatch_via_mutex) {
    mutex m;
    event gate;
    bool holder_done = false;
    bool waiter_done = false;
    bool waiter_deferred = false;

    auto holder = [&]() -> task<> {
        co_await m.lock();
        co_await gate.wait();
        m.unlock();
        // After unlock(), the waiter's resume was deferred — it should
        // not have run yet inside the holder's dispatch frame.
        waiter_deferred = !waiter_done;
        holder_done = true;
    };

    auto waiter = [&]() -> task<> {
        gate.set();
        co_await m.lock();
        waiter_done = true;
        m.unlock();
    };

    auto t1 = holder();
    auto t2 = waiter();
    schedule_all(t1, t2);

    EXPECT_TRUE(holder_done);
    EXPECT_TRUE(waiter_done);
    EXPECT_TRUE(waiter_deferred);
}

// event::set() calls resume_waiter -> dispatch() for each suspended
// waiter. When that happens inside an already-dispatching context,
// the waiters are deferred and drained after the outer dispatch.
TEST_CASE(nested_dispatch_via_event) {
    event ev;
    bool setter_done = false;
    bool waiter_done = false;
    bool waiter_deferred = false;

    auto setter_task = [&]() -> task<> {
        co_await sleep(1, loop);
        ev.set();
        // The waiter's resume was deferred — verify it hasn't run yet.
        waiter_deferred = !waiter_done;
        setter_done = true;
    };

    auto waiter_task = [&]() -> task<> {
        co_await ev.wait();
        waiter_done = true;
    };

    auto t1 = setter_task();
    auto t2 = waiter_task();
    schedule_all(t1, t2);

    EXPECT_TRUE(setter_done);
    EXPECT_TRUE(waiter_done);
    EXPECT_TRUE(waiter_deferred);
}

// Several coroutines are deferred during a single outer dispatch.
// All of them must execute before the dispatching flag is cleared.
TEST_CASE(multiple_deferred_items) {
    event ev;
    int completed = 0;
    bool all_deferred = false;

    auto setter = [&]() -> task<> {
        co_await sleep(1, loop);
        ev.set();
        // All three waiters should be deferred, none completed yet.
        all_deferred = (completed == 0);
    };

    auto waiter = [&]() -> task<> {
        co_await ev.wait();
        completed += 1;
    };

    auto t0 = setter();
    auto t1 = waiter();
    auto t2 = waiter();
    auto t3 = waiter();
    schedule_all(t0, t1, t2, t3);

    EXPECT_EQ(completed, 3);
    EXPECT_TRUE(all_deferred);
}

// A chain of dispatch -> deferred -> dispatch -> deferred.
// Task A sets event1, waking B (deferred). B sets event2, waking C
// (also deferred, appended during the drain loop). C sets event3,
// waking D. All four tasks must complete, exercising the index-based
// drain loop that picks up newly appended deferred items.
TEST_CASE(deferred_item_defers_more) {
    event ev1;
    event ev2;
    event ev3;
    int completed = 0;

    // task_a triggers the chain by setting ev1 inside a dispatch frame.
    auto task_a = [&]() -> task<> {
        co_await sleep(1, loop);
        ev1.set();
    };

    auto task_b = [&]() -> task<> {
        co_await ev1.wait();
        // Now executing inside drain_deferred(). Setting ev2 causes
        // task_c to be appended to the deferred vector while it is
        // being drained.
        ev2.set();
        completed += 1;
    };

    auto task_c = [&]() -> task<> {
        co_await ev2.wait();
        // Appended during drain_deferred by task_b's ev2.set().
        // Setting ev3 chains one more level of deferred dispatch.
        ev3.set();
        completed += 1;
    };

    auto task_d = [&]() -> task<> {
        co_await ev3.wait();
        completed += 1;
    };

    auto ta = task_a();
    auto tb = task_b();
    auto tc = task_c();
    auto td = task_d();
    schedule_all(ta, tb, tc, td);

    EXPECT_EQ(completed, 3);
}

// Semaphore::release wakes waiters via dispatch(), exercising the
// same reentrancy path through a different sync primitive. Releasing
// multiple permits in one call should defer each resumed waiter.
TEST_CASE(nested_dispatch_via_semaphore) {
    semaphore sem(0);
    int completed = 0;
    bool all_deferred = false;

    auto releaser = [&]() -> task<> {
        co_await sleep(1, loop);
        sem.release(3);
        // All three waiters should be deferred, none completed yet.
        all_deferred = (completed == 0);
    };

    auto waiter = [&]() -> task<> {
        co_await sem.acquire();
        completed += 1;
    };

    auto t0 = releaser();
    auto t1 = waiter();
    auto t2 = waiter();
    auto t3 = waiter();
    schedule_all(t0, t1, t2, t3);

    EXPECT_EQ(completed, 3);
    EXPECT_TRUE(all_deferred);
}

};  // TEST_SUITE(dispatch)

}  // namespace

}  // namespace kota
