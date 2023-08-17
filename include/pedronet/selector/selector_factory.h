#ifndef PEDRONET_SELECTOR_SELECTOR_FACTORY_H
#define PEDRONET_SELECTOR_SELECTOR_FACTORY_H

#include "pedronet/logger/logger.h"
#include "pedronet/selector/epoller.h"
#include "pedronet/selector/poller.h"
#include "pedronet/options.h"

namespace pedronet {

inline static std::unique_ptr<Selector> MakeSelector(SelectorType type) {
  switch (type) {
    case SelectorType::kEpoll:
      return std::make_unique<EpollSelector>();
    case SelectorType::kPoll:
      return std::make_unique<Poller>();
    default:
      return nullptr;
  }
}

}  // namespace pedronet

#endif  // PEDRONET_SELECTOR_SELECTOR_FACTORY_H
