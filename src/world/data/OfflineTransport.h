#ifndef OUTSHINE_WORLD_DATA_OFFLINETRANSPORT_H
#define OUTSHINE_WORLD_DATA_OFFLINETRANSPORT_H

#include "Transport.h"

namespace outshine::Data {

class OfflineTransport final : public Transport {
public:
  [[nodiscard]] Ticket Begin(const std::string &url) override {
    (void)url;
    return Ticket::None;
  }

  [[nodiscard]] Wire Collect(Ticket ticket) override {
    (void)ticket;
    return Wire::Never();
  }

  void Cancel(Ticket ticket) override { (void)ticket; }

  [[nodiscard]] bool Await(double waitMs) override {
    (void)waitMs;
    return false;
  }
};

}

#endif
