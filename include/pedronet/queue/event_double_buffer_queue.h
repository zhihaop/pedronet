#ifndef PEDRONET_EVENT_DOUBLE_BUFFER_QUEUE_H
#define PEDRONET_EVENT_DOUBLE_BUFFER_QUEUE_H
#include "pedronet/channel/event_channel.h"
#include "pedronet/queue/event_queue.h"
namespace pedronet {

class EventDoubleBufferQueue final : public EventQueue {
  EventChannel* channel_;

  std::mutex mu_;
  std::vector<Callback> pending_;
  std::vector<Callback> running_;

 public:
  void Add(Callback callback) override {
    std::unique_lock lock{mu_};

    pending_.emplace_back(std::move(callback));

    // the pending buffer size is empty before insertion.
    if (pending_.size() == 1) {
      channel_->WakeUp();
    }
  }

  void Process() override {
    std::unique_lock lock{mu_};
    running_.swap(pending_);
    lock.unlock();

    for (Callback& callback : running_) {
      callback();
    }

    running_.clear();
  }

  size_t Size() override {
    std::unique_lock lock{mu_};
    return pending_.size();
  }

  explicit EventDoubleBufferQueue(EventChannel* channel) : channel_(channel) {}
};
}  // namespace pedronet

#endif  // PEDRONET_EVENT_DOUBLE_BUFFER_QUEUE_H
