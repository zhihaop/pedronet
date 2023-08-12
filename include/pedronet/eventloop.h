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
#include "pedronet/queue/event_blocking_queue.h"
#include "pedronet/queue/event_double_buffer_queue.h"
#include "pedronet/queue/event_lock_free_queue.h"
#include "pedronet/queue/event_queue.h"
#include "pedronet/queue/timer_hash_wheel.h"
#include "pedronet/queue/timer_heap_queue.h"
#include "pedronet/selector/selector.h"

namespace pedronet {



class EventLoop : public Executor {
  inline const static Duration kSelectTimeout{std::chrono::seconds(10)};

  enum State {
    kLooping = 1 << 0,
  };

  std::unique_ptr<Selector> selector_;
  EventChannel event_channel_;
  TimerChannel timer_channel_;
  EventLockFreeQueue event_queue_;
  TimerHashWheel timer_queue_;

  std::atomic_int32_t state_{kLooping};
  std::unordered_map<Channel*, Callback> channels_;

  pedrolib::Latch close_latch_{1};

 public:
  explicit EventLoop(std::unique_ptr<Selector> selector);

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
    return timer_queue_.Add(delay, Duration::Zero(), std::move(cb));
  }

  uint64_t ScheduleEvery(Duration delay, Duration interval,
                         Callback cb) override {
    return timer_queue_.Add(delay, interval, std::move(cb));
  }

  void ScheduleCancel(uint64_t id) override { timer_queue_.Cancel(id); }

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
};

}  // namespace pedronet

#endif  // PEDRONET_EVENTLOOP_H