#ifndef PEDRONET_TCP_CLIENT_H
#define PEDRONET_TCP_CLIENT_H

#include "pedronet/callbacks.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/eventloopgroup.h"
#include "pedronet/inetaddress.h"
#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"

#include <memory>
#include <unordered_set>
#include <utility>

namespace pedronet {

class TcpClientChannelHandler;

class TcpClient : pedrolib::noncopyable, pedrolib::nonmovable {
  friend class TcpClientChannelHandler;

 public:
  enum class State {
    kOffline,
    kConnecting,
    kConnected,
    kDisconnecting,
    kDisconnected
  };

 private:
  EventLoopGroup::Ptr worker_group_;
  TcpConnection::Ptr conn_;

  InetAddress address_;
  std::atomic<State> state_{State::kOffline};

  EventLoop* eventloop_{};
  ChannelBuilder builder_;

  TcpClientOptions options_{};

 private:
  void handleConnection(pedronet::Socket conn);
  void retry(Socket socket, Error reason);
  void raiseConnection();

 public:
  explicit TcpClient(InetAddress address) : address_(std::move(address)) {}

  void SetGroup(EventLoopGroup::Ptr worker_group) {
    worker_group_ = std::move(worker_group);
  }

  void Start();
  void Close();
  void Shutdown();
  void ForceClose();
  void ForceShutdown();

  void SetOptions(const TcpClientOptions& options) { options_ = options; }

  void SetBuilder(ChannelBuilder builder) { builder_ = std::move(builder); }

  [[nodiscard]] TcpConnection::Ptr GetConnection() const noexcept {
    return conn_;
  }

  [[nodiscard]] EventLoop* GetEventLoop() const noexcept { return eventloop_; }

  void Send(std::string message);
};
}  // namespace pedronet

#endif  // PEDRONET_TCP_CLIENT_H