#pragma once

#include <atomic>
#include <cassert>
#include <new>
#include <thread>

namespace retlock {

  constexpr std::size_t cache_line_size() { return 64; }

  /**
   * @brief An optimized implementation of reentrant locking.
   * Compatible with std::recurisve_mutex.
   * @note
   * Public Methods:
   *   - lock()
   *   - unlock()
   *   - try_lock()
   */
  class ReTLock {
  public:
    ReTLock() : lock_(), counter_(0) {}
    ReTLock(const ReTLock&) = delete;
    ReTLock& operator=(const ReTLock&) = delete;

    void lock() {
      for (size_t i = 0; !try_lock(); ++i) {
        if (i % 10 == 0) std::this_thread::yield();
        if (i % 100 == 0) std::this_thread::sleep_for(std::chrono::microseconds(1 + i / 100));
        // NOTE: glibc uses exponential backoff here
      }
    }

    void unlock() {
      auto current = lock_.load(std::memory_order_relaxed);
      assert(is_already_locked(current));

      assert(0 < current.counter);
      assert(0 < counter_);
      counter_--;
      if (0 < counter_) return;
      lock_.store({}, std::memory_order_release);
    }

    bool try_lock() {
      auto current = lock_.load(std::memory_order_relaxed);
      if (is_already_locked(current)) {
        assert(0 < counter_);
        assert(0 < current.counter);
        counter_++;
        return true;
      }

      if (0 < current.counter) return false;
      assert(current.counter == 0);
      assert(current.owner_tid == 0);

      Container desired{getThreadId(), 1};

      auto success = lock_.compare_exchange_weak(current, desired, std::memory_order_acquire);
      if (success) counter_ = 1;
      return success;
    }

  private:
    struct Container {
      uint32_t owner_tid;
      uint32_t counter;
      Container() : owner_tid(0), counter(0) {}
      Container(uint32_t o, uint32_t c) : owner_tid(o), counter(c) {}
    };

    alignas(cache_line_size()) std::atomic<Container> lock_;
    alignas(cache_line_size()) size_t counter_;

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

  std::atomic<uint32_t> ReTLock::thread_id_allocator_(1);
}  // namespace retlock
