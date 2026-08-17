/* THE ORDER THE ANSWERS COME BACK IN, AS AN INPUT. Every request goes to the transport underneath
 * unchanged and its answer is held back by a delay derived from the URL and a seed, so one seed is
 * one arrival order and the same seed is the same order again. Nothing here is a fixture: the bytes
 * and the status are the upstream's own.
 *
 * It exists because a gate cannot sample what it does not control — the host decides the completion
 * order, and on a warm path it decided the same one six times running. This was an HTTP proxy in
 * another language for as long as there was a process boundary to impose it at; in one process the
 * order is ours, and a decorator over the host seam is the whole of it. */
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
    /* The widest a single answer may be held. The delay is uniform over [0, Spread]. */
    int SpreadMs = 400;
  };

  DelayedTransport(Data::Transport &under, const Config &config) : Under_(under), Config_(config) {}

  [[nodiscard]] Data::Ticket Begin(const std::string &url) override;
  [[nodiscard]] Data::Wire Collect(Data::Ticket ticket) override;
  void Cancel(Data::Ticket ticket) override;

  /* FNV-1a over the URL, so the delay is a property of WHICH thing is asked for and of the seed — a
   * request repeated after a retry must not overtake itself. */
  [[nodiscard]] static int DelayMs(const std::string &url, uint32_t seed, int spreadMs);

private:
  Data::Transport &Under_;
  Config Config_;
  std::mutex Mutex_;
  std::map<uint64_t, std::chrono::steady_clock::time_point> Release_;
};

} // namespace outshine::Host
#endif
