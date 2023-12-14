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

  template <bool AdaptiveSleep = false> class ReTLockImpl {
  public:
    ReTLockImpl() : lock_(), counter_(0), counter_max_(0) {}
    ReTLockImpl(const ReTLockImpl&) = delete;
    ReTLockImpl& operator=(const ReTLockImpl&) = delete;

    static constexpr uint32_t LOCKED = 1;
    static constexpr uint32_t UNLOCKED = 0;

    void lock() {
      for (size_t i = 0; !try_lock(); ++i) {
        if (i % 10 == 0) std::this_thread::yield();
        if (i % 100 == 0) {
          if constexpr (AdaptiveSleep) {
            const size_t adaptive = lock_.load(std::memory_order_relaxed).recursive_count_metric;
            std::this_thread::sleep_for(std::chrono::nanoseconds(1 + (i / 100) * adaptive));
          } else {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1 + i / 100));
          }
          // NOTE: glibc uses exponential backoff here
        }
      }
    }

    void unlock() {
      auto current = lock_.load(std::memory_order_relaxed);
      assert(is_already_locked(current));

      assert(LOCKED == current.lockbits);
      assert(0 < counter_);
      counter_--;
      if (0 < counter_) return;
      decltype(current.recursive_count_metric) new_metric
          = (current.recursive_count_metric + counter_max_ / 2);
      lock_.store({0, UNLOCKED, new_metric}, std::memory_order_release);
    }

    bool try_lock() {
      auto current = lock_.load(std::memory_order_relaxed);
      if (is_already_locked(current)) {
        assert(0 < counter_);
        assert(LOCKED == current.lockbits);
        counter_++;
        counter_max_ = std::max(counter_max_, counter_);
        return true;
      }

      if (LOCKED == current.lockbits) return false;
      assert(UNLOCKED == current.lockbits);
      assert(current.owner_tid == 0);

      Container desired{getThreadId(), LOCKED, current.recursive_count_metric};

      auto success = lock_.compare_exchange_weak(current, desired, std::memory_order_acquire);
      if (success) counter_ = 1;
      return success;
    }

  private:
    struct Container {
      uint32_t owner_tid : 32;
      uint32_t lockbits : 1;
      uint32_t recursive_count_metric : 31;

      Container() : owner_tid(0), lockbits(0), recursive_count_metric(0) {}
      Container(uint32_t o, uint32_t c, uint32_t r)
          : owner_tid(o), lockbits(c), recursive_count_metric(r) {}
    };

    alignas(cache_line_size()) std::atomic<Container> lock_;
    alignas(cache_line_size()) size_t counter_;
    size_t counter_max_;

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

  using ReTLockAFS = ReTLockImpl<false>;
  using ReTLock = ReTLockImpl<true>;

  template <> std::atomic<uint32_t> ReTLockAFS::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLock::thread_id_allocator_(1);
}  // namespace retlock
