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

    static constexpr uint32_t LOCKED = 1;
    static constexpr uint32_t UNLOCKED = 0;

    void lock() { try_lock(true); }

    void unlock() {
      auto* my_node = getMyQNode();
      assert(my_node->lock_.load() == LOCKED);
      assert(my_node->counter_ > 0);
      my_node->counter_--;
      if (my_node->counter_ > 0) return;

      my_node->lock_.store(UNLOCKED);
      auto* next = my_node->next_.load();
      if (next == nullptr) return;

      next->waiting_.store(false);
    }

    bool try_lock(bool wait = false) {
      auto* my_node = getMyQNode();

      if (my_node->lock_.load() == LOCKED) {
        my_node->counter_++;
        return true;
      }

      my_node->lock_.store(LOCKED);
      my_node->counter_ = 1;
      my_node->next_.store(nullptr);
      my_node->waiting_.store(true);

    lock:
      auto* current = tail_.load();

      // queue is empty
      if (current == nullptr) {
        auto success = (tail_.compare_exchange_strong(current, my_node));
        if (success) {
          return true;
        } else {
          if (!wait) {
            my_node = new (&my_node) QNode();
            return false;
          }
          goto lock;
        }
      }
      if (!wait) {
        my_node = new (&my_node) QNode();
        return false;
      }

      // enqueue
      bool queued = false;
      while (!queued) {
        auto* next = current->next_.load();
        while (next != nullptr) {
          current = next;
          next = current->next_.load();
        }
        assert(current != nullptr);
        assert(next == nullptr);

        queued = current->next_.compare_exchange_weak(next, my_node);
      }

      // wait for unlock
      assert(wait);
      size_t i = 0;
      auto* my_metric = getMyMetric();
      const size_t adaptive = *my_metric;  // TODO
      for (;;) {
        auto waiting = my_node->waiting_.load();
        if (!waiting) {
          if constexpr (AdaptiveSleep) {
            uint32_t new_metric = (*my_metric + i / 2);
            *getMyMetric() = new_metric;
          }
          return true;
        }
        if (i % 10 == 0) std::this_thread::yield();
        if (i % 100 == 0) {
          if constexpr (AdaptiveSleep) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1 + (i / 100) * adaptive));
          } else {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1 + i / 100));
          }
        }
        i++;
      }
    }

  private:
    static constexpr std::size_t cache_line_size() { return 64; }

    struct QNode {
      alignas(cache_line_size()) std::atomic<QNode*> next_;
      std::atomic<bool> lock_;
      std::atomic<bool> waiting_;
      alignas(cache_line_size()) size_t counter_;
      QNode() : next_(nullptr), lock_(UNLOCKED), waiting_(false), counter_(0) {}
    };

    std::atomic<QNode*> tail_;

    static std::atomic<uint32_t> thread_id_allocator_;

    QNode* getMyQNode() {
      static thread_local QNode node;
      return &node;
    }
    size_t* getMyMetric() {
      static thread_local size_t metric = 1;
      return &metric;
    }
  };

  using ReTLockQueueAFS = ReTLockQueueImpl<false>;
  using ReTLockQueue = ReTLockQueueImpl<true>;

  template <> std::atomic<uint32_t> ReTLockQueueAFS::thread_id_allocator_(0);
  template <> std::atomic<uint32_t> ReTLockQueue::thread_id_allocator_(0);
}  // namespace retlock
