#include <pedrolib/concurrent/latch.h>
#include <pedrolib/logger/logger.h>
#include <pedronet/eventloop.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/logger/logger.h>
#include <future>

using pedrolib::Latch;
using pedrolib::Logger;
using pedronet::EpollSelector;
using pedronet::EventLoop;

int main() {
  EventLoop executor(std::make_unique<EpollSelector>());

  Logger logger("bench");
  logger.SetLevel(Logger::Level::kTrace);
  pedronet::logger::SetLevel(Logger::Level::kWarn);
  auto defer = std::async(std::launch::async, [&] { executor.Loop(); });

  int n = 1e7 + 5;
  std::atomic<int> counter = 0;

  Latch latch(1);
  logger.Info("bench start");
  for (int i = 0; i < n; ++i) {
    executor.Schedule([&] {
      counter++;
      if (counter == n) {
        latch.CountDown();
      }
    });
  }
  latch.Await();
  logger.Info("bench end: {}", counter.load());
  executor.Close();
  return 0;
}
