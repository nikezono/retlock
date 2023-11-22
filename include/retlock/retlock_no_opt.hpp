#pragma once

#include <atomic>
#include <bitset>
#include <cassert>
#include <thread>
#include <unordered_map>

namespace retlock {
  /**
   * @brief An optimized implementation of reentrant locking.
   * Compatible with std::recurisve_mutex.
   * @note
   * Public Methods:
   *   - lock()
   *   - unlock()
   *   - try_lock()
   */
  class ReTLockNoOpt {
  public:
    ReTLockNoOpt() : lock_() {}
    ReTLockNoOpt(const ReTLockNoOpt&) = delete;
    ReTLockNoOpt& operator=(const ReTLockNoOpt&) = delete;

    void lock() {
      for (size_t i = 0; !try_lock(); ++i) {
        if (i % 10 == 0) std::this_thread::yield();
        if (i % 100 == 0) std::this_thread::sleep_for(std::chrono::nanoseconds(1 + i / 100));
        // NOTE: glibc uses exponential backoff here
      }
    }

    void unlock() {
      auto current = lock_.load(std::memory_order_relaxed);
      assert(is_already_locked(current));
      auto desired = current;
      desired.counter--;
      if (desired.counter == 0) {
        desired.owner_tid = 0;
      }
      lock_.store(desired, std::memory_order_release);
    }

    bool try_lock() {
      auto current = lock_.load(std::memory_order_relaxed);
      if (is_already_locked(current)) {
        auto desired = current;
        desired.counter++;
        lock_.store(desired);
        return true;
      }

      if (0 < current.counter) return false;
      assert(current.counter == 0);
      assert(current.owner_tid == 0);

      auto desired = current;
      desired.owner_tid = getThreadId();
      desired.counter = 1;

      auto success = lock_.compare_exchange_weak(current, desired, std::memory_order_acquire);
      return success;
    }

  private:
    struct Container {
      uint32_t owner_tid;
      uint32_t counter;
      Container() : owner_tid(0), counter(0) {}
    };

    std::atomic<Container> lock_;

    static std::atomic<uint32_t> thread_id_allocator_;

    inline static uint32_t getThreadId() {
      static thread_local uint32_t thread_id = thread_id_allocator_.fetch_add(1);
      return thread_id;
    }
    inline bool is_already_locked(Container& current) const {
      return current.owner_tid == getThreadId();
    }

    static_assert(std::atomic<Container>::is_always_lock_free, "This class is not lock-free");
  };

  std::atomic<uint32_t> ReTLockNoOpt::thread_id_allocator_(1);
}  // namespace retlock
