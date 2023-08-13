#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>
#include "pedrolib/logger/logger.h"

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

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

Result benchmark(std::shared_ptr<EventLoopGroup> group, std::string_view topic,
                 size_t length, size_t clients) {

  auto buf = std::string(length, 'a');

  StaticVector<TcpClient> tcp_clients(clients);
  InetAddress address = InetAddress::Create("127.0.0.1", 1082);

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

      conn->GetEventLoop().ScheduleAfter(5s, [conn] { conn->Shutdown(); });
    });

    client.OnClose([&](const TcpConnectionPtr& conn) { latch.CountDown(); });

    client.OnMessage([&](const TcpConnectionPtr& conn, ArrayBuffer& buffer,
                         Timestamp now) {
      msg_send.fetch_add(1, std::memory_order_relaxed);
      byte_send.fetch_add(buffer.ReadableBytes(), std::memory_order_relaxed);
      conn->Send(&buffer);
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
  fmt::print("[{}] {:.2f} msg/s, {:.2f} MiB/s, {:.2f} byte/msg\n", result.topic, avg_msg,
             avg_byte / (1 << 20), avg_msg_byte);
}

int main() {
  fmt::print("start benchmarking...\n");

  auto group = EventLoopGroup::Create();

  // 1 KiB
  for (size_t c = 1; c <= 64; c *= 2) {
    PrintResult(benchmark(group, "pedronet", 1 << 10, c));
  }

  // 32 KiB
  for (size_t c = 1; c <= 64; c *= 2) {
    PrintResult(benchmark(group, "pedronet", 32 << 10, c));
  }

  // 1 MiB
  for (size_t c = 1; c <= 64; c *= 2) {
    PrintResult(benchmark(group, "pedronet", 1 << 20, c));
  }
  return 0;
}