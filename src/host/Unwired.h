#ifndef OUTSHINE_HOST_UNWIRED_H
#define OUTSHINE_HOST_UNWIRED_H

#include "Transport.h"

namespace outshine {

class Unwired final : public Data::Transport {
public:
  [[nodiscard]] Data::Ticket Begin(const std::string &) override { return Data::Ticket::None; }

  [[nodiscard]] Data::Wire Collect(Data::Ticket) override { return Data::Wire::Never(); }

  void Cancel(Data::Ticket) override {}
};

} // namespace outshine
#endif
