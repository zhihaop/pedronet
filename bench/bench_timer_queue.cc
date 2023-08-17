#include <pedrolib/concurrent/latch.h>
#include <pedrolib/logger/logger.h>
#include <pedronet/eventloop.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <future>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

using pedrolib::Latch;
using pedrolib::Logger;
using pedronet::Duration;
using pedronet::EpollSelector;
using pedronet::EventLoop;
using pedronet::EventLoopOptions;
using pedronet::EventQueueType;
using pedronet::TimerQueueType;

template <typename Generator>
void benchmark(const EventLoopOptions& options, const std::string& topic,
               ankerl::nanobench::Bench& bench, Generator&& generator) {
  EventLoop executor(options);
  auto defer = std::async(std::launch::async, [&] { executor.Loop(); });

  std::atomic_size_t counter = 0;
  bench.run(topic, [&] {
    executor.ScheduleAfter(Duration::Milliseconds(generator()),
                           [&] { counter++; });
  });
  bench.doNotOptimizeAway(counter);
  executor.Close();
}

void benchmark(const EventLoopOptions& options, const std::string& topic) {
  const int n = 10000000;

  ankerl::nanobench::Bench bench;
  bench.epochs(1);
  bench.title(topic);
  bench.epochIterations(n);
  bench.batch(1);

  std::mt19937_64 rnd(time(nullptr));
  benchmark(options, "RandomDelay(500, 500000)", bench, [&] {
    thread_local std::uniform_int_distribution<int> dist(500, 50000);
    return dist(rnd);
  });

  benchmark(options, "RandomDelay(500, 5000)", bench, [&] {
    thread_local std::uniform_int_distribution<int> dist(500, 5000);
    return dist(rnd);
  });
  
  benchmark(options, "RandomDelay(500, 1000)", bench, [&] {
    thread_local std::uniform_int_distribution<int> dist(500, 1000);
    return dist(rnd);
  });
  
  benchmark(options, "Delay(500)", bench, [&] {
    return 500;
  });
}

int main() {
  pedronet::logger::SetLevel(Logger::Level::kTrace);

  EventLoopOptions options{};
  options.event_queue_type = EventQueueType::kLockFreeQueue;
  options.timer_queue_type = TimerQueueType::kHeap;
  options.selector_type = pedronet::SelectorType::kEpoll;

  options.timer_queue_type = TimerQueueType::kHeap;
  benchmark(options, "HeapQueue");

  options.timer_queue_type = TimerQueueType::kHashWheel;
  benchmark(options, "HashTimeWheel");

  return 0;
}