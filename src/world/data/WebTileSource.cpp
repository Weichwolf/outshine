#include "WebTileSource.h"
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Data {
namespace {

constexpr int kDeepestTileZoom = 30;

}

Coverage WebTileSource::Covers(const Fetch &request) const noexcept {
  if (request.Kind() != Decl_.Kind) { return Coverage::Outside; }
  const std::optional<TileId> tile = request.Where().Tile();
  if (!tile) { return Coverage::Outside; }
  const auto [zoom, tileX, tileY] = *tile;
  if (zoom < Decl_.MinZoom || zoom > kDeepestTileZoom) { return Coverage::Outside; }
  if (zoom > Decl_.MaxZoom && !Decl_.AncestorFill) { return Coverage::Outside; }
  const uint32_t side = 1u << static_cast<uint32_t>(zoom);
  if (tileX >= side || tileY >= side) { return Coverage::Outside; }
  return Coverage::Inside;
}

Address WebTileSource::Serves(const Fetch &request) const noexcept {
  const std::optional<TileId> tile = request.Where().Tile();
  if (!tile) { return request.Where(); }
  const auto [zoom, tileX, tileY] = *tile;
  if (zoom <= Decl_.MaxZoom) { return request.Where(); }
  const int steps = zoom - Decl_.MaxZoom;
  return Address::At(TileId{.Zoom = Decl_.MaxZoom,
                            .X = tileX >> static_cast<uint32_t>(steps),
                            .Y = tileY >> static_cast<uint32_t>(steps)});
}

std::string WebTileSource::Url(const Address &at) const {
  const std::optional<TileId> tile = at.Tile();
  if (!tile) { return {}; }
  std::string url = Endpoint_;
  const auto replace = [&url](std::string_view token, uint32_t value) {
    const size_t at = url.find(token);
    url.replace(at, token.size(), std::to_string(value));
  };
  replace("{z}", static_cast<uint32_t>(tile->Zoom));
  replace("{x}", tile->X);
  replace("{y}", tile->Y);
  return url;
}

FetchStart WebTileSource::Begin(const Address &at, Transport &transport) const {
  return transport.Begin(Url(at));
}

Fetched WebTileSource::Collect(const Address &at, Ticket ticket, Transport &transport) const {
  (void)at;
  Wire wire = transport.Collect(ticket);
  switch (wire.Where()) {
    case Wire::State::Working: return Fetched::Working();

    case Wire::State::Unreachable: {
      const auto reason = wire.FailureReason();
      const bool terminal =
          reason == FetchFailureReason::Cancelled || reason == FetchFailureReason::CapacityRefused;
      return Fetched::Meant(terminal ? Meaning::Refused : Meaning::Retry, reason);
    }
    case Wire::State::Consumed:
    case Wire::State::Never: return Fetched::Meant(Meaning::Refused, wire.FailureReason());
    case Wire::State::Answered: break;
  }
  std::optional<Wire::Response> answered = wire.Take();
  if (!answered) { return Fetched::Meant(Meaning::Refused); }
  if (answered->Status == kHttpNotFound) { return Fetched::NotFound(); }
  const Meaning what = Classify({.Status = answered->Status, .Bytes = answered->Body.size()});
  if (what != Meaning::Bytes) {
    const auto reason = answered->Status == kHttpTimeout ? FetchFailureReason::TimedOut
                                                         : FetchFailureReason::ProviderRefused;
    return Fetched::MeantAfter(what, wire.RetryAfterS(), reason);
  }
  return Fetched::Delivered(std::move(answered->Body));
}

Meaning WebTileSource::Classify(Replied said) const noexcept {
  if (said.Status == kHttpOk) { return said.Bytes > 0 ? Meaning::Bytes : Meaning::Retry; }
  if (CountsAbsent(said.Status)) { return Meaning::Absent; }
  if (said.Status == kHttpTimeout || said.Status == kHttpTooMany ||
      said.Status >= kHttpServerFirst) {
    return Meaning::Retry;
  }
  return Meaning::Refused;
}

}
