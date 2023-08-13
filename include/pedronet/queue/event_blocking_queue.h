#ifndef PEDRONET_EVENT_BLOCKING_QUEUE_H
#define PEDRONET_EVENT_BLOCKING_QUEUE_H

#include <deque>
#include "pedronet/channel/event_channel.h"
#include "pedronet/queue/event_queue.h"

namespace pedronet {

class EventBlockingQueue final : public EventQueue {
  EventChannel* channel_{};

  std::mutex mu_;
  std::deque<Callback> queue_;

  bool Pop(Callback& callback) {
    std::unique_lock lock{mu_};
    if (queue_.empty()) {
      return false;
    }

    callback = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

 public:
  explicit EventBlockingQueue(EventChannel* channel) : channel_(channel) {}

  void Add(Callback callback) override {
    std::unique_lock lock{mu_};
    queue_.emplace_back(std::move(callback));

    // the previous deque is empty.
    if (queue_.size() == 1) {
      channel_->WakeUp();
    }
  }

  void Process() override {
    Callback callback;
    while (Pop(callback)) {
      callback();
    }
  }

  size_t Size() override { 
    std::unique_lock lock{mu_};
    return queue_.size();
  }
};
}  // namespace pedronet

#endif  // PEDRONET_EVENT_BLOCKING_QUEUE_H
