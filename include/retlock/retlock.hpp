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
  template <bool Optimized> class ReTLockImpl;

  using ReTLock = ReTLockImpl<true>;
  using ReTLockNoOp = ReTLockImpl<false>;

  /**
   *  --- Implementation ---
   **/

  template <bool Optimized = true> class ReTLockImpl {
  public:
    ReTLockImpl() : lock_() {}
    ReTLockImpl(const ReTLockImpl&) = delete;
    ReTLockImpl& operator=(const ReTLockImpl&) = delete;

    void lock() {
      for (;;) {
        if (try_lock()) break;
      }
    }
    void unlock() {
      auto current = lock_.load(std::memory_order_relaxed);
      assert(is_already_locked(current));

      if constexpr (Optimized) {
        auto& counter = get_counter();
        assert(0 < counter);
        counter--;
        if (0 < counter) return;
        lock_.store({}, std::memory_order_relaxed);
      } else {
        auto desired = current;
        desired.counter--;

        if (desired.counter == 0) {
          desired.owner_tid = 0;
        }
        lock_.store(desired, std::memory_order_relaxed);
      }
    }

    bool try_lock() {
      auto current = lock_.load(std::memory_order_relaxed);
      if (is_already_locked(current)) {
        if constexpr (Optimized) {
          auto& counter = get_counter();
          assert(0 < counter);
          counter++;
          return true;
        } else {
          auto desired = current;
          desired.counter++;
          lock_.store(desired, std::memory_order_relaxed);
          return true;
        }
      }

      if (0 < current.counter) return false;
      assert(current.counter == 0);
      assert(current.owner_tid == 0);

      auto desired = current;
      desired.owner_tid = getThreadId();
      desired.counter = 1;

      auto success = lock_.compare_exchange_weak(current, desired);
      if constexpr (Optimized) {
        if (success) counter_ = 1;
      }
      return success;
    }

    // TODO do not use unordered_map! optimize!
    // Thread-local な vector を用意するのはどうだろう．
    // thread_local std::vector<size_t> counters_; とする．
    // lock object と index の対応を取る方法は？ hash? conflictは許されない
    // bionicは 32bit にtid, 32bit にcounter を持っている．同じでいくか？

  private:
    struct Container {
      uint32_t owner_tid;
      uint32_t counter;
      Container() : owner_tid(0), counter(0) {}
    };

    alignas(64) std::atomic<Container> lock_;
    alignas(64) size_t counter_;

    static std::atomic<uint32_t> thread_id_allocator_;

    inline size_t& get_counter() { return counter_; }
    inline static uint32_t getThreadId() {
      static thread_local uint32_t thread_id = thread_id_allocator_.fetch_add(1);
      return thread_id;
    }
    inline bool is_already_locked(Container& current) { return current.owner_tid == getThreadId(); }

    static_assert(std::atomic<Container>::is_always_lock_free, "This class is not lock-free");
  };

  template <> std::atomic<uint32_t> ReTLock::thread_id_allocator_(1);
  template <> std::atomic<uint32_t> ReTLockNoOp::thread_id_allocator_(1);

}  // namespace retlock
