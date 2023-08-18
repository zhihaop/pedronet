#ifndef PEDRONET_TIMER_HASH_WHEEL_H
#define PEDRONET_TIMER_HASH_WHEEL_H

#include <concurrentqueue.h>
#include <pedrolib/collection/static_vector.h>
#include <map>

#include "pedronet/channel/timer_channel.h"
#include "pedronet/core/spinlock.h"
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

  class Bucket {
    mutable SpinLock mu_{};
    std::map<uint64_t, std::vector<uint64_t>> queue_;

   public:
    void Add(uint64_t rounds, uint64_t id) {
      std::lock_guard guard{mu_};
      queue_[rounds].emplace_back(id);
    }

    bool Pop(uint64_t rounds, std::vector<uint64_t>& entry) {
      std::unique_lock lock{mu_};

      while (true) {
        auto it = queue_.begin();
        if (it == queue_.end()) {
          return false;
        }

        if (it->first > rounds) {
          return false;
        }

        entry = std::move(it->second);
        queue_.erase(it);
        return true;
      }
    }
  };

  [[nodiscard]] uint64_t GetTicks(Timestamp ts) const {
    return ts.usecs / tick_.usecs;
  }

  [[nodiscard]] uint64_t GetRounds(Timestamp ts) const {
    return ts.usecs / tick_.usecs / buckets_.size();
  }

  std::shared_ptr<Entry> GetEntry(uint64_t id) {
    std::lock_guard guard{mu_};
    auto it = table_.find(id);
    if (it == table_.end()) {
      return nullptr;
    }
    return it->second;
  }

  void SetEntry(uint64_t id, std::shared_ptr<Entry> entry) {
    std::lock_guard guard{mu_};
    table_[id] = std::move(entry);
  }

  void ProcessBuckets(Timestamp now, size_t source_bucket,
                      size_t target_bucket) {
    uint64_t rounds = GetRounds(now);
    std::vector<uint64_t> entries;

    for (size_t i = source_bucket; i <= target_bucket; ++i) {
      auto& bucket = buckets_[i];
      entries.clear();
      while (bucket.Pop(rounds, entries)) {
        for (auto& id : entries) {
          auto timer = GetEntry(id);
          if (timer == nullptr) {
            continue;
          }

          if (timer->interval <= Duration::Zero()) {
            Cancel(id);
          } else {
            Timestamp expire = now + timer->interval;
            uint64_t ticks = GetTicks(expire);
            timer->rounds = GetRounds(expire);
            buckets_[ticks % buckets_.size()].Add(timer->rounds, timer->id);
          }

          timer->callback();
        }
      }
    }
  }

  void WakeUp(Timestamp expire) {
    expire.usecs = expire.usecs / tick_.usecs * tick_.usecs;
    channel_->WakeUpAt(expire);
  }

 public:
  struct Options {
    Duration tick = Duration::Milliseconds(100);
    size_t buckets = 600;
  };

  TimerHashWheel(TimerChannel* channel, const Options& options)
      : tick_(options.tick),
        channel_(channel),
        last_(Timestamp::Now()),
        buckets_(options.buckets) {
    for (int i = 0; i < buckets_.capacity(); ++i) {
      buckets_.emplace_back();
    }
    channel_->WakeUpAt(last_);
  }

  explicit TimerHashWheel(TimerChannel* channel)
      : TimerHashWheel(channel, Options{}) {}

  uint64_t Add(Duration delay, Duration interval, Callback callback) override {
    uint64_t id = counter_.fetch_add(1, std::memory_order_relaxed) + 1;

    Timestamp expired = Timestamp::Now() + delay;
    uint64_t ticks = GetTicks(expired);
    uint64_t rounds = GetRounds(expired);
    size_t b = ticks % buckets_.size();

    auto entry = std::make_shared<Entry>();
    entry->id = id;
    entry->rounds = rounds;
    entry->interval = interval;
    entry->callback = std::move(callback);

    SetEntry(id, std::move(entry));
    buckets_[b].Add(rounds, id);

    WakeUp(expired);
    return id;
  }

  void Cancel(uint64_t id) override {
    std::lock_guard guard{mu_};
    table_.erase(id);
  }

  void Process() override {
    Timestamp now = Timestamp::Now();
    uint64_t t1 = GetTicks(last_);
    uint64_t t2 = GetTicks(now);

    if (t2 - t1 < buckets_.size()) {
      size_t b1 = t1 % buckets_.size();
      size_t b2 = t2 % buckets_.size();
      if (b1 <= b2) {
        ProcessBuckets(now, b1, b2);
      } else {
        ProcessBuckets(now, b1, buckets_.size() - 1);
        ProcessBuckets(now, 0, b2);
      }
    } else {
      ProcessBuckets(now, 0, buckets_.size() - 1);
    }

    last_ = now;
    WakeUp(now + tick_);
  }

 private:
  const Duration tick_;
  TimerChannel* channel_;

  Timestamp last_{};
  std::atomic_uint64_t counter_{};

  pedrolib::StaticVector<Bucket> buckets_;

  SpinLock mu_;
  std::unordered_map<uint64_t, std::shared_ptr<Entry>> table_;
};
}  // namespace pedronet

#endif  // PEDRONET_TIMER_HASH_WHEEL_H
