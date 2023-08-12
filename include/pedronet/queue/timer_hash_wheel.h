#ifndef PEDRONET_TIMER_HASH_WHEEL_H
#define PEDRONET_TIMER_HASH_WHEEL_H

#include <pedrolib/collection/simple_concurrent_hashmap.h>
#include <pedrolib/collection/static_vector.h>
#include <list>
#include <utility>
#include "pedronet/channel/timer_channel.h"
#include "pedronet/queue/timer_heap_queue.h"
#include "pedronet/queue/timer_queue.h"

namespace pedronet {

class TimerHashWheel : public TimerQueue {

  struct Entry {
    uint64_t rounds{};
    uint64_t id{};
    Duration interval;
    Callback callback;
  };

  struct Bucket {
    std::mutex mu_;
    std::vector<std::weak_ptr<Entry>> entry_;

    std::shared_ptr<Entry> Add(Entry entry) {
      std::lock_guard guard{mu_};
      auto ptr = std::make_shared<Entry>(std::move(entry));
      entry_.emplace_back(ptr);
      return ptr;
    }

    void Add(const std::shared_ptr<Entry>& entry) {
      std::lock_guard guard{mu_};
      entry_.emplace_back(entry);
    }
  };

  void Process(uint64_t ticks) {
    size_t b = ticks % buckets_.size();
    uint64_t rounds = ticks / buckets_.size();

    std::unique_lock lock{buckets_[b].mu_};
    auto& entry = buckets_[b].entry_;
    for (auto it = entry.begin(); it != entry.end();) {
      auto timer = it->lock();
      if (timer == nullptr) {
        it = entry.erase(it);
        continue;
      }

      if (timer->rounds > rounds) {
        ++it;
        continue;
      }

      it = entry.erase(it);
      expired_.emplace(timer);

      if (timer->interval > Duration::Zero()) {
        Timestamp expire = Timestamp::Now() + timer->interval;
        uint64_t expire_ticks = GetTicks(expire);
        timer->rounds = expire_ticks / buckets_.size();
        buckets_[expire_ticks % buckets_.size()].Add(timer);
      }
    }
  }

  [[nodiscard]] uint64_t GetTicks(Timestamp ts) const {
    return ts.usecs / options_.tick_unit.usecs;
  }

 public:
  struct Options {
    Duration tick_unit = Duration::Milliseconds(20);
    size_t buckets = 1 << 10;
  };

  TimerHashWheel(TimerChannel* channel, Options options)
      : channel_(channel),
        buckets_(options.buckets),
        options_(std::move(options)) {
    for (int i = 0; i < options_.buckets; ++i) {
      buckets_.emplace_back();
    }
  }

  explicit TimerHashWheel(TimerChannel* channel)
      : TimerHashWheel(channel, Options{}) {}

  void Init() {
    Timestamp now = Timestamp::Now();
    last_ticks_ = GetTicks(now);
    channel_->WakeUpAfter(options_.tick_unit);
  }

  std::shared_ptr<Entry> Add(uint64_t id, Duration delay, Duration interval,
                             Callback callback) {
    Timestamp expired = Timestamp::Now() + delay;
    uint64_t ticks = GetTicks(expired);
    size_t b = ticks % buckets_.size();

    Entry entry;
    entry.id = id;
    entry.rounds = ticks / buckets_.size();
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
    uint64_t next_ticks = GetTicks(now) + 1;

    while (last_ticks_ != next_ticks) {
      Process(last_ticks_++);
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

    channel_->WakeUpAt(now + options_.tick_unit);
  }

 private:
  TimerChannel* channel_;

  uint64_t last_ticks_{};
  std::atomic_uint64_t counter_{};

  pedrolib::StaticVector<Bucket> buckets_;
  std::queue<std::weak_ptr<Entry>> expired_;
  Options options_;

  pedrolib::SimpleConcurrentHashMap<uint64_t, std::shared_ptr<Entry>> table_;
};
}  // namespace pedronet

#endif  // PEDRONET_TIMER_HASH_WHEEL_H
