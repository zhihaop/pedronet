#ifndef PEDRONET_EVENTLOOP_H
#define PEDRONET_EVENTLOOP_H

#include <concurrentqueue.h>
#include <pedrolib/concurrent/latch.h>
#include <atomic>
#include "pedrolib/executor/executor.h"
#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/channel/event_channel.h"
#include "pedronet/channel/timer_channel.h"
#include "pedronet/event.h"
#include "pedronet/queue/event_queue_factory.h"
#include "pedronet/queue/timer_queue_factory.h"
#include "pedronet/selector/selector_factory.h"

namespace pedronet {

class EventLoop : public Executor {
  enum State {
    kLooping = 1 << 0,
    kJoinable = 1 << 1,
  };

  static EventLoop*& current() noexcept;

  void join();

 public:
  static EventLoop* GetEventLoop() noexcept { return current(); }

  explicit EventLoop(const EventLoopOptions& options);

  Selector* GetSelector() noexcept { return selector_.get(); }

  void Remove(const Channel::Ptr& channel, Callback callback);

  void Add(const Channel::Ptr& channel, Callback callback);

  [[nodiscard]] size_t Size() const noexcept override;

  void Schedule(Callback cb) override;

  uint64_t ScheduleAfter(Duration delay, Callback cb) override {
    return timer_queue_->Add(delay, Duration::Zero(), std::move(cb));
  }

  uint64_t ScheduleEvery(Duration delay, Duration interval,
                         Callback cb) override {
    return timer_queue_->Add(delay, interval, std::move(cb));
  }

  void ScheduleCancel(uint64_t id) override { timer_queue_->Cancel(id); }

  template <typename Runnable>
  void Run(Runnable&& runnable) {
    if (current() == this) {
      runnable();
      return;
    }
    Schedule(std::forward<Runnable>(runnable));
  }

  void Close() override;

  void Loop();

  ~EventLoop() override { join(); }

  void Join() override;

 private:
  EventLoopOptions options_;
  EventChannel::Ptr event_channel_;
  TimerChannel::Ptr timer_channel_;
  std::unique_ptr<Selector> selector_;
  std::unique_ptr<EventQueue> event_queue_;
  std::unique_ptr<TimerQueue> timer_queue_;

  std::atomic_int32_t state_{};
  Latch close_latch_{1};
};

}  // namespace pedronet

#endif  // PEDRONET_EVENTLOOP_H