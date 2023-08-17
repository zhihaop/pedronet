#ifndef PEDRONET_QUEUE_TIMER_QUEUE_FACTORY_H
#define PEDRONET_QUEUE_TIMER_QUEUE_FACTORY_H
#include "pedronet/queue/timer_hash_wheel.h"
#include "pedronet/queue/timer_heap_queue.h"
#include "pedronet/options.h"

namespace pedronet {

inline static std::unique_ptr<TimerQueue> MakeTimerQueue(TimerQueueType type,
                                           TimerChannel* channel) {
  switch (type) {
    case TimerQueueType::kHashWheel:
      return std::make_unique<TimerHashWheel>(channel);
    case TimerQueueType::kHeap:
      return std::make_unique<TimerHeapQueue>(channel);
    default:
      return nullptr;
  }
}

}  // namespace pedronet
#endif  // PEDRONET_QUEUE_TIMER_QUEUE_FACTORY_H
