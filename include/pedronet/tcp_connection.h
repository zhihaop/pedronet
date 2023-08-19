#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "pedrolib/buffer/array_buffer.h"
#include "pedrolib/buffer/buffer.h"
#include "pedronet/callbacks.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/logger/logger.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"

#include <any>
#include <memory>
#include <utility>

namespace pedronet {

class TcpConnection;

class TcpConnection : pedrolib::noncopyable,
                      pedrolib::nonmovable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

 protected:
  std::atomic<State> state_{TcpConnection::State::kConnecting};

  std::shared_ptr<ChannelHandler> handler_;

  ArrayBuffer output_;
  ArrayBuffer input_;

  SocketChannel channel_;
  InetAddress local_;
  EventLoop& eventloop_;

  void handleRead(Timestamp now);
  void handleError(Error);
  void handleWrite();

  void handleClose();

 public:
  TcpConnection(EventLoop& eventloop, Socket socket);

  ~TcpConnection();

  auto& GetSocket() noexcept { return channel_.GetFile(); }

  State GetState() const noexcept { return state_; }

  void Start();

  template <class Packable>
  void SendPackable(Packable&& packable) {
    if (eventloop_.CheckUnderLoop()) {
      if (GetState() != State::kConnected) {
        return;
      }
      packable.Pack(&output_);

      if (output_.ReadableBytes()) {
        channel_.SetWritable(true);
      }
      handleWrite();
      return;
    }

    eventloop_.Schedule(
        [this, clone = std::forward<Packable>(packable)]() mutable {
          SendPackable(std::forward<Packable>(clone));
        });
  }

  void Send(ArrayBuffer* buf) {
    if (eventloop_.CheckUnderLoop()) {
      std::string_view view{buf->ReadIndex(), buf->ReadableBytes()};
      handleSend(view);
      buf->Reset();
      return;
    }

    std::string clone(buf->ReadIndex(), buf->ReadableBytes());
    buf->Reset();
    eventloop_.Schedule([self = shared_from_this(),
                         clone = std::move(clone)]() { self->Send(clone); });
  }

  void Send(std::string_view buffer) {
    if (eventloop_.CheckUnderLoop()) {
      handleSend(buffer);
      return;
    }

    eventloop_.Run([self = shared_from_this(), clone = std::string(buffer)] {
      self->handleSend(clone);
    });
  }

  void Send(std::string buffer) {
    eventloop_.Run([self = shared_from_this(), clone = std::move(buffer)] {
      self->handleSend(clone);
    });
  }

  void SetHandler(std::shared_ptr<ChannelHandler> handler) {
    handler_ = std::move(handler);
  }

  const InetAddress& GetLocalAddress() const noexcept { return local_; }
  InetAddress GetPeerAddress() const noexcept {
    return channel_.GetPeerAddress();
  }

  void Close();
  void Shutdown();
  void ForceShutdown();
  void ForceClose();

  EventLoop& GetEventLoop() noexcept { return eventloop_; }

  std::string String() const;
  void handleSend(std::string_view buffer);
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::TcpConnection);
#endif  // PEDRONET_TCP_CONNECTION_H