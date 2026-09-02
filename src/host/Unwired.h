#ifndef OUTSHINE_HOST_UNWIRED_H
#define OUTSHINE_HOST_UNWIRED_H

#include "Transport.h"

namespace outshine {

class Unwired final : public Data::Transport {
public:
  [[nodiscard]] Data::Ticket Begin([[maybe_unused]] const std::string &url) override {
    return Data::Ticket::None;
  }

  [[nodiscard]] Data::Wire Collect([[maybe_unused]] Data::Ticket ticket) override {
    return Data::Wire::Never();
  }

  void Cancel([[maybe_unused]] Data::Ticket ticket) override {}
};

} // namespace outshine
#endif
