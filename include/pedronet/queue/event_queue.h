#ifndef PEDRONET_QUEUE_EVENT_QUEUE_H
#define PEDRONET_QUEUE_EVENT_QUEUE_H

#include "pedronet/defines.h"

namespace pedronet {

struct EventQueue {
  virtual void Add(Callback callback) = 0;
  virtual void Process() = 0;
  virtual size_t Size() = 0;
};
}  // namespace pedronet

#endif  // PEDRONET_QUEUE_EVENT_QUEUE_H
