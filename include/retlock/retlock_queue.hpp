#pragma once

#include <atomic>
#include <cassert>
#include <new>
#include <thread>
#include <vector>
#include <memory>

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

  template <bool AdaptiveSleep = false> class ReTLockQueueImpl {
  public:
    ReTLockQueueImpl() : tail_(nullptr) {}
    ReTLockQueueImpl(const ReTLockQueueImpl&) = delete;
    ReTLockQueueImpl& operator=(const ReTLockQueueImpl&) = delete;

    void lock() {
      auto result = try_lock(false);
      assert(result == true);
    }

    void unlock() {
      auto* my_node = getMyQNode();
      assert(my_node->counter_ > 0);
      my_node->counter_--;
      if constexpr (AdaptiveSleep) {
        auto* next = my_node->next_.load();
        if (next != nullptr) {
          next->waiting_.store(my_node->counter_);
	  if (my_node->counter_ == 0) return; // already unlocked.
        }
      }
      if (my_node->counter_ > 0) return;

      auto* next = my_node->next_.load();
      if (next == nullptr) {
        auto* expected = my_node;
        // my_node may be the tail_. set tail to nullptr
        if (tail_.compare_exchange_strong(expected, nullptr)) {
          return;
        }
        // someone has interleaved.
        while (next == nullptr) {
          next = my_node->next_.load();
        }
      }
      assert(next != nullptr);
      next->waiting_.store(false);
    }

    bool try_lock(bool no_wait = true) {
      auto* my_node = getMyQNode();

      if (0 < my_node->counter_) {
        assert(my_node->waiting_.load() == false);
        my_node->counter_++;
        if constexpr (AdaptiveSleep) {
          auto* next = my_node->next_.load();
          if (next != nullptr) {
            next->waiting_.store(my_node->counter_);
          }
        }
        return true;
      }

      my_node->counter_ = 1;
      my_node->next_.store(nullptr);
      my_node->waiting_.store(true);

      auto* current = tail_.load();
      assert(current != my_node);

      // queue is not empty
      if (current != nullptr && no_wait) {
        my_node->reset();
        return false;
      }

      // enqueue
      auto* pred = tail_.exchange(my_node);
      if (pred != nullptr) {
        pred->next_.store(my_node);
      } else {
        my_node->waiting_.store(false);
        return true;
      }

      // wait for unlock
      for (;;) {
        auto waiting = my_node->waiting_.load();
        if (!waiting) return true;
        if constexpr (AdaptiveSleep) {
          if (1 < waiting) {
            // lock holder is in reentrant mode.
            // it seems that I should wait for a while.
            std::this_thread::yield();
          }
        }
      }
    }

  private:
    static constexpr std::size_t cache_line_size() { return 64; }

    struct QNode {
      std::atomic<QNode*> next_;
      std::atomic<uint32_t> waiting_;
      alignas(cache_line_size()) size_t counter_;
      QNode() : next_(nullptr), waiting_(true), counter_(0) {}
      void reset() { new (this) QNode(); }
    };

    std::atomic<QNode*> tail_;

    static std::atomic<uint32_t> thread_id_allocator_;

    QNode* getMyQNode() {
      static thread_local QNode node;
      return &node;
    }
  };

  using ReTLockQueueAFS = ReTLockQueueImpl<true>;
  using ReTLockQueue = ReTLockQueueImpl<false>;

  template <> std::atomic<uint32_t> ReTLockQueueAFS::thread_id_allocator_(0);
  template <> std::atomic<uint32_t> ReTLockQueue::thread_id_allocator_(0);
}  // namespace retlock
