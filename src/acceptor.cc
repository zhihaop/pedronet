#include "pedronet/acceptor.h"
#include <pedrolib/concurrent/latch.h>
#include "pedronet/eventloop.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

Acceptor::Acceptor(EventLoop& eventloop, const InetAddress& address,
                   const SocketOptions& options)
    : address_(address),
      channel_(std::make_shared<SocketChannel>(
          Socket::Create(address.Family(), true))),
      eventloop_(eventloop) {
  PEDRONET_TRACE("Acceptor::Acceptor()");

  channel_->SetOptions(options);
  channel_->SetSelector(eventloop.GetSelector());

  channel_->OnRead([this](ReceiveEvents events, Timestamp now) {
    while (true) {
      PEDRONET_TRACE("{}::handleRead()", *this);
      Socket socket;
      auto err = channel_->Accept(address_, &socket);
      if (!err.Empty()) {
        if (err.GetCode() == EAGAIN || err.GetCode() == EWOULDBLOCK) {
          break;
        }
        PEDRONET_ERROR("failed to accept [{}]", err);
        continue;
      }
      if (acceptor_callback_) {
        acceptor_callback_(std::move(socket));
      }
    }
  });
}

std::string Acceptor::String() const {
  return fmt::format("Acceptor[socket={}]", channel_->GetFile());
}

void Acceptor::Close() {
  PEDRONET_TRACE("Acceptor::Close() enter");
  pedrolib::Latch latch(1);
  eventloop_.Remove(channel_, [&] { latch.CountDown(); });
  latch.Await();
  PEDRONET_TRACE("Acceptor::Close() exit");
}

void Acceptor::Listen() {
  eventloop_.Add(channel_, [this] {
    channel_->SetReadable(true);
    channel_->Listen();
  });
}
}  // namespace pedronet