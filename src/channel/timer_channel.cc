#include "pedronet/channel/timer_channel.h"
#include <sys/timerfd.h>
#include "pedronet/logger/logger.h"

namespace pedronet {

inline static File CreateTimerFile() {
  int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd <= 0) {
    PEDRONET_FATAL("failed to create timer fd");
  }
  return File{fd};
}

TimerChannel::TimerChannel() : Channel(), file_(CreateTimerFile()) {}

void TimerChannel::HandleEvents(ReceiveEvents, Timestamp) {
  last_wakeup_us_ = std::numeric_limits<int64_t>::max();
  
  uint64_t val;
  if (std::lock_guard guard{mu_}; file_.Read(&val, sizeof(val)) != sizeof(val)) {
    PEDRONET_WARN("failed to read timer fd: {}", file_.GetError());
  }
  if (event_callback_) {
    event_callback_();
  }
}

void TimerChannel::WakeUpAfter(Duration duration) {
  duration = std::max(duration, kMinWakeUpDuration);

  struct itimerspec u {};
  struct itimerspec v {};
  int64_t usec = duration.Microseconds();

  for (;;) {
    int64_t us = last_wakeup_us_.load();
    if (usec >= us) {
      return;
    }

    if (last_wakeup_us_.compare_exchange_strong(us, usec)) {
      break;
    }
  }

  v.it_value.tv_sec = usec / Duration::kMicroseconds;
  v.it_value.tv_nsec = (usec % Duration::kMicroseconds) * 1000;

  std::lock_guard guard{mu_};
  if (::timerfd_settime(file_.Descriptor(), 0, &v, &u) < 0) {
    PEDRONET_WARN("failed to set timerfd time");
  }
}

std::string TimerChannel::String() const {
  return fmt::format("TimerChannel[fd={}]", file_.Descriptor());
}
}  // namespace pedronet