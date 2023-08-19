#ifndef PEDRONET_EVENTLOOPGROUP_H
#define PEDRONET_EVENTLOOPGROUP_H

#include "pedronet/eventloop.h"
#include "pedronet/selector/epoller.h"

#include <pedrolib/collection/static_vector.h>
#include <atomic>
#include <thread>

namespace pedronet {

class EventLoopGroup : public Executor {

  size_t next() noexcept;

  void HandleJoin();

 public:
  using Ptr = std::shared_ptr<EventLoopGroup>;

  explicit EventLoopGroup(size_t threads)
      : loops_(threads), threads_(threads), size_(threads), next_(0) {}

  static EventLoopGroup::Ptr Create() {
    return Create(std::thread::hardware_concurrency());
  }

  template <typename... Executor>
  static void Joins(Executor&&... executor) {
    (executor->Join(), ...);
  }

  [[nodiscard]] size_t Size() const noexcept override;

  static EventLoopGroup::Ptr Create(size_t threads) {
    return Create(threads, {});
  }

  static EventLoopGroup::Ptr Create(size_t threads,
                                    const EventLoopOptions& options) {
    auto group = std::make_shared<EventLoopGroup>(threads);

    for (size_t i = 0; i < threads; ++i) {
      group->loops_.emplace_back(options);
    }
    for (size_t i = 0; i < threads; ++i) {
      auto& loop = group->loops_[i];
      group->threads_.emplace_back([&loop] { loop.Loop(); });
    }
    return group;
  }

  ~EventLoopGroup() override {
    Close();
    HandleJoin();
  }

  EventLoop& Next() { return loops_[next()]; }

  void Join() override;

  void Schedule(Callback cb) override { Next().Schedule(std::move(cb)); }

  uint64_t ScheduleAfter(Duration delay, Callback cb) override;

  uint64_t ScheduleEvery(Duration delay, Duration interval,
                         Callback cb) override;

  void ScheduleCancel(uint64_t id) override;

  void Close() override;

 private:
  pedrolib::StaticVector<EventLoop> loops_;
  pedrolib::StaticVector<std::thread> threads_;
  std::atomic_size_t next_;
  const size_t size_;
};

}  // namespace pedronet

#endif  // PEDRONET_EVENTLOOPGROUP_H
