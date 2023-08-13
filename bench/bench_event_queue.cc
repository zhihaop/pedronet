#include <pedrolib/concurrent/latch.h>
#include <pedrolib/logger/logger.h>
#include <pedronet/eventloop.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <future>
#include <vector>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

using pedrolib::Latch;
using pedrolib::Logger;
using pedronet::EpollSelector;
using pedronet::EventLoop;
using pedronet::EventQueueType;
using pedronet::TimerQueueType;

void benchmark(EventLoop& executor, const std::string& topic, size_t thread) {
  std::vector<std::future<void>> defers;

  const int n = 1000000;

  ankerl::nanobench::Bench bench;
  bench.epochs(1);
  bench.title(topic);
  bench.epochIterations(1);
  bench.batch(n * thread);

  bench.run(fmt::format("{}-{}", topic, thread), [&] {
    Latch latch(thread * n);
    for (size_t i = 0; i < thread; ++i) {
      defers.emplace_back(std::async(std::launch::async, [&] {
        for (int j = 0; j < n; ++j) {
          executor.Schedule([&] { latch.CountDown(); });
        }
      }));
    }
    latch.Await();
  });
}

void benchmark(const EventLoop::Options& options, const std::string& topic) {
  EventLoop executor(options);
  auto defer = std::async(std::launch::async, [&] { executor.Loop(); });
  for (size_t i = 1; i <= 16; i *= 2) {
    benchmark(executor, topic, i);
  }
  executor.Close();
}

int main() {
  pedronet::logger::SetLevel(Logger::Level::kTrace);

  EventLoop::Options options{};
  options.timer_queue_type = TimerQueueType::kHeap;
  options.selector_type = pedronet::SelectorType::kEpoll;

  options.event_queue_type = EventQueueType::kLockFreeQueue;
  benchmark(options, "LockFreeQueue");

  options.event_queue_type = EventQueueType::kBlockingQueue;
  benchmark(options, "BlockingQueue");

  options.event_queue_type = EventQueueType::kDoubleBufferQueue;
  benchmark(options, "DoubleBufferQueue");

  return 0;
}