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

 public:
  explicit EventLockFreeQueue(EventChannel* channel) : channel_(channel) {}

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
    Callback callback;
    size_t count = 0;
    while (queue_.try_dequeue(callback)) {
      count++;
      callback();
    }

    size_.fetch_add(-count);
  }

  size_t Size() override { return size_; }
};
}  // namespace pedronet
#endif  //PEDRONET_EVENT_LOCK_FREE_QUEUE_H
