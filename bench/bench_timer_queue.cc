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
using pedronet::EventQueueType;
using pedronet::TimerQueueType;

void benchmark(const EventLoop::Options& options, const std::string& topic) {
  EventLoop executor(options);
  auto defer = std::async(std::launch::async, [&] { executor.Loop(); });

  const int n = 1000000;
  
  ankerl::nanobench::Bench bench;
  bench.epochs(1);
  bench.title(topic);
  bench.epochIterations(n);
  bench.batch(1);

  std::mt19937_64 rnd(time(nullptr));
  {
    Latch latch(n);
    std::uniform_int_distribution<int> dist(500, 5000);
    bench.run("RandomDelay(500, 5000)", [&] {
      executor.ScheduleAfter(Duration::Milliseconds(dist(rnd)),
                             [&] { latch.CountDown(); });
    });
    latch.Await();
  }
  
  {
    Latch latch(n);
    std::uniform_int_distribution<int> dist(500, 2000);
    bench.run("RandomDelay(500, 2000)", [&] {
      executor.ScheduleAfter(Duration::Milliseconds(dist(rnd)),
                             [&] { latch.CountDown(); });
    });
    latch.Await();
  }
  
  {
    Latch latch(n);
    std::uniform_int_distribution<int> dist(500, 1000);
    bench.run("RandomDelay(500, 5000)", [&] {
      executor.ScheduleAfter(Duration::Milliseconds(dist(rnd)),
                             [&] { latch.CountDown(); });
    });
    latch.Await();
  }

  {
    Latch latch(n);
    bench.run("Delay(1000)", [&] {
      executor.ScheduleAfter(Duration::Milliseconds(1000),
                             [&] { latch.CountDown(); });
    });
    latch.Await();
  }

  executor.Close();
}

int main() {
  pedronet::logger::SetLevel(Logger::Level::kTrace);

  EventLoop::Options options{};
  options.event_queue_type = EventQueueType::kLockFreeQueue;
  options.timer_queue_type = TimerQueueType::kHeap;
  options.selector_type = pedronet::SelectorType::kEpoll;

  options.timer_queue_type = TimerQueueType::kHeap;
  benchmark(options, "HeapQueue");

  options.timer_queue_type = TimerQueueType::kHashWheel;
  benchmark(options, "HashTimeWheel");

  return 0;
}