#include "WebTileSource.h"
#include <cstdint>
#include <vector>
#include <utility>

namespace outshine::Data {
namespace {

constexpr int kDeepestTileZoom = 30;

}

Coverage WebTileSource::Covers(const Request &request) const noexcept {
  if (request.Kind() != Decl_.Kind) { return Coverage::Outside; }
  int z = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  if (!request.Where().TryTile(&z, &x, &y)) { return Coverage::Outside; }
  if (z < Decl_.MinZoom || z > kDeepestTileZoom) { return Coverage::Outside; }
  if (z > Decl_.MaxZoom && !Decl_.AncestorFill) { return Coverage::Outside; }
  const uint32_t side = 1u << static_cast<uint32_t>(z);
  if (x >= side || y >= side) { return Coverage::Outside; }
  return Coverage::Inside;
}

Address WebTileSource::Serves(const Request &request) const noexcept {
  int z = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  if (!request.Where().TryTile(&z, &x, &y)) { return request.Where(); }
  if (z <= Decl_.MaxZoom) { return request.Where(); }
  const int steps = z - Decl_.MaxZoom;
  return Address::Tile(
      Decl_.MaxZoom, x >> static_cast<uint32_t>(steps), y >> static_cast<uint32_t>(steps));
}

Ticket WebTileSource::Begin(const Address &at, Transport &transport) const {
  return transport.Begin(Url(at));
}

Fetched WebTileSource::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  (void)at;
  Wire wire = transport.Collect(ticket);
  switch (wire.Where()) {
    case Wire::State::Working: return Fetched::Working();

    case Wire::State::Unreachable: return Fetched::Meant(Meaning::Retry);
    case Wire::State::Never: return Fetched::Meant(Meaning::Refused);
    case Wire::State::Answered: break;
  }
  int status = 0;
  std::vector<uint8_t> body;
  if (!wire.TryTake(&status, &body)) { return Fetched::Meant(Meaning::Refused); }
  const Meaning what = Classify(status, body.size());
  if (what != Meaning::Bytes) { return Fetched::MeantAfter(what, wire.RetryAfterS()); }
  return Fetched::Delivered(std::move(body));
}

} // namespace outshine::Data
