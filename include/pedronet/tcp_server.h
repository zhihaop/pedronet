#ifndef PEDRONET_TCP_SERVER_H
#define PEDRONET_TCP_SERVER_H

#include "pedrolib/buffer/buffer.h"
#include "pedronet/acceptor.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/eventloopgroup.h"
#include "pedronet/inetaddress.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"

#include <unordered_map>
#include <unordered_set>

namespace pedronet {

class TcpServerChannelHandler;

class TcpServer : pedrolib::noncopyable, pedrolib::nonmovable {
  friend class TcpServerChannelHandler;

  EventLoopGroup::Ptr boss_group_;
  EventLoopGroup::Ptr worker_group_;
  Acceptor::Ptr acceptor_;

  ChannelBuilder builder_;

  std::mutex mu_;
  std::unordered_set<TcpConnection::Ptr> conns_;

  TcpServerOptions options_{};

 public:
  TcpServer() = default;
  ~TcpServer() { Close(); }

  void SetOptions(const TcpServerOptions& options) { options_ = options; }

  void SetGroup(EventLoopGroup::Ptr boss, EventLoopGroup::Ptr worker) {
    boss_group_ = std::move(boss);
    worker_group_ = std::move(worker);
  }

  void Bind(const InetAddress& address);

  void Start();
  void Close();

  void SetBuilder(ChannelBuilder builder) { builder_ = std::move(builder); }
};

}  // namespace pedronet

#endif  // PEDRONET_TCP_SERVER_H