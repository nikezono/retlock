#pragma once

#include <atomic>
#include <bitset>
#include <cassert>
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
    ReTLockImpl() : bits_(0) {}
    ReTLockImpl(const ReTLockImpl&) = delete;
    ReTLockImpl& operator=(const ReTLockImpl&) = delete;

    void lock() {
      for (;;) {
        if (try_lock()) break;
      }
    }
    void unlock() {
      auto* counter = get_counter();

      if constexpr (Optimized) {
        assert(0 < *counter);
        (*counter)--;
        if (0 < *counter) return;
        bits_.store(0, std::memory_order_relaxed);
      } else {
        assert(0 < *counter);
        const auto sub = bits_.fetch_sub(1llu, std::memory_order_relaxed);
        if (sub == 1llu) *counter = 0;
      }
    }
    bool try_lock() {
      auto* counter = get_counter();

      if constexpr (Optimized) {
        if (0 < *counter) {
          (*counter)++;
          return true;
        }

        auto current = bits_.load(std::memory_order_relaxed);
        if (current & 1llu) return false;

        size_t desired = 1llu;
        auto success = bits_.compare_exchange_weak(current, desired);
        if (success) (*counter)++;

        return success;
      } else {
        if (0 < *counter) {
          bits_.fetch_add(1llu, std::memory_order_relaxed);
          return true;
        } else {
          auto current = bits_.load(std::memory_order_relaxed);
          if (current & 1llu) return false;

          size_t desired = 1llu;
          auto success = bits_.compare_exchange_weak(current, desired);
          if (success) *counter = 1;
          return success;
        }
      }
    }

    using Container = uint64_t;

  private:
    std::atomic<Container> bits_;
    static thread_local std::unordered_map<decltype(bits_)*, size_t> counter_;

    size_t* get_counter() {
      auto it = counter_.find(&bits_);
      if (it == counter_.end()) {
        counter_[&bits_] = 0;
        it = counter_.find(&bits_);
      }
      return &it->second;
    }

    static_assert(std::atomic<Container>::is_always_lock_free, "This class is not lock-free");
  };

  template <>
  thread_local std::unordered_map<std::atomic<ReTLockImpl<>::Container>*, size_t> ReTLock::counter_
      = {};
  template <> thread_local std::unordered_map<std::atomic<ReTLockImpl<>::Container>*, size_t>
      ReTLockNoOp::counter_ = {};

}  // namespace retlock
