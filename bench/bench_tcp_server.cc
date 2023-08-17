#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_server.h>
#include "pedrolib/logger/logger.h"

using namespace std::chrono_literals;
using pedrolib::Logger;
using pedrolib::Timestamp;
using pedronet::ArrayBuffer;
using pedronet::EpollSelector;
using pedronet::Error;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::TcpConnectionPtr;
using pedronet::TcpServer;

int main() {
  TcpServer server;
  pedronet::logger::SetLevel(Logger::Level::kWarn);

  Logger logger("bench");
  logger.SetLevel(Logger::Level::kWarn);

  auto boss_group = EventLoopGroup::Create(1);
  auto worker_group = EventLoopGroup::Create();

  server.SetGroup(boss_group, worker_group);
  server.OnConnect([&](const TcpConnectionPtr& conn) {
    logger.Info("peer connect: {}", *conn);
  });
  server.OnClose([&](const TcpConnectionPtr& conn) {
    logger.Info("peer disconnect: {}", *conn);
  });
  server.OnError([&](const TcpConnectionPtr& conn, Error what) {
    logger.Error("peer {} error: {}", *conn, what);
  });

  server.OnMessage(
      [=](const TcpConnectionPtr& conn, ArrayBuffer& buffer, Timestamp now) {
        // Echo to peer.
        conn->Send(&buffer);
      });

  server.Bind(InetAddress::Create("0.0.0.0", 1082));
  server.Start();

  EventLoopGroup::Joins(boss_group, worker_group);

  return 0;
}