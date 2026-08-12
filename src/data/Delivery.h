#ifndef DELIVERY_H
#define DELIVERY_H

#include <string>
#include <utility>
#include <vector>

#include "Address.h"

namespace outshine::Data {

/* WHAT THE REGISTRY ANSWERED, and *no provider here* and *no data here* are two of its states rather
 * than two readings of one empty buffer. Confusing them cost this tree two rounds:
 *
 *   Delivered  — bytes, from a named source, at the address that actually answered
 *   Pending    — a promise; ask again
 *   Vacant     — every source that declared this place inside its domain said there is nothing here.
 *                This is the only state that is a statement about the WORLD, and the only one a
 *                caller may cache as final
 *   Undeclared — no source covers this request at all. A declaration error, not a fact about Earth
 *   Refused    — this tree, the request or the wire is wrong. Never cacheable as an absence
 *
 * The payload is reachable only from Delivered, and so is the pair (who answered, at which address)
 * — a request at z15 served from a z14 ancestor says so here, which is what turns "what resolution
 * actually answered" from an assumption into a measurement. */
class Delivery {
public:
  enum class State { Delivered, Pending, Vacant, Undeclared, Refused };

  struct Answer {
    std::string SourceId;
    Address At = Address::Whole(0);
    std::vector<uint8_t> Bytes;
  };

  static Delivery From(std::string sourceId, Address at, std::vector<uint8_t> bytes) {
    Delivery d(State::Delivered);
    d.Answer_.SourceId = std::move(sourceId);
    d.Answer_.At = at;
    d.Answer_.Bytes = std::move(bytes);
    return d;
  }
  static Delivery Waiting() { return Delivery(State::Pending); }
  static Delivery Nothing() { return Delivery(State::Vacant); }
  static Delivery NoSource() { return Delivery(State::Undeclared); }
  static Delivery Wire() { return Delivery(State::Refused); }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  /* The one door. Hands the whole answer over, so a caller cannot read who answered without the
   * bytes that came with it. */
  [[nodiscard]] bool TryTake(Answer *out) {
    if (Where_ != State::Delivered) return false;
    *out = std::move(Answer_);
    return true;
  }

private:
  explicit Delivery(State where) : Where_(where) {}

  State Where_;
  Answer Answer_;
};

} // namespace outshine::Data
#endif
