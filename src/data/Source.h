#ifndef SOURCE_H
#define SOURCE_H

#include <cstdint>

#include "Fetched.h"
#include "Request.h"
#include "SourceDecl.h"
#include "Transport.h"

namespace outshine::Data {

/* Whether a request falls inside this source's declared domain. Not a fact about the world: a source
 * that covers a place may still find nothing there, and those two are the distinction the whole
 * layer exists for. */
enum class Coverage : uint8_t { Inside, Outside };

/* THE GENERATOR CONTRACT WITH THE ARROW REVERSED. A generator declares what it can propose before it
 * proposes anything and is registered at a rank that refuses a duplicate at registration; a source
 * declares what it covers before it fetches anything and is registered the same way. Both are
 * declared, both are replaceable, and neither may name the engine — a source that needed the world
 * to answer *what do you cover* would have the arrow the wrong way round.
 *
 * WHAT A SOURCE DOES NOT DO: decide who answers, or how often to try. Both are the selector's, over
 * Covers, the declared rank and the declared retry budget. */
class Source {
public:
  virtual ~Source() = default;

  [[nodiscard]] virtual const SourceDecl &Declaration() const noexcept = 0;

  /* PURE, allocation-free, no I/O — and the only thing that can mint the right to a world fact. */
  [[nodiscard]] virtual Coverage Covers(const Request &request) const noexcept = 0;

  /* Which address this source will actually answer a covered request from: itself, or the nearest
   * ancestor it serves natively. Pure, for the same reason Covers is, and it is what a Delivery
   * carries so that *what resolution answered* is a measurement rather than an assumption. */
  [[nodiscard]] virtual Address Serves(const Request &request) const noexcept = 0;

  /* Ticket::None when this source needs no transport at all — the star catalogue is generated in
   * this tree and has no upstream, and that is a declaration rather than a special case. */
  [[nodiscard]] virtual Ticket Begin(const Address &at, Transport &transport) const = 0;

  [[nodiscard]] virtual Fetched Collect(const Address &at, Ticket ticket,
                                        Transport &transport) const = 0;
};

} // namespace outshine::Data
#endif
