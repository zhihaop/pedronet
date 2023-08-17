#ifndef PEDRONET_TIMER_HASH_WHEEL_H
#define PEDRONET_TIMER_HASH_WHEEL_H

#include <pedrolib/collection/static_vector.h>
#include <list>
#include <map>

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

  struct Timeout {
    uint64_t rounds{};
    std::weak_ptr<Entry> entry;

    bool operator<(const Timeout& other) const noexcept {
      return rounds > other.rounds;
    }
  };

  struct Bucket {
    std::mutex mu_;
    std::priority_queue<Timeout> queue_;

    std::shared_ptr<Entry> Add(Entry entry) {
      std::lock_guard guard{mu_};
      auto rounds = entry.rounds;
      auto ptr = std::make_shared<Entry>(std::move(entry));
      queue_.emplace(Timeout{rounds, ptr});
      return ptr;
    }

    void Add(const std::shared_ptr<Entry>& entry) {
      std::lock_guard guard{mu_};
      queue_.emplace(Timeout{entry->rounds, entry});
    }

    bool Pop(uint64_t current_rounds, std::weak_ptr<Entry>& entry) {
      std::unique_lock lock{mu_};

      while (true) {
        if (queue_.empty()) {
          return false;
        }

        auto& top = queue_.top();
        if (top.rounds > current_rounds) {
          return false;
        }

        if (top.entry.expired()) {
          queue_.pop();
          continue;
        }

        entry = top.entry;
        queue_.pop();
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
    Duration tick_unit = Duration::Milliseconds(100);
    size_t buckets = 600;
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

    Entry entry;
    entry.id = id;
    entry.rounds = GetRounds(expired);
    entry.interval = interval;
    entry.callback = std::move(callback);

    return buckets_[b].Add(std::move(entry));
  }

  uint64_t Add(Duration delay, Duration interval, Callback callback) override {
    std::lock_guard guard{mu_};
    uint64_t id = counter_.fetch_add(1, std::memory_order_relaxed) + 1;
    table_[id] = Add(id, delay, interval, std::move(callback));
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

    auto ProcessExpired = [&](const std::weak_ptr<Entry>& entry) {
      auto timer = entry.lock();
      if (timer == nullptr) {
        return;
      }

      if (timer->interval <= Duration::Zero()) {
        Cancel(timer->id);
      } else {
        Timestamp expire = now + timer->interval;
        uint64_t expire_ticks = GetTicks(expire);
        timer->rounds = GetRounds(expire);
        buckets_[expire_ticks % buckets_.size()].Add(timer);
      }

      timer->callback();
    };

    if (next_ticks - last_ticks < buckets_.size()) {
      for (size_t i = last_ticks; i != next_ticks; ++i) {
        auto& bucket = buckets_[i % buckets_.size()];
        std::weak_ptr<Entry> entry;
        while (bucket.Pop(rounds, entry)) {
          ProcessExpired(entry);
        }
      }
    } else {
      for (auto& bucket : buckets_) {
        std::weak_ptr<Entry> entry;
        while (bucket.Pop(rounds, entry)) {
          ProcessExpired(entry);
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

  std::mutex mu_;
  std::unordered_map<uint64_t, std::shared_ptr<Entry>> table_;
};
}  // namespace pedronet

#endif  // PEDRONET_TIMER_HASH_WHEEL_H
