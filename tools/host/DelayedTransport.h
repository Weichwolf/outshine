#ifndef DELAYEDTRANSPORT_H
#define DELAYEDTRANSPORT_H

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#include "Transport.h"

namespace outshine::Host {

class DelayedTransport : public Data::Transport {
public:
  struct Config {
    uint32_t Seed = 1;

    int SpreadMs = 400;
  };

  DelayedTransport(Data::Transport &under, const Config &config) : Under_(under), Config_(config) {}

  [[nodiscard]] Data::Ticket Begin(const std::string &url) override;
  [[nodiscard]] Data::Wire Collect(Data::Ticket ticket) override;
  void Cancel(Data::Ticket ticket) override;

  [[nodiscard]] static int DelayMs(const std::string &url, uint32_t seed, int spreadMs);

private:
  Data::Transport &Under_;
  Config Config_;
  std::mutex Mutex_;
  std::map<uint64_t, std::chrono::steady_clock::time_point> Release_;
};

}
#endif
