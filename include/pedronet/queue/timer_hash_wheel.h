#ifndef PEDRONET_TIMER_HASH_WHEEL_H
#define PEDRONET_TIMER_HASH_WHEEL_H

#include <pedrolib/collection/simple_concurrent_hashmap.h>
#include <pedrolib/collection/static_vector.h>
#include <map>

#include "pedronet/channel/timer_channel.h"
#include "pedronet/queue/timer_heap_queue.h"
#include "pedronet/queue/timer_queue.h"

namespace pedronet {

class TimerHashWheel : public TimerQueue {

  struct Entry {
    Timestamp expired;
    uint64_t id{};
    Duration interval;
    Callback callback;
  };

  struct Timeout {
    Timestamp expire;
    std::weak_ptr<Entry> entry;

    bool operator<(const Timeout& other) const noexcept {
      return expire > other.expire;
    }
  };

  struct Bucket {
    std::mutex mu_;
    std::priority_queue<Timeout> queue_;

    std::shared_ptr<Entry> Add(Entry entry) {
      std::lock_guard guard{mu_};
      auto expired = entry.expired;
      auto ptr = std::make_shared<Entry>(std::move(entry));
      queue_.emplace(Timeout{expired, ptr});
      return ptr;
    }

    void Add(const std::shared_ptr<Entry>& entry) {
      std::lock_guard guard{mu_};
      queue_.emplace(Timeout{entry->expired, entry});
    }
  };

  void Process(Timestamp now, uint64_t ticks) {
    size_t b = ticks % buckets_.size();

    std::unique_lock lock{buckets_[b].mu_};
    auto& queue = buckets_[b].queue_;
    
    while (!queue.empty() && queue.top().expire <= now) {
      auto timer = queue.top().entry.lock();
      queue.pop();
      
      if (timer == nullptr) {
        continue;
      }
      
      expired_.emplace(timer);

      if (timer->interval > Duration::Zero()) {
        Timestamp expire = timer->expired + timer->interval;
        uint64_t expire_ticks = GetTicks(expire);
        timer->expired = expire;
        buckets_[expire_ticks % buckets_.size()].Add(timer);
      }
    }
  }

  [[nodiscard]] uint64_t GetTicks(Timestamp ts) const {
    return ts.usecs / options_.tick_unit.usecs;
  }

  [[nodiscard]] Timestamp GetWakeUp(Timestamp expire) const {
    expire.usecs =
        expire.usecs / options_.tick_unit.usecs * options_.tick_unit.usecs;
    return expire;
  }

 public:
  struct Options {
    Duration tick_unit = Duration::Milliseconds(20);
    size_t buckets = 1 << 10;
  };

  TimerHashWheel(TimerChannel* channel, Options options)
      : channel_(channel),
        last_(Timestamp::Now()),
        buckets_(options.buckets),
        options_(std::move(options)) {
    for (int i = 0; i < options_.buckets; ++i) {
      buckets_.emplace_back();
    }
    channel_->WakeUpAt(last_);
  }

  explicit TimerHashWheel(TimerChannel* channel)
      : TimerHashWheel(channel, Options{}) {}

  std::shared_ptr<Entry> Add(uint64_t id, Duration delay, Duration interval,
                             Callback callback) {
    Timestamp expired = Timestamp::Now() + delay;
    uint64_t ticks = GetTicks(expired);
    size_t b = ticks % buckets_.size();

    Entry entry;
    entry.id = id;
    entry.expired = expired;
    entry.interval = interval;
    entry.callback = std::move(callback);

    return buckets_[b].Add(std::move(entry));
  }

  uint64_t Add(Duration delay, Duration interval, Callback callback) override {
    uint64_t id = counter_.fetch_add(1, std::memory_order_relaxed) + 1;
    table_.insert(id, Add(id, delay, interval, std::move(callback)));
    return id;
  }

  void Cancel(uint64_t id) override {
    std::shared_ptr<Entry> entry;
    table_.erase(id, entry);
  }

  void Process() override {
    Timestamp now = Timestamp::Now();
    uint64_t last_ticks = GetTicks(last_);
    uint64_t next_ticks = GetTicks(now) + 1;
    last_ = now;

    while (last_ticks != next_ticks) {
      Process(now, last_ticks++);
    }

    while (!expired_.empty()) {
      auto front = expired_.front().lock();
      expired_.pop();

      if (front == nullptr) {
        continue;
      }

      if (front->interval <= Duration::Zero()) {
        Cancel(front->id);
      }

      front->callback();
    }

    channel_->WakeUpAt(GetWakeUp(now + options_.tick_unit));
  }

 private:
  TimerChannel* channel_;

  Timestamp last_;
  std::atomic_uint64_t counter_{};

  pedrolib::StaticVector<Bucket> buckets_;
  std::queue<std::weak_ptr<Entry>> expired_;
  Options options_;

  pedrolib::SimpleConcurrentHashMap<uint64_t, std::shared_ptr<Entry>> table_;
};
}  // namespace pedronet

#endif  // PEDRONET_TIMER_HASH_WHEEL_H
