#ifndef PERDONET_SELECTOR_POLLER_H
#define PERDONET_SELECTOR_POLLER_H

#include "pedrolib/file/file.h"
#include "pedronet/channel/channel.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"

#include <sys/poll.h>
#include <vector>

namespace pedronet {

class Poller : public Selector {
  using Locator = std::pair<size_t, Channel*>;

  bool cleanable_{};
  size_t ready_{};
  std::vector<struct pollfd> buf_;
  std::unordered_map<int, Locator> channels_;

  void CleanUp() {
    size_t head = 0;
    for (auto& p : buf_) {
      if (channels_.count(p.fd) == 0) {
        continue;
      }

      buf_[head] = p;
      channels_[p.fd].first = head++;
    }
    buf_.resize(head);
  }

 public:
  Poller() = default;

  ~Poller() override = default;

  void Add(Channel* channel, SelectEvents events) override {
    auto& pfd = buf_.emplace_back();
    pfd.fd = channel->GetFile().Descriptor();
    pfd.events = (short)events.Value();
    pfd.revents = 0;
    channels_[pfd.fd] = {buf_.size() - 1, channel};
  }

  void Remove(Channel* channel) override {
    int fd = channel->GetFile().Descriptor();
    auto it = channels_.find(fd);
    if (it != channels_.end()) {
      channels_.erase(it);
      cleanable_ = true;
    }
  }

  void Update(Channel* channel, SelectEvents events) override {
    int fd = channel->GetFile().Descriptor();
    auto it = channels_.find(fd);
    if (it != channels_.end()) {
      buf_[it->second.first].events = (short)events.Value();
    }
  }

  Error Wait(Duration timeout) override {
    if (cleanable_) {
      CleanUp();
    }
    ready_ = 0;

    int n = poll(buf_.data(), buf_.size(), (int)timeout.Milliseconds());
    if (n < 0) {
      return Error{errno};
    }

    ready_ = n;

    std::partition(buf_.begin(), buf_.end(),
                   [](const struct pollfd& p) { return p.revents != 0; });

    return Error::Success();
  }

  [[nodiscard]] size_t Size() const override { return ready_; }

  [[nodiscard]] SelectChannel Get(size_t index) const override {
    auto& p = buf_[index];
    auto it = channels_.find(p.fd);
    if (it == channels_.end()) {
      return {nullptr, ReceiveEvents{0}};
    }
    return {it->second.second, ReceiveEvents{(uint32_t)p.revents}};
  }
};
}  // namespace pedronet

#endif  // PERDONET_SELECTOR_POLLER_H