#ifndef PERDONET_SELECTOR_EPOLLER_H
#define PERDONET_SELECTOR_EPOLLER_H

#include "pedrolib/file/file.h"
#include "pedronet/channel/channel.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"

#include <vector>

struct epoll_event;

namespace pedronet {

class EpollSelector : public Selector {
  void internalUpdate(Channel* channel, int op, SelectEvents events);

 public:
  EpollSelector();
  ~EpollSelector() override;

  void Add(Channel* channel, SelectEvents events) override;
  void Remove(Channel* channel) override;
  void Update(Channel* channel, SelectEvents events) override;

  Error Wait(Duration timeout) override;
  [[nodiscard]] size_t Size() const override;
  [[nodiscard]] SelectChannel Get(size_t index) const override;

 private:
  File fd_;
  size_t len_{};
  std::vector<struct epoll_event> buf_;
};
}  // namespace pedronet

#endif  // PERDONET_SELECTOR_EPOLLER_H