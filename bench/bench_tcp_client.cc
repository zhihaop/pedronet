#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>
#include "pedrolib/logger/logger.h"

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <future>
#include <utility>

using namespace std::chrono_literals;
using pedrolib::Duration;
using pedrolib::Logger;
using pedrolib::StaticVector;
using pedronet::ArrayBuffer;
using pedronet::ChannelHandlerAdaptor;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::Latch;
using pedronet::TcpClient;
using pedronet::TcpConnection;
using pedronet::TcpConnectionPtr;
using pedronet::Timestamp;

struct Result {
  std::string topic;
  Timestamp start_ts;
  Timestamp end_ts;

  uint64_t msg_send{};
  uint64_t byte_send{};
};

struct TestOptions {
  InetAddress address;

  std::string topic;
  size_t threads{std::thread::hardware_concurrency()};
  size_t clients{32};
  size_t length{1 << 10};

  Duration duration{Duration::Seconds(2)};
  Duration echo_delay{};
  bool random_delay{true};
};

class EchoClientChannelHandler : public ChannelHandlerAdaptor {
 public:
  EchoClientChannelHandler(const std::weak_ptr<TcpConnection>& conn,
                           Latch& closeLatch, Latch& connectLatch,
                           std::mutex& mu, Result& result, TestOptions options)
      : ChannelHandlerAdaptor(conn),
        close_latch_(closeLatch),
        connect_latch_(connectLatch),
        mu_(mu),
        result_(result),
        options_(std::move(options)),
        dist_(0, 1000),
        gen_(time(nullptr)) {}

  void OnRead(Timestamp now, ArrayBuffer& buffer) override {
    auto conn = GetConnection();
    if (conn == nullptr) {
      return;
    }

    msg_snd_ += 1;
    byte_snd_ += buffer.ReadableBytes();

    if (options_.echo_delay == Duration::Zero()) {
      conn->Send(&buffer);
    } else {
      Duration d = options_.echo_delay;
      if (options_.random_delay) {
        d.usecs += dist_(gen_) * 1000;
      }
      std::string buf(buffer.ReadIndex(), buffer.ReadableBytes());
      buffer.Reset();

      conn->GetEventLoop().ScheduleAfter(d, [conn, buf] { conn->Send(buf); });
    }
  }

  void OnConnect(Timestamp now) override { connect_latch_.CountDown(); }

  void OnClose(Timestamp now) override {
    close_latch_.CountDown();

    std::lock_guard guard{mu_};
    result_.byte_send += byte_snd_;
    result_.msg_send += msg_snd_;
  }

 private:
  Latch& close_latch_;
  Latch& connect_latch_;

  std::mutex& mu_;
  Result& result_;

  TestOptions options_;

  size_t msg_snd_{};
  size_t byte_snd_{};

  std::uniform_int_distribution<int> dist_;
  std::mt19937_64 gen_;
};

Result benchmark(TestOptions options) {
  std::vector<std::shared_ptr<TcpClient>> clients(options.clients);

  auto group = EventLoopGroup::Create(options.threads);
  auto buf = std::string(options.length, 'a');

  Result result;
  result.topic = fmt::format("[{}] client:{}, package:{} KiB", options.topic,
                             clients.size(), buf.size() >> 10);

  std::mutex mu;

  Latch close_latch(options.clients);
  Latch connect_latch(options.clients);

  for (auto& client : clients) {
    client = std::make_shared<TcpClient>(options.address);
    client->SetGroup(group);
    client->SetBuilder([&](const std::weak_ptr<TcpConnection>& conn) {
      return std::make_shared<EchoClientChannelHandler>(
          conn, close_latch, connect_latch, mu, result, options);
    });
    client->Start();
  }

  connect_latch.Await();
  for (auto& client : clients) {
    std::weak_ptr<TcpClient> self = client;
    group->ScheduleAfter(options.duration, [self] {
      auto client = self.lock();
      if (client) {
        client->Shutdown();
      }
    });
  }

  result.start_ts = Timestamp::Now();
  for (auto& client : clients) {
    client->Send(buf);
  }
  close_latch.Await();
  group->Close();
  group->Join();
  result.end_ts = Timestamp::Now();

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

void benchmark(const InetAddress& address, const std::string& topic) {
  TestOptions options;
  options.address = address;
  options.topic = topic;
  options.duration = 3s;
  options.random_delay = false;
  options.threads = 32;
  options.clients = 1;
  options.length = 1 << 10;

  // 1 KiB: many busy clients
  for (size_t c = 1024; c < 32768; c *= 2) {
    TestOptions copy = options;
    copy.clients = c;
    PrintResult(benchmark(copy));
  }

  // 1 KiB: many clients, some busy
  for (size_t c = 1024; c < 32768; c *= 2) {
    auto ignore = std::async(std::launch::async, [=] {
      TestOptions copy = options;
      copy.clients = c;
      copy.random_delay = true;
      copy.echo_delay = 3s;
      copy.duration = 10s;
      benchmark(copy);
    });

    TestOptions copy = options;
    copy.clients = 256;
    copy.duration = 7s;
    copy.topic += "(some busy)";
    std::this_thread::sleep_for(2s);
    PrintResult(benchmark(copy));

    ignore.get();
  }

  // 1 KiB
  for (size_t c = 1; c <= 64; c *= 2) {
    TestOptions copy = options;
    copy.clients = c;
    copy.length = 1 << 10;
    PrintResult(benchmark(copy));
  }

  // 32 KiB
  for (size_t c = 1; c <= 64; c *= 2) {
    TestOptions copy = options;
    copy.clients = c;
    copy.length = 32 << 10;
    PrintResult(benchmark(copy));
  }

  // 1 MiB
  for (size_t c = 1; c <= 64; c *= 2) {
    TestOptions copy = options;
    copy.clients = c;
    copy.length = 1 << 20;
    PrintResult(benchmark(copy));
  }
}

int main() {
  pedronet::logger::SetLevel(Logger::Level::kWarn);
  fmt::print("start benchmarking...\n");

  benchmark(InetAddress::Create("127.0.0.1", 1082), "pedronet");
  benchmark(InetAddress::Create("127.0.0.1", 1083), "asio");
  benchmark(InetAddress::Create("127.0.0.1", 1084), "muduo");
  benchmark(InetAddress::Create("127.0.0.1", 1085), "netty");
  return 0;
}