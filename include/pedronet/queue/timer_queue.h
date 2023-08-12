#ifndef PEDRONET_QUEUE_TIMER_QUEUE_H
#define PEDRONET_QUEUE_TIMER_QUEUE_H

#include "pedronet/callbacks.h"
#include "pedronet/defines.h"

namespace pedronet {
struct TimerQueue {
  virtual uint64_t Add(Duration delay, Duration interval,
                       Callback callback) = 0;
  virtual void Cancel(uint64_t id) = 0;
  virtual void Process() = 0;
  virtual size_t Size() = 0;
};
}  // namespace pedronet
#endif  //PEDRONET_QUEUE_TIMER_QUEUE_H
