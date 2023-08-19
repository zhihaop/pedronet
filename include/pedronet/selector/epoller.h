#ifndef PERDONET_SELECTOR_EPOLLER_H
#define PERDONET_SELECTOR_EPOLLER_H

#include "pedrolib/file/file.h"
#include "pedronet/channel/channel.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"

#include <unordered_set>
#include <vector>

struct epoll_event;

namespace pedronet {

class EpollSelector : public Selector {
  void internalUpdate(Channel* channel, int op, SelectEvents events);

 public:
  EpollSelector();
  ~EpollSelector() override;

  void Add(const Channel::Ptr& channel, SelectEvents events) override;
  void Remove(const Channel::Ptr& channel) override;
  void Update(Channel* channel, SelectEvents events) override;

  bool Contain(const Channel::Ptr& channel) const noexcept override;

  Error Wait(Duration timeout) override;
  [[nodiscard]] size_t Size() const override;
  [[nodiscard]] SelectChannel Get(size_t index) const override;

 private:
  File fd_;
  size_t len_{};
  std::vector<struct epoll_event> buf_;
  std::unordered_map<Channel*, Channel::Ptr> channel_;
};
}  // namespace pedronet

#endif  // PERDONET_SELECTOR_EPOLLER_H