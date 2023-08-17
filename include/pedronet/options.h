#ifndef PEDRONET_OPTIONS_H
#define PEDRONET_OPTIONS_H

#include "pedronet/defines.h"

namespace pedronet {

enum class EventQueueType {
  kBlockingQueue,
  kDoubleBufferQueue,
  kLockFreeQueue,
};

enum class TimerQueueType { kHashWheel, kHeap };

enum class SelectorType { kEpoll, kPoll };

struct SocketOptions {
  bool reuse_addr{true};
  bool reuse_port{false};
  bool keep_alive{true};
  bool tcp_no_delay{true};
};

struct EventLoopOptions {
  EventQueueType event_queue_type{EventQueueType::kLockFreeQueue};
  TimerQueueType timer_queue_type{TimerQueueType::kHeap};
  SelectorType selector_type{SelectorType::kEpoll};
  Duration select_timeout{Duration::Seconds(10)};
};

struct TcpServerOptions {
  SocketOptions boss_options{};
  SocketOptions child_options{};
};

struct TcpClientOptions {
  SocketOptions options{};
};

}  // namespace pedronet

#endif  //PEDRONET_OPTIONS_H
