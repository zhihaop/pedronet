#include "pedronet/queue/timer_heap_queue.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

uint64_t TimerHeapQueue::Add(Duration delay, Duration interval,
                             Callback callback) {
  std::unique_lock<std::mutex> lock(mu_);
  Timestamp expired = Timestamp::Now() + delay;
  uint64_t id = ++sequences_;

  auto timer = std::make_shared<TimerStruct>(id, std::move(callback), interval);
  schedule_timer_.emplace(expired, timer);
  timers_.emplace(id, std::move(timer));
  channel_->WakeUpAt(expired);
  return id;
}

void TimerHeapQueue::Cancel(uint64_t timer_id) {
  std::unique_lock lock(mu_);
  timers_.erase(timer_id);
}

void TimerHeapQueue::Process() {
  Timestamp now = Timestamp::Now();
  {
    std::unique_lock lock(mu_);
    while (!schedule_timer_.empty() && schedule_timer_.top().expire <= now) {
      auto timer = std::move(schedule_timer_.top().timer);
      schedule_timer_.pop();
      if (!timer.expired()) {
        expired_timers_.emplace(std::move(timer));
      }
    }
  }

  while (!expired_timers_.empty()) {
    auto timer = expired_timers_.front().lock();
    expired_timers_.pop();

    if (timer == nullptr) {
      continue;
    }
    
    timer->callback();

    std::lock_guard guard(mu_);
    if (timer->interval > Duration::Zero()) {
      schedule_timer_.emplace(now + timer->interval, timer);
      channel_->WakeUpAt(now + timer->interval);
    } else {
      timers_.erase(timer->id);
    }
  }
}

size_t TimerHeapQueue::Size() {
  std::unique_lock lock(mu_);
  return timers_.size();
}
}  // namespace pedronet