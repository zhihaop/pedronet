#ifndef PEDRONET_CHANNEL_TIMED_CHANNEL_H
#define PEDRONET_CHANNEL_TIMED_CHANNEL_H

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/selector/selector.h"

namespace pedronet {

class TimerChannel final : public Channel {
 public:
  using Ptr = std::shared_ptr<TimerChannel>;

  TimerChannel();
  ~TimerChannel() override = default;

  void SetEventCallBack(Callback cb) { event_callback_ = std::move(cb); }

  void HandleEvents(ReceiveEvents events, Timestamp now) override;

  File& GetFile() noexcept override { return file_; }

  [[nodiscard]] const File& GetFile() const noexcept override { return file_; }

  [[nodiscard]] std::string String() const override;

  void WakeUpAt(Timestamp timestamp) {
    WakeUpAfter(timestamp - Timestamp::Now());
  }

  void WakeUpAfter(Duration duration);

 private:
  inline static const Duration kMinWakeUpDuration = Duration::Microseconds(100);

  Callback event_callback_;
  std::atomic_int64_t last_wakeup_us_{std::numeric_limits<int64_t>::max()};
  File file_;
};

}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::TimerChannel);
#endif  // PEDRONET_CHANNEL_TIMED_CHANNEL_H