#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>

#include <future>
#include <iostream>

using namespace std::chrono_literals;
using pedrolib::Duration;
using pedrolib::Logger;
using pedronet::EpollSelector;
using pedronet::EventLoop;
using pedronet::EventLoopGroup;

int main() {
  pedronet::logger::SetLevel(Logger::Level::kInfo);

  Logger logger("test");
  logger.SetLevel(Logger::Level::kTrace);

  EventLoop executor;
  auto defer = std::async(std::launch::async, [&] { executor.Loop(); });
  
  executor.ScheduleAfter(1s, [&] {
    logger.Info("start after 1s");
  });
  
  for (int i = 0; i < 5; ++i) {
    executor.Schedule([&, i] { logger.Info("schedule {}", i); });
  }

  int i = 0;
  auto id = executor.ScheduleEvery(500ms, 500ms, [&] {
    logger.Info("hello world timer 1[{}]", ++i);
  });

  int j = 0;
  executor.ScheduleEvery(0ms, 1000ms, [&] {
    logger.Info("hello world timer 2[{}]", ++j);
    if (j == 5) {
      logger.Info("shutdown timer 1");
      executor.ScheduleCancel(id);
    }
    if (j == 10) {
      logger.Info("close");
      executor.Close();
    }
  });
  return 0;
}