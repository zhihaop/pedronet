#ifndef PEDRONET_SELECTOR_SELECTOR_FACTORY_H
#define PEDRONET_SELECTOR_SELECTOR_FACTORY_H

#include "pedronet/selector/epoller.h"

namespace pedronet {

enum class SelectorType { kEpoll };

inline static std::unique_ptr<Selector> MakeSelector(SelectorType type) {
  switch (type) {
    case SelectorType::kEpoll:
      return std::make_unique<EpollSelector>();
    default:
      return nullptr;
  }
}

}  // namespace pedronet

#endif  // PEDRONET_SELECTOR_SELECTOR_FACTORY_H
