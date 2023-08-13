#ifndef PEDRONET_SELECTOR_SELECTOR_H
#define PEDRONET_SELECTOR_SELECTOR_H

#include <pedrolib/duration.h>
#include <pedrolib/nonmovable.h>
#include <pedrolib/timestamp.h>
#include "pedrolib/file/file.h"
#include "pedronet/event.h"

#include <functional>
#include <memory>

namespace pedronet {

struct Channel;

using SelectChannel = std::pair<Channel*, ReceiveEvents>;

struct Selector : pedrolib::noncopyable, pedrolib::nonmovable {
  virtual void Add(Channel* channel, SelectEvents events) = 0;
  virtual void Remove(Channel* channel) = 0;
  virtual void Update(Channel* channel, SelectEvents events) = 0;
  virtual Error Wait(Duration timeout) = 0;
  [[nodiscard]] virtual size_t Size() const = 0;
  [[nodiscard]] virtual SelectChannel Get(size_t index) const = 0;
  virtual ~Selector() = default;
};
}  // namespace pedronet
#endif  // PEDRONET_SELECTOR_SELECTOR_H