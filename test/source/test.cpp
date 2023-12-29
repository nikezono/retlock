#include <doctest/doctest.h>
#include <retlock/version.h>

#include <atomic>
#include <future>
#include <mutex>
#include <retlock/retlock.hpp>
#include <retlock/retlock_same_cacheline.hpp>
#include <retlock/retlock_queue.hpp>
#include <string>
#include <tuple>

/**
 * Testing Classes
 */
#define RECURSIVE_LOCK                                                             \
  std::recursive_mutex, retlock::ReTLock, retlock::ReTLockAFS, retlock::ReTLockAS, \
      retlock::ReTLockNormal, retlock::ReTLockQueue, retlock::ReTLockQueueAFS
#define NORMAL_LOCK RECURSIVE_LOCK, std::mutex

/** Test cases for Exclusive Locking */
TEST_SUITE("Ordinary Lock"
           * doctest::description("Interface & Property of Ordinary Locking Class")) {
  TEST_CASE_TEMPLATE("lock and unlock", T, NORMAL_LOCK) {
    T l;
    l.lock();
    l.unlock();
  }

  TEST_CASE_TEMPLATE("is_locked", T, NORMAL_LOCK) {
    T l;
    std::unique_lock<T> ul(l);
    CHECK(ul.owns_lock());
  }

  TEST_CASE_TEMPLATE("exclusive", T, NORMAL_LOCK) {
    T l;
    std::atomic<bool> lock_failed(false);
    std::atomic<bool> locked(false);
    auto thread1 = std::async(std::launch::async, [&] {
      {
        std::unique_lock<T> ul(l);
        locked.store(true);
        CHECK(ul.owns_lock());
        while (!lock_failed.load()) {
          std::this_thread::yield();
        }
      }
      locked.store(false);
    });

    auto thread2 = std::async(std::launch::async, [&] {
      while (!locked.load()) {
        std::this_thread::yield();
      }
      CHECK(!l.try_lock());
      lock_failed.store(true);
      while (locked.load()) {
        std::this_thread::yield();
      }
      CHECK(l.try_lock());
    });
  }
}

/** Test cases for Reentrant Locking */
TEST_SUITE("Reentrant Lock"
           * doctest::description("Interface & Property of Reentrant Locking Class")) {
  TEST_CASE_TEMPLATE("reentrant", T, RECURSIVE_LOCK) {
    T l;
    l.lock();
    l.lock();
  }

  TEST_CASE_TEMPLATE("is_locked", T, RECURSIVE_LOCK) {
    T l;
    std::unique_lock<T> ul(l);
    CHECK(ul.owns_lock());
    std::unique_lock<T> ul2(l);
    CHECK(ul.owns_lock());
    CHECK(ul2.owns_lock());
    ul2.unlock();
    CHECK(ul.owns_lock());
    CHECK(!ul2.owns_lock());
    ul.unlock();
    CHECK(!ul.owns_lock());
  }

  TEST_CASE_TEMPLATE("exclusive", T, RECURSIVE_LOCK) {
    T l;
    std::atomic<bool> lock_failed(false);
    std::atomic<bool> locked(false);
    auto thread1 = std::async(std::launch::async, [&] {
      {
        std::unique_lock<T> ul(l);
        std::unique_lock<T> ul2(l);
        ul2.unlock();  // locked twice, unlocked once. not yet released.
        locked.store(true);
        CHECK(ul.owns_lock());
        while (!lock_failed.load()) {
          std::this_thread::yield();
        }
      }
      locked.store(false);
    });

    auto thread2 = std::async(std::launch::async, [&] {
      while (!locked.load()) {
        std::this_thread::yield();
      }
      CHECK(!l.try_lock());
      lock_failed.store(true);
      while (locked.load()) {
        std::this_thread::yield();
      }
      CHECK(l.try_lock());
    });
  }
}
