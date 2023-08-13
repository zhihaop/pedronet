#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>
#include "pedrolib/logger/logger.h"
#include "reporter.h"

using namespace std::chrono_literals;
using pedrolib::Duration;
using pedrolib::Logger;
using pedrolib::StaticVector;
using pedronet::ArrayBuffer;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::TcpClient;
using pedronet::TcpConnectionPtr;

int main() {
  pedronet::logger::SetLevel(Logger::Level::kInfo);
  Logger logger("bench");
  logger.SetLevel(Logger::Level::kTrace);

  auto worker_group = EventLoopGroup::Create();

  Reporter reporter;
  reporter.SetCallback([&](size_t bps, size_t ops, size_t, size_t max_bytes) {
    double speed = 1.0 * static_cast<double>(bps) / (1 << 20);
    logger.Info("client receive: {} MiB/s, {} packages/s, {} bytes/msg", speed,
                ops, max_bytes);
  });
  reporter.Start(*worker_group, Duration::Seconds(1));

  auto buf = std::string(1 << 20, 'a');

  size_t n_clients = 128;
  StaticVector<TcpClient> clients(n_clients);
  InetAddress address = InetAddress::Create("127.0.0.1", 1082);
  for (size_t i = 0; i < n_clients; ++i) {
    TcpClient& client = clients.emplace_back(address);
    client.SetGroup(worker_group);

    client.OnConnect([buf](const TcpConnectionPtr& conn) { conn->Send(buf); });
    client.OnMessage(
        [&](const TcpConnectionPtr& conn, ArrayBuffer& buffer, auto) {
          reporter.Trace(buffer.ReadableBytes());
          conn->Send(&buffer);
        });
    client.Start();
  }

  worker_group->Join();
  return 0;
}