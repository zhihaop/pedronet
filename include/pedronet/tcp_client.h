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

class TcpClient : pedrolib::noncopyable, pedrolib::nonmovable {
 public:
  enum class State {
    kOffline,
    kConnecting,
    kConnected,
    kDisconnecting,
    kDisconnected
  };

 private:
  EventLoopGroupPtr worker_group_;
  InetAddress address_;
  std::atomic<State> state_{State::kOffline};
  TcpConnectionPtr conn_;
  EventLoop* eventloop_{};

  ChannelBuilder builder_;
  TcpClientOptions options_{};

 private:
  void handleConnection(pedronet::Socket conn);
  void retry(Socket socket, Error reason);
  void raiseConnection();

 public:
  explicit TcpClient(InetAddress address) : address_(std::move(address)) {}

  void SetGroup(EventLoopGroupPtr worker_group) {
    worker_group_ = std::move(worker_group);
  }

  void Start();
  void Close();
  void Shutdown();
  void ForceClose();
  void ForceShutdown();

  void SetOptions(const TcpClientOptions& options) { options_ = options; }

  void SetBuilder(ChannelBuilder builder) { builder_ = std::move(builder); }

  [[nodiscard]] TcpConnectionPtr GetConnection() const noexcept {
    return conn_;
  }

  template <class Packable>
  bool Write(Packable&& packable) {
    if (state_ == State::kConnected) {
      conn_->SendPackable(std::forward<Packable>(packable));
      return true;
    }
    return false;
  }

  bool Send(std::string message) {
    if (state_ == State::kConnected) {
      conn_->Send(std::move(message));
      return true;
    }
    return false;
  }

  [[nodiscard]] State GetState() const noexcept { return state_; }
  auto GetConnection() noexcept { return conn_; }
};
}  // namespace pedronet

#endif  // PEDRONET_TCP_CLIENT_H