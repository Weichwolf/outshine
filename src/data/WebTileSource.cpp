#include "WebTileSource.h"

namespace outshine::Data {
namespace {

/* Above this a tile index no longer fits the 32 bits an address carries, and no upstream in this
 * tree serves anything near it. */
constexpr int kDeepestTileZoom = 30;

} // namespace

Coverage WebTileSource::Covers(const Request &request) const noexcept {
  if (request.Kind() != Decl_.Kind) return Coverage::Outside;
  int z = 0;
  uint32_t x = 0, y = 0;
  if (!request.Where().TryTile(&z, &x, &y)) return Coverage::Outside;
  if (z < Decl_.MinZoom || z > kDeepestTileZoom) return Coverage::Outside;
  if (z > Decl_.MaxZoom && !Decl_.AncestorFill) return Coverage::Outside;
  const uint32_t side = 1u << z;
  if (x >= side || y >= side) return Coverage::Outside;
  return Coverage::Inside;
}

Address WebTileSource::Serves(const Request &request) const noexcept {
  int z = 0;
  uint32_t x = 0, y = 0;
  if (!request.Where().TryTile(&z, &x, &y)) return request.Where();
  if (z <= Decl_.MaxZoom) return request.Where();
  const int steps = z - Decl_.MaxZoom;
  return Address::Tile(Decl_.MaxZoom, x >> steps, y >> steps);
}

Ticket WebTileSource::Begin(const Address &at, Transport &transport) const {
  return transport.Begin(Url(at));
}

Fetched WebTileSource::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  (void)at;
  Wire wire = transport.Collect(ticket);
  switch (wire.Where()) {
    case Wire::State::Working:
      return Fetched::Working();
    /* NO ANSWER TO HAVE is not a statement about the world and never becomes one: a name that did
     * not resolve or a connection that dropped is the wire, and the wire heals. */
    case Wire::State::Unreachable:
      return Fetched::Meant(Meaning::Retry);
    case Wire::State::Answered:
      break;
  }
  int status = 0;
  std::vector<uint8_t> body;
  if (!wire.TryTake(&status, &body)) return Fetched::Meant(Meaning::Refused);
  const Meaning what = Classify(status, body.size());
  if (what != Meaning::Bytes) return Fetched::Meant(what);
  return Fetched::Delivered(std::move(body));
}

} // namespace outshine::Data
