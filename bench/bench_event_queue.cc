#include <pedrolib/concurrent/latch.h>
#include <pedrolib/logger/logger.h>
#include <pedronet/eventloop.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <future>
#include <vector>

using pedrolib::Latch;
using pedrolib::Logger;
using pedronet::EpollSelector;
using pedronet::EventLoop;
using pedronet::EventQueueType;
using pedronet::TimerQueueType;

int main() {
  EventLoop::Options options{};
  options.event_queue_type = EventQueueType::kLockFreeQueue;
  options.timer_queue_type = TimerQueueType::kHeap;
  options.selector_type = pedronet::SelectorType::kEpoll;
  EventLoop executor(options);

  Logger logger("bench");
  logger.SetLevel(Logger::Level::kTrace);
  pedronet::logger::SetLevel(Logger::Level::kWarn);
  std::vector<std::future<void>> defers;

  defers.emplace_back(std::async(std::launch::async, [&] { executor.Loop(); }));

  int n = 1000000;
  int m = 16;
  std::atomic<int> counter = 0;
  Latch latch(m);

  logger.Info("bench start");
  for (int p = 0; p < m; ++p) {
    defers.emplace_back(std::async(std::launch::async, [&] {
      for (int i = 0; i < n; ++i) {
        executor.Schedule([&] { counter++; });
      }
      executor.Schedule([&] { latch.CountDown(); });
    }));
  }

  latch.Await();
  logger.Info("bench end: {}", counter.load());
  executor.Close();
  return 0;
}