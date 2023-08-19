#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_server.h>
#include "pedrolib/logger/logger.h"

using namespace std::chrono_literals;
using pedrolib::Logger;
using pedrolib::Timestamp;
using pedronet::ArrayBuffer;
using pedronet::ChannelHandlerAdaptor;
using pedronet::EpollSelector;
using pedronet::Error;
using pedronet::EventLoopGroup;
using pedronet::EventLoopOptions;
using pedronet::InetAddress;
using pedronet::TcpConnection;
using pedronet::TcpServer;

class EchoServerHandler : public ChannelHandlerAdaptor {
 public:
  explicit EchoServerHandler(Logger& logger,
                             const std::weak_ptr<TcpConnection>& conn)
      : ChannelHandlerAdaptor(conn), logger_(logger) {}

  void OnRead(Timestamp now, ArrayBuffer& buffer) override {
    if (conn_ != nullptr) {
      conn_->Send(&buffer);
    }
  }

  void OnError(Timestamp now, Error err) override {
    logger_.Error("peer {} error: {}", *GetConnection(), err);
  }

  void OnConnect(Timestamp now) override { conn_ = GetConnection().get(); }

  void OnClose(Timestamp now) override { conn_ = nullptr; }

 private:
  Logger& logger_;
  TcpConnection* conn_ = nullptr;
};

int main() {
  TcpServer server;
  pedronet::logger::SetLevel(Logger::Level::kWarn);

  Logger logger("bench");
  logger.SetLevel(Logger::Level::kInfo);

  EventLoopOptions options;
  options.selector_type = pedronet::SelectorType::kEpoll;
  auto boss_group = EventLoopGroup::Create(1);
  auto worker_group = EventLoopGroup::Create(32, options);

  server.SetGroup(boss_group, worker_group);

  server.SetBuilder([&](const std::weak_ptr<TcpConnection>& conn) {
    return std::make_shared<EchoServerHandler>(logger, conn);
  });

  server.Bind(InetAddress::Create("0.0.0.0", 1082));
  server.Start();

  EventLoopGroup::Joins(boss_group, worker_group);

  return 0;
}