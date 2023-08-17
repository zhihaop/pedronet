#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>
#include "pedrolib/logger/logger.h"

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <future>

using namespace std::chrono_literals;
using pedrolib::Duration;
using pedrolib::Logger;
using pedrolib::StaticVector;
using pedronet::ArrayBuffer;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::Latch;
using pedronet::TcpClient;
using pedronet::TcpConnectionPtr;
using pedronet::Timestamp;

struct Result {
  std::string topic;
  Timestamp start_ts;
  Timestamp end_ts;

  uint64_t msg_send{};
  uint64_t byte_send{};
};

Result benchmark(InetAddress address, std::shared_ptr<EventLoopGroup> group,
                 std::string_view topic, size_t length, size_t clients,
                 Duration delay) {

  auto buf = std::string(length, 'a');

  StaticVector<TcpClient> tcp_clients(clients);

  std::mutex mu;
  Result result;
  std::atomic_uint64_t msg_send{};
  std::atomic_uint64_t byte_send{};

  Latch latch(clients);
  for (size_t i = 0; i < clients; ++i) {
    TcpClient& client = tcp_clients.emplace_back(address);
    client.SetGroup(group);

    client.OnConnect([&](const TcpConnectionPtr& conn) {
      std::lock_guard guard{mu};
      result.start_ts = std::max(result.start_ts, Timestamp::Now());
      conn->Send(buf);

      conn->GetEventLoop().ScheduleAfter(3s, [conn] { conn->Shutdown(); });
    });

    client.OnClose([&](const TcpConnectionPtr& conn) { latch.CountDown(); });

    client.OnMessage([&](const TcpConnectionPtr& conn, ArrayBuffer& buffer,
                         Timestamp now) {
      msg_send.fetch_add(1, std::memory_order_relaxed);
      byte_send.fetch_add(buffer.ReadableBytes(), std::memory_order_relaxed);
      if (delay == Duration::Zero()) {
        conn->Send(&buffer);
      } else {
        conn->GetEventLoop().ScheduleAfter(
            delay, [conn, &buffer] { conn->Send(&buffer); });
      }
    });
    client.Start();
  }

  latch.Await();

  result.topic = fmt::format("[{}] client:{}, package:{} KiB", topic, clients,
                             length >> 10);
  result.end_ts = Timestamp::Now();
  result.byte_send = byte_send;
  result.msg_send = msg_send;
  return result;
}

void PrintResult(const Result& result) {
  Duration duration = result.end_ts - result.start_ts;
  uint64_t ms = duration.Milliseconds();
  double avg_msg = 1000.0 * result.msg_send / ms;
  double avg_byte = 1000.0 * result.byte_send / ms;
  double avg_msg_byte = avg_byte / avg_msg;
  fmt::print("[{}] {:.2f} msg/s, {:.2f} MiB/s, {:.2f} byte/msg\n", result.topic,
             avg_msg, avg_byte / (1 << 20), avg_msg_byte);
}

void benchmark(InetAddress address, std::shared_ptr<EventLoopGroup> group,
               const std::string& topic) {
  // 32 B: many busy clients
  for (size_t c = 1024; c <= 65536; c *= 2) {
    PrintResult(benchmark(address, group, topic, 32, c, 0s));
  }

  // 1 KiB: many clients, some busy
  for (size_t c = 1024; c <= 65536; c *= 2) {
    auto ignore = std::async(std::launch::async, [&] {
      benchmark(address, group, topic, 32, c, 1s);
    });
    PrintResult(benchmark(address, group, topic + "(some busy)", 1 << 10, 32, 0s));
    ignore.get();
  }

  // 1 KiB
  for (size_t c = 1; c <= 64; c *= 2) {
    PrintResult(benchmark(address, group, topic, 1 << 10, c, 0s));
  }

  // 32 KiB
  for (size_t c = 1; c <= 64; c *= 2) {
    PrintResult(benchmark(address, group, topic, 32 << 10, c, 0s));
  }

  // 1 MiB
  for (size_t c = 1; c <= 64; c *= 2) {
    PrintResult(benchmark(address, group, topic, 1 << 20, c, 0s));
  }
}

int main() {
  pedronet::logger::SetLevel(Logger::Level::kError);
  fmt::print("start benchmarking...\n");

  auto group = EventLoopGroup::Create();

  benchmark(InetAddress::Create("127.0.0.1", 1082), group, "pedronet");
  //  benchmark(InetAddress::Create("127.0.0.1", 1083), group, "asio");
  //  benchmark(InetAddress::Create("127.0.0.1", 1084), group, "muduo");
  //  benchmark(InetAddress::Create("127.0.0.1", 1085), group, "netty");
  return 0;
}