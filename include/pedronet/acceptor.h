#ifndef PEDRONET_ACCEPTOR_H
#define PEDRONET_ACCEPTOR_H
#include "pedronet/channel/socket_channel.h"
#include "pedronet/options.h"
#include "pedronet/socket.h"

#include <pedrolib/format/formatter.h>
#include <algorithm>

namespace pedronet {

class EventLoop;

using AcceptorCallback = std::function<void(Socket)>;

class Acceptor : pedrolib::noncopyable,
                 pedrolib::nonmovable,
                 public std::enable_shared_from_this<Acceptor> {
 protected:
  AcceptorCallback acceptor_callback_;
  InetAddress address_;

  SocketChannel::Ptr channel_;
  EventLoop& eventloop_;

 public:
  using Ptr = std::shared_ptr<Acceptor>;

  Acceptor(EventLoop& eventloop, const InetAddress& address,
           const SocketOptions& option);

  ~Acceptor() { Close(); }

  void Bind() { channel_->Bind(address_); }

  void OnAccept(AcceptorCallback acceptor_callback) {
    acceptor_callback_ = std::move(acceptor_callback);
  }

  void Listen();

  void Close();

  [[nodiscard]] std::string String() const;
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::Acceptor);

#endif  // PEDRONET_ACCEPTOR_H