#include <memory>

#include "pedronet/logger/logger.h"
#include "pedronet/tcp_server.h"

namespace pedronet {

class TcpServerChannelHandler final : public ChannelHandler {
 public:
  explicit TcpServerChannelHandler(ChannelContext::Ptr ctx, TcpServer* server)
      : ctx_(ctx), server_(server) {
    handler_ = server_->builder_(std::move(ctx));
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

  void OnConnect(Timestamp now) override {
    handler_->OnConnect(now);

    std::unique_lock lock(server_->mu_);
    server_->conns_.emplace(ctx_->GetConnection()->shared_from_this());
  }

  void OnClose(Timestamp now) override {
    handler_->OnClose(now);
    std::unique_lock lock(server_->mu_);
    server_->conns_.erase(ctx_->GetConnection()->shared_from_this());
  }

 private:
  TcpServer* server_;
  ChannelContext::Ptr ctx_;
  std::shared_ptr<ChannelHandler> handler_;
};

void TcpServer::Start() {
  PEDRONET_TRACE("TcpServer::Start() enter");

  acceptor_->OnAccept([this](Socket socket) {
    PEDRONET_TRACE("TcpServer::OnAccept({})", socket);
    socket.SetOptions(options_.child_options);

    auto conn = std::make_shared<TcpConnection>(worker_group_->Next(),
                                                std::move(socket));
    conn->SetHandler(std::make_shared<TcpServerChannelHandler>(
        conn->GetChannelContext(), this));
    conn->Start();
  });

  acceptor_->Listen();
  PEDRONET_TRACE("TcpServer::Start() exit");
}

void TcpServer::Close() {
  PEDRONET_TRACE("TcpServer::Close() enter");
  acceptor_->Close();

  std::unique_lock<std::mutex> lock(mu_);
  for (auto& conn : conns_) {
    conn->Close();
  }
  conns_.clear();
}

void TcpServer::Bind(const pedronet::InetAddress& address) {
  PEDRONET_TRACE("TcpServer::Bind({})", address);

  if (!boss_group_) {
    PEDRONET_FATAL("boss group is not set");
  }

  acceptor_ = std::make_shared<Acceptor>(boss_group_->Next(), address,
                                         options_.boss_options);
  acceptor_->Bind();
}
}  // namespace pedronet