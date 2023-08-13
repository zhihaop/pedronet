#include "pedronet/selector/epoller.h"
#include <sys/epoll.h>
#include "pedronet/logger/logger.h"

namespace pedronet {

inline static File CreateEpollFile() {
  int fd = ::epoll_create1(EPOLL_CLOEXEC);
  if (fd <= 0) {
    PEDRONET_FATAL("failed to create epoll fd, errno[{}]", errno);
  }
  return File{fd};
}

EpollSelector::EpollSelector() : fd_(CreateEpollFile()), buf_(256) {}

EpollSelector::~EpollSelector() = default;

void EpollSelector::internalUpdate(Channel* channel, int op,
                                   SelectEvents events) {
  if (channel_.count(channel) == 0) {
    return;
  }
  
  if (op == EPOLL_CTL_DEL) {
    channel_.erase(channel);
  }
  
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;
  int efd = fd_.Descriptor();
  int cfd = channel->GetFile().Descriptor();
  if (::epoll_ctl(efd, op, cfd, op == EPOLL_CTL_DEL ? nullptr : &ev)) {
    PEDRONET_FATAL("epoll_ctl({}) failed, reason[{}]", *channel, Error{errno});
  }
}

void EpollSelector::Add(Channel* channel, SelectEvents events) {
  channel_.emplace(channel);
  internalUpdate(channel, EPOLL_CTL_ADD, events);
}

void EpollSelector::Update(Channel* channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_MOD, events);
}

void EpollSelector::Remove(Channel* channel) {
  internalUpdate(channel, EPOLL_CTL_DEL, SelectEvents::kNoneEvent);
}

Error EpollSelector::Wait(Duration timeout) {
  int efd = fd_.Descriptor();
  int n = ::epoll_wait(efd, buf_.data(), (int)buf_.size(),
                       (int)timeout.Milliseconds());

  len_ = std::max(0, n);
  return n < 0 ? Error{errno} : Error::Success();
}

size_t EpollSelector::Size() const {
  return len_;
}

SelectChannel EpollSelector::Get(size_t index) const {
  return {(Channel*)buf_[index].data.ptr, ReceiveEvents{buf_[index].events}};
}

bool EpollSelector::Contain(Channel* channel) const noexcept {
  return channel_.count(channel) != 0;
}

}  // namespace pedronet