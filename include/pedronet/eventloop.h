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
#include "pedronet/core/thread.h"
#include "pedronet/event.h"
#include "pedronet/queue/event_queue_factory.h"
#include "pedronet/queue/timer_queue_factory.h"
#include "pedronet/selector/selector_factory.h"

namespace pedronet {

class EventLoop : public Executor {
  enum State {
    kLooping = 1 << 0,
  };

 public:
  explicit EventLoop(const EventLoopOptions& options);

  Selector* GetSelector() noexcept { return selector_.get(); }

  void Deregister(Channel* channel);

  void Register(Channel* channel, Callback register_callback,
                Callback deregister_callback);

  bool CheckUnderLoop() const noexcept {
    return core::Thread::Current().CheckUnderLoop(this);
  }

  size_t Size() const noexcept override;

  void AssertUnderLoop() const;

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
    if (CheckUnderLoop()) {
      runnable();
      return;
    }
    Schedule(std::forward<Runnable>(runnable));
  }

  bool Closed() const noexcept { return (state_ & kLooping) == 0; }

  void Close() override;

  void Loop();

  // TODO join before exit.
  ~EventLoop() override = default;

  void Join() override;

 private:
  EventLoopOptions options_;
  EventChannel event_channel_;
  TimerChannel timer_channel_;
  std::unique_ptr<Selector> selector_;
  std::unique_ptr<EventQueue> event_queue_;
  std::unique_ptr<TimerQueue> timer_queue_;

  std::atomic_int32_t state_{kLooping};
  std::unordered_map<Channel*, Callback> channels_;

  Latch close_latch_{1};
};

}  // namespace pedronet

#endif  // PEDRONET_EVENTLOOP_H