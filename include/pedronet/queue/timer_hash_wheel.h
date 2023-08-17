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
    SpinLock mu_{};
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
    return ts.usecs / options_.tick_unit.usecs;
  }

  [[nodiscard]] uint64_t GetRounds(Timestamp ts) const {
    return ts.usecs / options_.tick_unit.usecs / options_.buckets;
  }

  [[nodiscard]] Timestamp GetWakeUp(Timestamp expire) const {
    expire.usecs =
        expire.usecs / options_.tick_unit.usecs * options_.tick_unit.usecs;
    return expire;
  }

 public:
  struct Options {
    Duration tick_unit = Duration::Milliseconds(10);
    size_t buckets = 6000;
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

  std::shared_ptr<Entry> Add(uint64_t id, const Duration& delay,
                             const Duration& interval, Callback callback) {
    Timestamp expired = Timestamp::Now() + delay;
    uint64_t ticks = GetTicks(expired);
    size_t b = ticks % buckets_.size();

    auto entry = std::make_shared<Entry>();
    entry->id = id;
    entry->rounds = GetRounds(expired);
    entry->interval = interval;
    entry->callback = std::move(callback);

    buckets_[b].Add(entry->rounds, entry->id);
    return entry;
  }

  uint64_t Add(Duration delay, Duration interval, Callback callback) override {
    uint64_t id = counter_.fetch_add(1, std::memory_order_relaxed) + 1;
    auto ptr = Add(id, delay, interval, std::move(callback));

    std::lock_guard guard{mu_};
    table_[id] = ptr;
    return id;
  }

  void Cancel(uint64_t id) override {
    std::lock_guard guard{mu_};
    table_.erase(id);
  }

  void Process() override {
    Timestamp now = Timestamp::Now();
    uint64_t last_ticks = GetTicks(last_);
    uint64_t next_ticks = GetTicks(now) + 1;
    uint64_t rounds = GetRounds(now);
    last_ = now;

    auto ProcessExpired = [&](uint64_t id) {
      std::unique_lock lock{mu_};
      auto it = table_.find(id);
      if (it == table_.end()) {
        return;
      }

      auto timer = it->second;
      if (timer->interval <= Duration::Zero()) {
        table_.erase(it);
      } else {
        Timestamp expire = now + timer->interval;
        uint64_t expire_ticks = GetTicks(expire);
        timer->rounds = GetRounds(expire);
        buckets_[expire_ticks % buckets_.size()].Add(timer->rounds, timer->id);
      }
      lock.unlock();

      timer->callback();
    };

    std::vector<uint64_t> entries;
    if (next_ticks - last_ticks < buckets_.size()) {
      for (size_t i = last_ticks; i != next_ticks; ++i) {
        auto& bucket = buckets_[i % buckets_.size()];
        entries.clear();
        while (bucket.Pop(rounds, entries)) {
          for (auto& entry : entries) {
            ProcessExpired(entry);
          }
        }
      }
    } else {
      for (auto& bucket : buckets_) {
        entries.clear();
        while (bucket.Pop(rounds, entries)) {
          for (auto& entry : entries) {
            ProcessExpired(entry);
          }
        }
      }
    }

    channel_->WakeUpAt(GetWakeUp(now + options_.tick_unit));
  }

 private:
  TimerChannel* channel_;

  Timestamp last_;
  std::atomic_uint64_t counter_{};

  pedrolib::StaticVector<Bucket> buckets_;
  Options options_;

  SpinLock mu_;
  std::unordered_map<uint64_t, std::shared_ptr<Entry>> table_;
};
}  // namespace pedronet

#endif  // PEDRONET_TIMER_HASH_WHEEL_H
