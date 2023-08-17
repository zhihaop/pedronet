#ifndef PEDRONET_QUEUE_TIMER_QUEUE
#define PEDRONET_QUEUE_TIMER_QUEUE

#include <mutex>
#include <queue>
#include "pedrolib/executor/executor.h"
#include "pedronet/channel/timer_channel.h"
#include "pedronet/queue/timer_queue.h"
#include "pedronet/core/spinlock.h"

namespace pedronet {

class TimerHeapQueue final : public TimerQueue {

  struct Entry {
    uint64_t id;
    Callback callback;
    Duration interval;

    Entry(uint64_t id, Callback callback, const Duration& interval)
        : id(id), callback(std::move(callback)), interval(interval) {}
  };

  struct Timeout {
    Timestamp expire;
    mutable std::weak_ptr<Entry> timer;

    Timeout(Timestamp expire, const std::weak_ptr<Entry>& timer)
        : expire(expire), timer(timer) {}

    bool operator<(const Timeout& other) const noexcept {
      return expire > other.expire;
    }
  };

 public:
  explicit TimerHeapQueue(TimerChannel* channel) : channel_(channel) {}

  uint64_t Add(Duration delay, Duration interval, Callback callback) override;

  void Process() override;

  void Cancel(uint64_t timer_id) override;

 private:
  TimerChannel* channel_;
  std::atomic_uint64_t counter_{};
  std::queue<std::shared_ptr<const Entry>> expires_;

  SpinLock mu_;
  std::priority_queue<Timeout> queue_;
  std::unordered_map<uint64_t, std::shared_ptr<const Entry>> table_;
};
}  // namespace pedronet
#endif  // PEDRONET_TIMER_QUEUE