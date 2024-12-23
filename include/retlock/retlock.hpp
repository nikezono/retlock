#pragma once

#include <math.h>

#include <atomic>
#include <cassert>
#include <new>
#include <thread>

namespace retlock {

  /**
   * @brief An optimized implementation of reentrant locking.
   * Padding: this implementation uses padding to avoid false sharing of lock and counter.
   * Compatible with std::recurisve_mutex.
   * @note
   * Public Methods:
   *   - lock()
   *   - unlock()
   *   - try_lock()
   */

  enum class SleepType { NoSleep, Adaptive, Yield, Exponential };

  template <SleepType Sleep = SleepType::Exponential> class ReTLockImpl {
  public:
    ReTLockImpl() : lock_(), counter_(0), counter_max_(0) {}
    ReTLockImpl(const ReTLockImpl&) = delete;
    ReTLockImpl& operator=(const ReTLockImpl&) = delete;

    static constexpr uint32_t LOCKED = 1;
    static constexpr uint32_t UNLOCKED = 0;

    void lock() {
      for (size_t i = 0; !try_lock(); ++i) {
        if constexpr (Sleep == SleepType::NoSleep) {
          continue;
        }
        if constexpr (Sleep == SleepType::Adaptive) {
          auto current = getLocalLockCache();
          const size_t adaptive = current.recursive_count_metric;
          std::this_thread::sleep_for(std::chrono::nanoseconds(1 << (adaptive)));
        } else if constexpr (Sleep == SleepType::Exponential) {
          std::this_thread::sleep_for(std::chrono::nanoseconds(1 << (i / 10)));
        } else if constexpr (Sleep == SleepType::Yield) {
          std::this_thread::yield();
        } else {
          static_assert(Sleep == SleepType::Adaptive || Sleep == SleepType::Exponential
                            || Sleep == SleepType::Yield || Sleep == SleepType::NoSleep,
                        "Invalid SleepType");
        }
        // NOTE: glibc uses exponential backoff here
      }
    }

    bool try_lock() {
      auto current = lock_.load(std::memory_order_relaxed);
      if constexpr (Sleep == SleepType::Adaptive) {
        auto& cache = getLocalLockCache();
        cache = current;
      }
      if (isAlreadyLocked(current)) {
        assert(0 < counter_);
        counter_++;
        if constexpr (Sleep == SleepType::Adaptive) {
          counter_max_ = std::max(counter_max_, counter_);
        }
        return true;
      }
      if (LOCKED == current.lockbits) return false;
      assert(current.owner_tid == 0);

      Container desired{getThreadId(), LOCKED, current.recursive_count_metric};

      auto success = lock_.compare_exchange_weak(current, desired);
      if (success) {
        assert(counter_ == 0);
        counter_++;
      }
      return success;
    }

    void unlock() {
      auto current = lock_.load(std::memory_order_relaxed);

      assert(isAlreadyLocked(current));
      assert(0 < counter_);
      counter_--;
      if (0 < counter_) {
        return;
      }

      lock_.store(Container{0, UNLOCKED, 0});
    }

  private:
    /** Inner classes */
    struct Container {
      uint32_t owner_tid : 32;
      uint32_t lockbits : 1;
      uint32_t recursive_count_metric : 31;

      Container() : owner_tid(0), lockbits(0), recursive_count_metric(0) {}
      Container(uint32_t o, uint32_t c, uint32_t r)
          : owner_tid(o), lockbits(c), recursive_count_metric(r) {}
    };
    static_assert(sizeof(Container) == sizeof(uint64_t));
    static_assert(std::atomic<Container>::is_always_lock_free, "This class is not lock-free");

    /** Members */
    alignas(64) std::atomic<Container> lock_;
    alignas(64) size_t counter_;
    size_t counter_max_;

    static std::atomic<uint32_t> thread_id_allocator_;

    inline static uint32_t getThreadId() {
      static thread_local uint32_t thread_id = thread_id_allocator_.fetch_add(1);
      return thread_id;
    }

    inline static Container& getLocalLockCache() {
      static thread_local Container cache{};
      return cache;
    }

    template <typename T> inline bool isAlreadyLocked(T& current) const {
      return current.owner_tid == getThreadId();
    }
  };

  using ReTLockPadding = ReTLockImpl<SleepType::Exponential>;
  using ReTLockYieldPadding = ReTLockImpl<SleepType::Yield>;
  using ReTLockAdaptivePadding = ReTLockImpl<SleepType::Adaptive>;
  using ReTLockNoSleepPadding = ReTLockImpl<SleepType::NoSleep>;

  using ReTLock = ReTLockAdaptivePadding;

  template <> std::atomic<uint32_t> ReTLockPadding::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockYieldPadding::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockAdaptivePadding::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockNoSleepPadding::thread_id_allocator_(1);
}  // namespace retlock
