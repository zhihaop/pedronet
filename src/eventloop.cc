#include "pedronet/eventloop.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

void EventLoop::Loop() {
  for (;;) {
    int state = state_;
    if (state & kLooping) {
      return;
    }
    int next_state = kLooping | kJoinable;
    if (state_.compare_exchange_strong(state, next_state)) {
      break;
    }
  }

  PEDRONET_TRACE("EventLoop::Loop() running");

  current() = this;

  while (state_ & kLooping) {
    Error err = selector_->Wait(options_.select_timeout);
    if (err != Error::kOk) {
      PEDRONET_ERROR("failed to call selector_.Wait(): {}", err);
      continue;
    }
    Timestamp now = Timestamp::Now();

    size_t n = selector_->Size();
    for (size_t i = 0; i < n; ++i) {
      auto [ch, ev] = selector_->Get(i);
      if (!selector_->Contain(ch)) {
        continue;
      }
      ch->HandleEvents(ev, now);
    }
  }
  current() = nullptr;
}

void EventLoop::Close() {
  state_.fetch_and(~kLooping);

  PEDRONET_TRACE("EventLoop is shutting down.");

  Schedule([this] {
    selector_->Remove(event_channel_);
    selector_->Remove(timer_channel_);
    close_latch_.CountDown();
  });

  PEDRONET_TRACE("EventLoop shutdown.");
}

void EventLoop::Schedule(Callback cb) {
  event_queue_->Add(std::move(cb));
}

void EventLoop::Add(const Channel::Ptr& channel, Callback callback) {
  PEDRONET_TRACE("EventLoopImpl::Add({})", *channel);
  if (current() != this) {
    Schedule([=, callback = std::move(callback)]() mutable {
      Add(channel, std::move(callback));
    });
    return;
  }

  if (!selector_->Contain(channel)) {
    selector_->Add(channel, SelectEvents::kNoneEvent);
    if (callback) {
      callback();
    }
  }
}

void EventLoop::Remove(const Channel::Ptr& channel, Callback callback) {
  if (current() != this) {
    Schedule([=, callback = std::move(callback)]() mutable {
      Remove(channel, std::move(callback));
    });
    return;
  }

  PEDRONET_TRACE("EventLoopImpl::Remove({})", *channel);
  if (selector_->Contain(channel)) {
    selector_->Remove(channel);
    if (callback) {
      callback();
    }
  }
}

EventLoop::EventLoop(const EventLoopOptions& options)
    : options_(options),
      selector_(MakeSelector(options.selector_type)),
      event_channel_(std::make_shared<EventChannel>()),
      timer_channel_(std::make_shared<TimerChannel>()),
      event_queue_(
          MakeEventQueue(options.event_queue_type, event_channel_.get())),
      timer_queue_(
          MakeTimerQueue(options.timer_queue_type, timer_channel_.get())) {

  selector_->Add(event_channel_, SelectEvents::kReadEvent);
  selector_->Add(timer_channel_, SelectEvents::kReadEvent);

  event_channel_->SetEventCallBack([this] { event_queue_->Process(); });
  timer_channel_->SetEventCallBack([this] { timer_queue_->Process(); });

  PEDRONET_TRACE("create event loop");
}

void EventLoop::Join() {
  if ((state_ & kJoinable) == 0) {
    return;
  }

  close_latch_.Await();
}

size_t EventLoop::Size() const noexcept {
  return 1;
}

}  // namespace pedronet