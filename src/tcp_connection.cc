#include "pedronet/tcp_connection.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

void TcpConnection::Start() {
  auto self = shared_from_this();

  channel_.OnRead([self](auto events, auto now) { self->handleRead(now); });
  channel_.OnWrite([self](auto events, auto now) { self->handleWrite(); });
  channel_.OnClose([self](auto events, auto now) { self->handleClose(); });
  channel_.OnError(
      [self](auto, auto now) { self->handleError(self->channel_.GetError()); });
  channel_.SetSelector(eventloop_.GetSelector());

  auto handle_register = [this, self] {
    State s = State::kConnecting;
    if (!state_.compare_exchange_strong(s, State::kConnected)) {
      PEDRONET_ERROR("{} has been register to channel", *this);
      return;
    }

    PEDRONET_INFO("handleConnection {}", *this);
    handler_->OnConnect(self, Timestamp::Now());
    channel_.SetReadable(true);
  };

  auto handle_deregister = [this, self] {
    if (state_ != State::kDisconnected) {
      PEDRONET_ERROR("should not happened");
      return;
    }

    handler_->OnClose(Timestamp::Now());
  };

  eventloop_.Register(&channel_, std::move(handle_register),
                      std::move(handle_deregister));
}

void TcpConnection::handleRead(Timestamp now) {
  ssize_t n = input_.Append(&channel_.GetFile());
  if (n < 0) {
    auto err = Error{errno};
    if (err.GetCode() != EWOULDBLOCK && err.GetCode() != EAGAIN) {
      handleError(err);
    }
    return;
  }

  if (n == 0) {
    PEDRONET_INFO("close because no data");
    handleClose();
    return;
  }

  handler_->OnRead(now, input_);
}

void TcpConnection::handleError(Error err) {
  handler_->OnError(Timestamp::Now(), err);
}

void TcpConnection::handleWrite() {
  if (!channel_.Writable()) {
    PEDRONET_TRACE("{} is down, no more writing", *this);
    return;
  }

  if (output_.ReadableBytes()) {
    ssize_t n = channel_.Write(output_.ReadIndex(), output_.ReadableBytes());
    if (n < 0) {
      handleError(channel_.GetError());
      return;
    }
    output_.Retrieve(n);
  }

  if (output_.ReadableBytes() == 0) {
    channel_.SetWritable(false);

    handler_->OnWriteComplete(Timestamp::Now());
    if (state_ == State::kDisconnecting) {
      channel_.CloseWrite();
    }
  }
}

void TcpConnection::Close() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnected)) {
    return;
  }

  eventloop_.Run([this, self = shared_from_this()] {
    if (output_.ReadableBytes() == 0) {
      PEDRONET_TRACE("{}::Close()", *this);
      channel_.SetWritable(false);
      channel_.SetReadable(false);
      eventloop_.Deregister(&channel_);
    }
  });
}

std::string TcpConnection::String() const {
  return fmt::format("TcpConnection[local={}, peer={}, channel={}]", local_,
                     GetPeerAddress(), channel_);
}

TcpConnection::TcpConnection(EventLoop& eventloop, Socket socket)
    : channel_(std::move(socket)),
      local_(channel_.GetLocalAddress()),
      eventloop_(eventloop) {}

TcpConnection::~TcpConnection() {
  PEDRONET_TRACE("{}::~TcpConnection()", *this);
}

void TcpConnection::handleSend(std::string_view buffer) {
  State s = state_;
  if (s != State::kConnected) {
    PEDRONET_WARN("{}::SendPackable(): give up sending buffer", *this);
    return;
  }

  if (!output_.ReadableBytes() && !buffer.empty()) {
    ssize_t w = channel_.Write(buffer.data(), buffer.size());
    if (w < 0) {
      auto err = channel_.GetError();
      if (err.GetCode() != EWOULDBLOCK && err.GetCode() != EAGAIN) {
        handleError(err);
      }
    }
    buffer = buffer.substr(w < 0 ? 0 : w);
  }

  if (!buffer.empty()) {
    size_t w = output_.WritableBytes();
    if (w < buffer.size()) {
      // TODO(zhihaop) the buffer size may be too large.
      output_.EnsureWritable(buffer.size());
    }

    output_.Append(buffer.data(), buffer.size());
    channel_.SetWritable(true);
  }
}

void TcpConnection::ForceClose() {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnected;
  eventloop_.Run([this] {
    PEDRONET_TRACE("{}::Close()", *this);
    channel_.SetWritable(false);
    channel_.SetReadable(false);
    eventloop_.Deregister(&channel_);
  });
}

void TcpConnection::Shutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  eventloop_.Run([this] {
    if (output_.ReadableBytes() == 0) {
      PEDRONET_TRACE("{}::Close()", *this);
      channel_.SetWritable(false);
      channel_.CloseWrite();
    }
  });
}

void TcpConnection::ForceShutdown() {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnecting;

  eventloop_.Run([this] {
    PEDRONET_TRACE("{}::Close()", *this);
    channel_.SetWritable(false);
    channel_.CloseWrite();
  });
}

void TcpConnection::handleClose() {
  if (state_ == State::kDisconnected) {
    return;
  }
  PEDRONET_INFO("{}::handleClose()", *this);
  state_ = State::kDisconnected;
  eventloop_.Deregister(&channel_);
}

}  // namespace pedronet
