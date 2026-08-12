#ifndef WEBTILESOURCE_H
#define WEBTILESOURCE_H

#include <string>
#include <utility>

#include "Source.h"

namespace outshine::Data {

/* WHAT EVERY SLIPPY-TILE UPSTREAM SHARES: the coverage predicate is its declared zoom band and the
 * pyramid, the address it serves is itself or its nearest declared ancestor, and one GET carries the
 * answer. What differs is the URL it forms and what a status means to it, and those two are the only
 * things a concrete source writes.
 *
 * IMPLEMENTATION INHERITANCE, deliberately: `Source` is the interface, this is the shared body, and a
 * source whose scheme is a bounding-box query derives from `Source` directly rather than bending
 * this one. */
class WebTileSource : public Source {
public:
  [[nodiscard]] const SourceDecl &Declaration() const noexcept final { return Decl_; }
  [[nodiscard]] Coverage Covers(const Request &request) const noexcept final;
  [[nodiscard]] Address Serves(const Request &request) const noexcept final;
  [[nodiscard]] Ticket Begin(const Address &at, Transport &transport) const final;
  [[nodiscard]] Fetched Collect(const Address &at, Ticket ticket,
                                Transport &transport) const final;

protected:
  explicit WebTileSource(SourceDecl decl) : Decl_(std::move(decl)) {}

  /* The upstream URL template is this source's own business and appears exactly once — here. */
  [[nodiscard]] virtual std::string Url(const Address &at) const = 0;
  [[nodiscard]] virtual Meaning Classify(int status, size_t bytes) const noexcept = 0;

private:
  SourceDecl Decl_;
};

} // namespace outshine::Data
#endif
