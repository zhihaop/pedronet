#ifndef PEDRONET_EVENT_LOCK_FREE_QUEUE_H
#define PEDRONET_EVENT_LOCK_FREE_QUEUE_H

#include "pedronet/channel/channel.h"
#include "pedronet/queue/event_queue.h"

#include <concurrentqueue.h>

namespace pedronet {
class EventLockFreeQueue final : public EventQueue {
  EventChannel* channel_;

  std::atomic_size_t size_{};
  moodycamel::ConcurrentQueue<Callback> queue_;
  std::vector<Callback> buf_;

 public:
  explicit EventLockFreeQueue(EventChannel* channel)
      : channel_(channel), buf_(32) {}

  void Add(Callback callback) override {
    while (
        !queue_.enqueue(std::move(callback))) {  // NOLINT: no move if failed.
      std::this_thread::yield();
    }

    if (size_.fetch_add(1) == 0) {
      channel_->WakeUp();
    }
  }

  void Process() override {
    while (true) {
      size_t s = queue_.try_dequeue_bulk(buf_.begin(), buf_.size());
      if (s == 0) {
        break;
      }
      size_.fetch_add(-s);
      for (int i = 0; i < s; ++i) {
        buf_[i]();
      }
    }
  }

  size_t Size() override { return size_; }
};
}  // namespace pedronet
#endif  //PEDRONET_EVENT_LOCK_FREE_QUEUE_H
