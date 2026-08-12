#include "DelayedTransport.h"

namespace outshine::Host {

int DelayedTransport::DelayMs(const std::string &url, uint32_t seed, int spreadMs) {
  if (spreadMs <= 0) return 0;
  uint32_t hash = 2166136261u ^ seed;
  for (char c : url) {
    hash ^= (uint32_t)(uint8_t)c;
    hash *= 16777619u;
  }
  return (int)(hash % (uint32_t)(spreadMs + 1));
}

Data::Ticket DelayedTransport::Begin(const std::string &url) {
  const Data::Ticket ticket = Under_.Begin(url);
  if (ticket == Data::Ticket::None) return ticket;
  const auto release = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(DelayMs(url, Config_.Seed, Config_.SpreadMs));
  std::lock_guard<std::mutex> lock(Mutex_);
  Release_[(uint64_t)ticket] = release;
  return ticket;
}

Data::Wire DelayedTransport::Collect(Data::Ticket ticket) {
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    const auto held = Release_.find((uint64_t)ticket);
    /* THE ANSWER IS HELD, NOT THE REQUEST. The transfer underneath runs at full speed; what this
     * decides is which of several finished answers the caller is allowed to see first. */
    if (held != Release_.end() && std::chrono::steady_clock::now() < held->second)
      return Data::Wire::Working();
  }
  Data::Wire wire = Under_.Collect(ticket);
  if (wire.Where() == Data::Wire::State::Working) return wire;
  std::lock_guard<std::mutex> lock(Mutex_);
  Release_.erase((uint64_t)ticket);
  return wire;
}

void DelayedTransport::Cancel(Data::Ticket ticket) {
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    Release_.erase((uint64_t)ticket);
  }
  Under_.Cancel(ticket);
}

} // namespace outshine::Host
