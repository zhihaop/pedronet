#ifndef PEDRONET_CALLBACK_H
#define PEDRONET_CALLBACK_H

#include "pedronet/defines.h"

#include <pedrolib/timestamp.h>
#include <functional>
#include <memory>
namespace pedronet {

class TcpConnection;
class ReceiveEvents;
class SelectEvents;
class Channel;

using Callback = std::function<void()>;
using SelectorCallback = std::function<void(ReceiveEvents events, Timestamp)>;

}  // namespace pedronet

#endif  // PEDRONET_CALLBACK_H