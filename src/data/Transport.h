#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Data {

/* WHAT THE LIBRARY NEEDS FROM A HOST TO REACH AN UPSTREAM, and the whole of it. No implementation of
 * this lives under src/: the library declares the seam and a host supplies the wire, which is why no
 * transport library has a symbol in this tree's archives.
 *
 * ASYNCHRONOUS BY SHAPE. Begin yields a ticket, the caller collects, Cancel is a real operation —
 * there is no spelling for "fetch this and block until it arrives", because that spelling is the one
 * property that cannot survive a host whose frame thread may not wait. */
enum class Ticket : uint64_t { None = 0 };

/* WHAT THE WIRE DID, and the four readings of one empty buffer are four states here instead. The
 * body is reachable only from Answered. */
class Wire {
public:
  enum class State { Working, Answered, Unreachable };

  static Wire Working() { return Wire(State::Working, 0, {}); }
  static Wire Answered(int status, std::vector<uint8_t> body) {
    return Wire(State::Answered, status, std::move(body));
  }
  /* No answer to have: the name did not resolve, the connection failed, the transfer was aborted.
   * Distinct from an answered 5xx, which is a server that spoke. */
  static Wire Unreachable() { return Wire(State::Unreachable, 0, {}); }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  /* Hands the status and the body over exactly once and only when there was one. */
  [[nodiscard]] bool TryTake(int *status, std::vector<uint8_t> *body) {
    if (Where_ != State::Answered) return false;
    *status = Status_;
    *body = std::move(Body_);
    return true;
  }

private:
  Wire(State where, int status, std::vector<uint8_t> body)
      : Where_(where), Status_(status), Body_(std::move(body)) {}

  State Where_;
  int Status_;
  std::vector<uint8_t> Body_;
};

class Transport {
public:
  virtual ~Transport() = default;

  /* Ticket::None when the transport could not even start; Collect on it answers Unreachable. */
  [[nodiscard]] virtual Ticket Begin(const std::string &url) = 0;
  /* Working until the answer is there, then Answered or Unreachable exactly once — the ticket is
   * spent by the first non-Working collect and must not be collected again. */
  [[nodiscard]] virtual Wire Collect(Ticket ticket) = 0;
  virtual void Cancel(Ticket ticket) = 0;
};

} // namespace outshine::Data
#endif
