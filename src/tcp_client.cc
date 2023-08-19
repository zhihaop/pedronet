#include <utility>

#include "pedronet/logger/logger.h"
#include "pedronet/tcp_client.h"

using pedronet::Duration;

namespace pedronet {

class TcpClientChannelHandler final : public ChannelHandler {
 public:
  explicit TcpClientChannelHandler(ChannelContext::Ptr ctx, TcpClient* client)
      : client_(client) {
    handler_ = client_->builder_(std::move(ctx));
  }

  void OnRead(Timestamp now, ArrayBuffer& buffer) override {
    handler_->OnRead(now, buffer);
  }
  void OnWriteComplete(Timestamp now) override {
    handler_->OnWriteComplete(now);
  }
  void OnError(Timestamp now, Error err) override {
    handler_->OnError(now, err);
  }
  void OnConnect(Timestamp now) override { handler_->OnConnect(now); }

  void OnClose(Timestamp now) override {
    handler_->OnClose(now);

    client_->state_ = TcpClient::State::kDisconnected;
    client_->conn_.reset();
  }

 private:
  TcpClient* client_;
  std::shared_ptr<ChannelHandler> handler_;
};

void TcpClient::handleConnection(Socket socket) {
  State s = State::kConnecting;
  if (!state_.compare_exchange_strong(s, State::kConnected)) {
    PEDRONET_WARN("state_ != State::kConnection, connection closed");
    return;
  }

  auto conn = std::make_shared<TcpConnection>(*eventloop_, std::move(socket));
  conn->SetHandler(std::make_shared<TcpClientChannelHandler>(
      conn->GetChannelContext(), this));
  conn->Start();
  conn_ = conn;
}

void TcpClient::raiseConnection() {
  if (state_ != State::kConnecting) {
    PEDRONET_WARN("TcpClient::raiseConnection() state is not kConnecting");
    return;
  }

  Socket socket = Socket::Create(address_.Family(), true);
  socket.SetOptions(options_.options);

  auto err = socket.Connect(address_);
  switch (err.GetCode()) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      handleConnection(std::move(socket));
      return;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(std::move(socket), err);
      return;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      PEDRONET_ERROR("raiseConnection error: {}", err);
      break;

    default:
      PEDRONET_ERROR("unexpected raiseConnection error: {}", err);
      break;
  }

  state_ = State::kOffline;
}

void TcpClient::retry(Socket socket, Error reason) {
  socket.Close();
  PEDRONET_TRACE("TcpClient::retry(): {}", reason);
  eventloop_->ScheduleAfter(Duration::Seconds(1), [&] { raiseConnection(); });
}

void TcpClient::Start() {
  PEDRONET_TRACE("TcpClient::Start()");

  State s = State::kOffline;
  if (!state_.compare_exchange_strong(s, State::kConnecting)) {
    PEDRONET_WARN("TcpClient::Start() has been invoked");
    return;
  }

  eventloop_ = &worker_group_->Next();
  eventloop_->Run([this] { raiseConnection(); });
}

void TcpClient::Close() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (conn_) {
    conn_->Close();
  }
}

void TcpClient::ForceClose() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (conn_) {
    conn_->ForceClose();
  }
}
void TcpClient::Shutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (conn_) {
    conn_->Shutdown();
  }
}
void TcpClient::ForceShutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (conn_) {
    conn_->ForceShutdown();
  }
}
}  // namespace pedronet