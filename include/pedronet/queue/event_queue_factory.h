#ifndef PEDRONET_EVENT_QUEUE_FACTORY_H
#define PEDRONET_EVENT_QUEUE_FACTORY_H

#include "pedronet/queue/event_blocking_queue.h"
#include "pedronet/queue/event_double_buffer_queue.h"
#include "pedronet/queue/event_lock_free_queue.h"

namespace pedronet {

enum class EventQueueType {
  kBlockingQueue,
  kDoubleBufferQueue,
  kLockFreeQueue,
};

inline static std::unique_ptr<EventQueue> MakeEventQueue(EventQueueType type,
                                          EventChannel* channel) {
  switch (type) {
    case EventQueueType::kBlockingQueue:
      return std::make_unique<EventBlockingQueue>(channel);
    case EventQueueType::kDoubleBufferQueue:
      return std::make_unique<EventDoubleBufferQueue>(channel);
    case EventQueueType::kLockFreeQueue:
      return std::make_unique<EventLockFreeQueue>(channel);
    default:
      return nullptr;
  }
}

}  // namespace pedronet
#endif  // PEDRONET_EVENT_QUEUE_FACTORY_H
