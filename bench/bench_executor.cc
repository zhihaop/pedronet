#include <pedrolib/logger/logger.h>
#include <pedronet/eventloop.h>
#include <pedronet/selector/epoller.h>
#include <future>

using pedrolib::Logger;
using pedronet::EpollSelector;
using pedronet::EventLoop;

int main() {
  EventLoop executor(std::make_unique<EpollSelector>());

  Logger logger("bench");
  logger.SetLevel(Logger::Level::kTrace);
  auto defer = std::async(std::launch::async, [&] { executor.Loop(); });

  int n = 1e6 + 5;
  std::atomic<int> counter = 0;

  logger.Info("bench start");
  for (int i = 0; i < n; ++i) {
    executor.Schedule([&] { counter++; });
  }
  logger.Info("bench end");

  executor.Close();
  return 0;
}

