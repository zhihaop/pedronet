#include "pedronet/queue/timer_heap_queue.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

uint64_t TimerHeapQueue::Add(Duration delay, Duration interval,
                             Callback callback) {
  Timestamp expired = Timestamp::Now() + delay;
  uint64_t id = counter_.fetch_add(1, std::memory_order_relaxed) + 1;
  auto timer = std::make_shared<Entry>(id, std::move(callback), interval);

  std::unique_lock<std::mutex> lock(mu_);
  queue_.emplace(expired, timer);
  table_.emplace(id, std::move(timer));
  channel_->WakeUpAt(expired);
  return id;
}

void TimerHeapQueue::Cancel(uint64_t timer_id) {
  std::unique_lock lock(mu_);
  table_.erase(timer_id);
}

void TimerHeapQueue::Process() {
  Timestamp now = Timestamp::Now();

  std::unique_lock lock(mu_);
  while (!queue_.empty() && queue_.top().expire <= now) {
    auto item = queue_.top();
    queue_.pop();

    auto timer = item.timer.lock();
    if (timer == nullptr) {
      continue;
    }

    lock.unlock();
    timer->callback();
    lock.lock();

    if (timer->interval > Duration::Zero()) {
      Timestamp expire = Timestamp::Now() + timer->interval;
      queue_.emplace(expire, timer);
      channel_->WakeUpAfter(timer->interval);
    } else {
      table_.erase(timer->id);
    }
  }
}
}  // namespace pedronet