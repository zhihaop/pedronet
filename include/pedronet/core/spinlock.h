#ifndef PEDRONET_CORE_SPINLOCK_H
#define PEDRONET_CORE_SPINLOCK_H

#include <atomic>
#include <thread>

namespace pedronet {
class alignas(64) SpinLock {
  std::atomic_flag lock_{};
  std::array<char, 64 - sizeof(std::atomic_flag)> padding_;
  
 public:
  void lock() noexcept {
    size_t n = 0;
    while (lock_.test_and_set()) {
      if ((++n & 127) == 0) {
        std::this_thread::yield();
      }
    }
  }

  void unlock() noexcept { lock_.clear(); }
};
}  // namespace pedronet
#endif  // PEDRONET_CORE_SPINLOCK_H
