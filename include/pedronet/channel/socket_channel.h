#ifndef PEDRONET_CHANNEL_SOCKET_CHANNEL_H
#define PEDRONET_CHANNEL_SOCKET_CHANNEL_H

#include <pedrolib/format/formatter.h>
#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/socket.h"

namespace pedronet {

struct Selector;

struct ChannelHandler {
  virtual void OnRead(Timestamp now, ArrayBuffer& buffer) = 0;
  virtual void OnWriteComplete(Timestamp now) = 0;
  virtual void OnError(Timestamp now, Error err) = 0;
  virtual void OnConnect(const std::shared_ptr<TcpConnection>& conn,
                         Timestamp now) = 0;
  virtual void OnClose(Timestamp now) = 0;
};

class ChannelHandlerAdaptor : public ChannelHandler {
 public:
  void OnRead(Timestamp now, ArrayBuffer& buffer) override {}
  void OnWriteComplete(Timestamp now) override {}
  void OnError(Timestamp now, Error err) override {}
  void OnConnect(const std::shared_ptr<TcpConnection>& conn,
                 Timestamp now) override {
    conn_ = conn;
  }
  void OnClose(Timestamp now) override { conn_.reset(); }
  TcpConnection* GetRawConnection() { return conn_.get(); }
  auto GetConnection() { return conn_; }

 private:
  std::shared_ptr<TcpConnection> conn_;
};

class SocketChannel final : public Socket, public Channel {
 protected:
  SelectEvents events_{SelectEvents::kNoneEvent};

  SelectorCallback close_callback_;
  SelectorCallback read_callback_;
  SelectorCallback error_callback_;
  SelectorCallback write_callback_;

  Selector* selector_{};
  int priority_{};

 public:
  explicit SocketChannel(Socket socket) : Socket(std::move(socket)) {}

  ~SocketChannel() override = default;

  void SetPriority(int priority) { priority_ = priority; }

  void SetSelector(Selector* selector) { selector_ = selector; }

  void OnRead(SelectorCallback read_callback) {
    read_callback_ = std::move(read_callback);
  }

  void OnClose(SelectorCallback close_callback) {
    close_callback_ = std::move(close_callback);
  }

  void OnWrite(SelectorCallback write_callback) {
    write_callback_ = std::move(write_callback);
  }

  void OnError(SelectorCallback error_callback) {
    error_callback_ = std::move(error_callback);
  }

  void HandleEvents(ReceiveEvents events, Timestamp now) final;

  void SetReadable(bool on);

  [[nodiscard]] bool Readable() const noexcept {
    return events_.Contains(SelectEvents::kReadEvent);
  }

  [[nodiscard]] bool Writable() const noexcept {
    return events_.Contains(SelectEvents::kWriteEvent);
  }

  void SetWritable(bool on);

  Socket& GetFile() noexcept final { return *this; }

  [[nodiscard]] const Socket& GetFile() const noexcept final { return *this; }

  [[nodiscard]] std::string String() const override {
    return fmt::format("SocketChannel[fd={}]", fd_);
  }
};

}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::SocketChannel);
#endif  // PEDRONET_CHANNEL_SOCKET_CHANNEL_H