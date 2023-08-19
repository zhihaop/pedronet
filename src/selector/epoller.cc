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
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;
  int efd = fd_.Descriptor();
  int cfd = channel->GetFile().Descriptor();
  if (::epoll_ctl(efd, op, cfd, op == EPOLL_CTL_DEL ? nullptr : &ev)) {
    PEDRONET_FATAL("epoll_ctl({}) failed, reason[{}]", *channel, Error{errno});
  }
}

void EpollSelector::Add(const Channel::Ptr& channel, SelectEvents events) {
  channel_.emplace(channel.get(), channel);
  internalUpdate(channel.get(), EPOLL_CTL_ADD, events);
}

void EpollSelector::Update(Channel* channel, SelectEvents events) {
  auto it = channel_.find(channel);
  if (it != channel_.end()) {
    internalUpdate(channel, EPOLL_CTL_MOD, events);
  }
}

void EpollSelector::Remove(const Channel::Ptr& channel) {
  auto it = channel_.find(channel.get());
  if (it != channel_.end()) {
    internalUpdate(channel.get(), EPOLL_CTL_DEL, SelectEvents::kNoneEvent);
    channel_.erase(channel.get());
  }
}

Error EpollSelector::Wait(Duration timeout) {
  int efd = fd_.Descriptor();
  int n = ::epoll_wait(efd, buf_.data(), (int)buf_.size(),
                       (int)timeout.Milliseconds());

  len_ = std::max(0, n);
  if (len_ == buf_.size() && len_ < 65536) {
    buf_.resize(buf_.size() << 1);
  }
  return n < 0 ? Error{errno} : Error::Success();
}

size_t EpollSelector::Size() const {
  return len_;
}

SelectChannel EpollSelector::Get(size_t index) const {
  auto* ptr = (Channel*)buf_[index].data.ptr;
  auto it = channel_.find(ptr);
  if (it != channel_.end()) {
    return {it->second, ReceiveEvents{buf_[index].events}};
  }
  return {nullptr, ReceiveEvents{buf_[index].events}};
}

bool EpollSelector::Contain(const Channel::Ptr& channel) const noexcept {
  return channel_.count(channel.get()) != 0;
}

}  // namespace pedronet