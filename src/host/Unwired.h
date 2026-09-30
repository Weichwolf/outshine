#ifndef OUTSHINE_HOST_UNWIRED_H
#define OUTSHINE_HOST_UNWIRED_H

#include <world/data/Transport.h>
#include <expected>

namespace outshine {

class Unwired final : public Data::Transport {
public:
  [[nodiscard]] Data::FetchStart Begin([[maybe_unused]] const std::string &url) override {
    return std::unexpected(Data::FetchFailureReason::Unavailable);
  }

  [[nodiscard]] Data::Wire Collect([[maybe_unused]] Data::Ticket ticket) override {
    return Data::Wire::Never();
  }

  void Cancel([[maybe_unused]] Data::Ticket ticket) override {}
};

}
#endif
