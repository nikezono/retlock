#pragma once

#include <math.h>

#include <atomic>
#include <cassert>
#include <new>
#include <thread>

namespace retlock {

  /**
   * @brief An optimized implementation of reentrant locking.
   * Sameline: this implementation uses the same cache line for the lock and the counter.
   * Compatible with std::recurisve_mutex.
   * @note
   * Public Methods:
   *   - lock()
   *   - unlock()
   *   - try_lock()
   */

  enum class SameLineSleepType { NoSleep, Adaptive, Yield, Exponential };

  template <SameLineSleepType Sleep = SameLineSleepType::Exponential> class ReTLockSameLineImpl {
  public:
    ReTLockSameLineImpl() : lock_() {}
    ReTLockSameLineImpl(const ReTLockSameLineImpl&) = delete;
    ReTLockSameLineImpl& operator=(const ReTLockSameLineImpl&) = delete;

    static constexpr uint32_t LOCKED = 1;
    static constexpr uint32_t UNLOCKED = 0;

    void lock() {
      for (size_t i = 0; !try_lock(); ++i) {
        if constexpr (Sleep == SameLineSleepType::NoSleep) {
          continue;
        }
        if constexpr (Sleep == SameLineSleepType::Adaptive) {
          auto current = getLocalLockCache();
          const size_t count = current.counter;
          // Adaptive: if lock is recursively acquired, sleep with exponential backoff. Otherwise
          // spin
          if (count >= 2) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1 << (i)));
          } else {
            continue;
          }
        } else if constexpr (Sleep == SameLineSleepType::Exponential) {
          std::this_thread::sleep_for(std::chrono::nanoseconds(1 << (i)));
        } else if constexpr (Sleep == SameLineSleepType::Yield) {
          std::this_thread::yield();
        } else {
          static_assert(
              Sleep == SameLineSleepType::Adaptive || Sleep == SameLineSleepType::Exponential
                  || Sleep == SameLineSleepType::Yield || Sleep == SameLineSleepType::NoSleep,
              "Invalid SameLineSleepType");
        }
        // NOTE: glibc uses exponential backoff here
      }
    }

    bool try_lock() {
      auto current = lock_.load(std::memory_order_relaxed);
      if constexpr (Sleep == SameLineSleepType::Adaptive) {
        auto& cache = getLocalLockCache();
        cache = current;
      }

      if (isAlreadyLocked(current)) {
        auto desired = current;
        desired.counter++;
        lock_.store(desired);
        return true;
      }
      if (0 < current.counter) return false;
      assert(current.counter == UNLOCKED);
      assert(current.owner_tid == 0);

      auto desired = current;
      desired.owner_tid = getThreadId();
      desired.counter = 1;

      auto success = lock_.compare_exchange_weak(current, desired, std::memory_order_acquire);
      return success;
    }

    void unlock() {
      auto current = lock_.load(std::memory_order_relaxed);
      assert(isAlreadyLocked(current));
      auto desired = current;
      desired.counter--;
      if (desired.counter == 0) {
        desired.owner_tid = 0;
      }
      lock_.store(desired);
    }

  private:
    /** Inner class */
    struct SameCacheLineContainer {
      uint32_t owner_tid;
      uint32_t counter;
      SameCacheLineContainer() : owner_tid(0), counter(0) {}
    };
    static_assert(sizeof(SameCacheLineContainer) == sizeof(uint64_t));
    static_assert(std::atomic<SameCacheLineContainer>::is_always_lock_free,
                  "This class is not lock-free");

    /** Members */
    std::atomic<SameCacheLineContainer> lock_;

    static std::atomic<uint32_t> thread_id_allocator_;

    inline static uint32_t getThreadId() {
      static thread_local uint32_t thread_id = thread_id_allocator_.fetch_add(1);
      return thread_id;
    }

    inline static SameCacheLineContainer& getLocalLockCache() {
      static thread_local SameCacheLineContainer cache{};
      return cache;
    }
    template <typename T> inline bool isAlreadyLocked(T& current) const {
      return current.owner_tid == getThreadId();
    }
  };

  using ReTLockVanilla = ReTLockSameLineImpl<SameLineSleepType::Exponential>;

  /** SameLineSleepType */
  using ReTLockSameLineYield = ReTLockSameLineImpl<SameLineSleepType::Yield>;
  using ReTLockSameLineAdaptive = ReTLockSameLineImpl<SameLineSleepType::Adaptive>;
  using ReTLockSameLineNoSleep = ReTLockSameLineImpl<SameLineSleepType::NoSleep>;

  template <> std::atomic<uint32_t> ReTLockVanilla::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockSameLineYield::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockSameLineAdaptive::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockSameLineNoSleep::thread_id_allocator_(1);
}  // namespace retlock
