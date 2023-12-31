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

class ChannelContext {
  friend class TcpConnection;

 public:
  using Ptr = std::shared_ptr<ChannelContext>;

  TcpConnection* GetConnection() { return conn_; }
  ArrayBuffer* GetOutputBuffer();
  ArrayBuffer* GetInputBuffer();

 private:
  TcpConnection* conn_{};
};

struct ChannelHandler {
  using Ptr = std::shared_ptr<ChannelHandler>;

  virtual ~ChannelHandler() = default;
  virtual void OnRead(Timestamp now, ArrayBuffer& buffer) = 0;
  virtual void OnWriteComplete(Timestamp now) = 0;
  virtual void OnError(Timestamp now, Error err) = 0;
  virtual void OnConnect(Timestamp now) = 0;
  virtual void OnClose(Timestamp now) = 0;
};

using ChannelBuilder = std::function<ChannelHandler::Ptr(ChannelContext::Ptr)>;

class ChannelHandlerAdaptor : public ChannelHandler {
 public:
  using Ptr = std::shared_ptr<ChannelHandlerAdaptor>;

  explicit ChannelHandlerAdaptor(ChannelContext::Ptr ctx)
      : ctx_(std::move(ctx)) {}

  void OnRead(Timestamp now, ArrayBuffer& buffer) override {}
  void OnWriteComplete(Timestamp now) override {}
  void OnError(Timestamp now, Error err) override {}
  void OnConnect(Timestamp now) override {}
  void OnClose(Timestamp now) override {}
  auto GetConnection() { return ctx_->GetConnection(); }
  ChannelContext& GetContext() { return *ctx_; }

 private:
  ChannelContext::Ptr ctx_;
};

class TcpConnection final : pedrolib::noncopyable,
                            pedrolib::nonmovable,
                            public std::enable_shared_from_this<TcpConnection> {
 public:
  friend class ChannelContext;
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

 protected:
  std::atomic<State> state_{TcpConnection::State::kConnecting};

  ArrayBuffer output_;
  ArrayBuffer input_;

  SocketChannel::Ptr channel_;
  ChannelHandler::Ptr handler_;
  ChannelContext::Ptr context_;

  InetAddress local_;
  EventLoop& eventloop_;

  Latch close_latch_;

  void handleRead(Timestamp now);
  void handleError(Error);
  void handleWrite();
  void handleClose();
  void handleRemove();

 public:
  using Ptr = std::shared_ptr<TcpConnection>;

  TcpConnection(EventLoop& eventloop, Socket socket);

  ~TcpConnection();

  auto GetChannelContext() noexcept { return context_; }

  State GetState() const noexcept { return state_; }

  void Start();

  void Send(ArrayBuffer* buf);

  void Send(std::string_view buffer);

  void Send(std::string buffer);

  void SetHandler(std::shared_ptr<ChannelHandler> handler) {
    handler_ = std::move(handler);
  }

  const InetAddress& GetLocalAddress() const noexcept { return local_; }

  InetAddress GetPeerAddress() const noexcept {
    return channel_->GetPeerAddress();
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