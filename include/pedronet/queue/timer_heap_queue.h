#ifndef PEDRONET_QUEUE_TIMER_QUEUE
#define PEDRONET_QUEUE_TIMER_QUEUE

#include <mutex>
#include <queue>
#include "pedrolib/executor/executor.h"
#include "pedronet/channel/timer_channel.h"
#include "pedronet/queue/timer_queue.h"

namespace pedronet {

class TimerHeapQueue final : public TimerQueue {

  struct TimerStruct : pedrolib::noncopyable, pedrolib::nonmovable {
    uint64_t id;
    Callback callback;
    Duration interval;

    TimerStruct(uint64_t id, Callback callback, const Duration& interval)
        : id(id), callback(std::move(callback)), interval(interval) {}
  };

  struct TimerOrder {
    Timestamp expire;
    mutable std::weak_ptr<TimerStruct> timer;

    TimerOrder(Timestamp expire, const std::weak_ptr<TimerStruct>& timer)
        : expire(expire), timer(timer) {}

    bool operator<(const TimerOrder& other) const noexcept {
      return expire > other.expire;
    }
  };

 public:
  explicit TimerHeapQueue(TimerChannel* channel) : channel_(channel) {}

  uint64_t Add(Duration delay, Duration interval, Callback callback) override;

  void Process() override;

  void Cancel(uint64_t timer_id) override;

  size_t Size() override;

 private:
  TimerChannel* channel_;
  std::priority_queue<TimerOrder> schedule_timer_;
  std::queue<std::weak_ptr<TimerStruct>> expired_timers_;
  std::queue<std::weak_ptr<TimerStruct>> pending_timers_;

  std::mutex mu_;
  uint64_t sequences_{};
  std::unordered_map<uint64_t, std::shared_ptr<TimerStruct>> timers_;
};
}  // namespace pedronet
#endif  // PEDRONET_TIMER_QUEUE