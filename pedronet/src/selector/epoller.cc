#include "pedronet/selector/epoller.h"
#include "pedronet/logger/logger.h"
#include <memory>
#include <sys/epoll.h>

namespace pedronet {

inline static core::File CreateEpollFile() {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd <= 0) {
    PEDRONET_FATAL("failed to create epoll fd, errno[{}]", errno);
  }
  return core::File{fd};
}

EpollSelector::EpollSelector() : core::File(CreateEpollFile()), buf_(8192) {}

EpollSelector::~EpollSelector() = default;

void EpollSelector::internalUpdate(Channel *channel, int op,
                                   SelectEvents events) {
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;

  int fd = channel->File().Descriptor();
  if (::epoll_ctl(fd_, op, fd, op == EPOLL_CTL_DEL ? nullptr : &ev) < 0) {
    PEDRONET_FATAL("failed to call epoll_ctl, reason[{}]", errno);
  }
}

void EpollSelector::Add(Channel *channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_ADD, events);
}

void EpollSelector::Update(Channel *channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_MOD, events);
}

void EpollSelector::Remove(Channel *channel) {
  internalUpdate(channel, EPOLL_CTL_DEL, SelectEvents::kNoneEvent);
}

Selector::Error EpollSelector::Wait(Duration timeout,
                                    SelectChannels *selected) {
  int n = ::epoll_wait(fd_, buf_.data(), buf_.size(), timeout.Milliseconds());
  selected->now = Timestamp::Now();
  selected->channels.clear();
  selected->events.clear();

  if (n == 0) {
    return Selector::Error::Success();
  }

  if (n < 0) {
    return Selector::Error{errno};
  }

  selected->channels.resize(n);
  selected->events.resize(n);
  for (int i = 0; i < n; ++i) {
    selected->channels[i] = static_cast<Channel *>(buf_[i].data.ptr);
    selected->events[i] = ReceiveEvents{buf_[i].events};
  }
  return Selector::Error::Success();
}
void EpollSelector::SetBufferSize(size_t size) { buf_.resize(size); }
} // namespace pedronet