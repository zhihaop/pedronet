#include "pedronet/eventloop.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

void EventLoop::Loop() {
  PEDRONET_TRACE("EventLoop::Loop() running");

  auto& current = core::Thread::Current();
  current.BindEventLoop(this);

  std::vector<SelectChannel> ready;

  while (state_ & kLooping) {
    Error err = selector_->Wait(options_.select_timeout);
    if (err != Error::kOk) {
      PEDRONET_ERROR("failed to call selector_.Wait(): {}", err);
      continue;
    }
    Timestamp now = Timestamp::Now();

    size_t n = selector_->Size();
    ready.clear();
    ready.reserve(n);

    for (size_t i = 0; i < n; ++i) {
      auto [ch, ev] = selector_->Get(i);
      if (!selector_->Contain(ch)) {
        continue;
      }
      ready.emplace_back(ch, ev);
    }

    for (auto& [ch, ev] : ready) {
      if (!selector_->Contain(ch)) {
        continue;
      }
      ch->HandleEvents(ev, now);
    }
  }

  current.UnbindEventLoop(this);
}

void EventLoop::Close() {
  state_.fetch_and(~kLooping);

  PEDRONET_TRACE("EventLoop is shutting down.");
  event_channel_.WakeUp();
  // TODO: await shutdown ?
}

void EventLoop::Schedule(Callback cb) {
  event_queue_->Add(std::move(cb));
}

void EventLoop::AssertUnderLoop() const {
  if (!CheckUnderLoop()) {
    PEDRONET_FATAL("check in event loop failed");
  }
}

void EventLoop::Register(Channel* channel, Callback register_callback,
                         Callback deregister_callback) {
  PEDRONET_INFO("EventLoopImpl::Register({})", *channel);
  if (!CheckUnderLoop()) {
    Schedule([this, channel, r = std::move(register_callback),
              d = std::move(deregister_callback)]() mutable {
      Register(channel, std::move(r), std::move(d));
    });
    return;
  }
  auto it = channels_.find(channel);
  if (it == channels_.end()) {
    selector_->Add(channel, SelectEvents::kNoneEvent);
    channels_.emplace_hint(it, channel, std::move(deregister_callback));
    if (register_callback) {
      register_callback();
    }
  }
}
void EventLoop::Deregister(Channel* channel) {
  if (!CheckUnderLoop()) {
    Schedule([=] { Deregister(channel); });
    return;
  }

  PEDRONET_INFO("EventLoopImpl::Deregister({})", *channel);
  auto it = channels_.find(channel);
  if (it == channels_.end()) {
    return;
  }

  auto callback = std::move(it->second);
  selector_->Remove(channel);
  channels_.erase(it);

  if (callback) {
    callback();
  }
}

EventLoop::EventLoop(const EventLoopOptions& options)
    : options_(options),
      selector_(MakeSelector(options.selector_type)),
      event_queue_(MakeEventQueue(options.event_queue_type, &event_channel_)),
      timer_queue_(MakeTimerQueue(options.timer_queue_type, &timer_channel_)) {
  selector_->Add(&event_channel_, SelectEvents::kReadEvent);
  selector_->Add(&timer_channel_, SelectEvents::kReadEvent);

  event_channel_.SetEventCallBack([this] { event_queue_->Process(); });
  timer_channel_.SetEventCallBack([this] { timer_queue_->Process(); });
  event_channel_.SetPriority(-1);
  timer_channel_.SetPriority(1);

  PEDRONET_TRACE("create event loop");
}

void EventLoop::Join() {
  // TODO check joinable.
  close_latch_.Await();
  PEDRONET_INFO("Eventloop join exit");
}

size_t EventLoop::Size() const noexcept {
  return 1;
}

}  // namespace pedronet