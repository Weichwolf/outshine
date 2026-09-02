#include "VersatilesVector.h"

#include <cstdint>
#include <format>
#include <string_view>

#include <cstdio>

namespace outshine::Data {

constexpr int kHttpOk = 200;
constexpr int kHttpNotFound = 404;
constexpr int kHttpTimeout = 408;
constexpr int kHttpTooMany = 429;
constexpr int kHttpServerFirst = 500;
constexpr size_t kTypicalPayloadBytes = 80000;

namespace Says {
inline constexpr std::string_view kTile = "https://tiles.versatiles.org/tiles/osm/{}/{}/{}";
}

namespace {

[[nodiscard]] SourceDecl Declared() {
  SourceDecl d;
  d.Id = "versatiles.osm";
  d.Version = 1;
  d.Kind = DataKind::VectorMap;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::MapboxVectorTile;
  d.Order = Rank{0};
  d.MinZoom = 0;
  d.MaxZoom = 14;
  d.AncestorFill = false;
  d.Keeps = Cacheability::Forever;
  d.Need = Necessity::Required;
  d.Latency = LatencyClass::Distant;

  d.TypicalPayloadBytes = kTypicalPayloadBytes;
  d.RetryBudget = 4;
  return d;
}

} // namespace

VersatilesVector::VersatilesVector() : WebTileSource(Declared()) {}

std::string VersatilesVector::Url(const Address &at) const {
  int z = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  if (!at.TryTile(&z, &x, &y)) { return {}; }
  return std::format(Says::kTile, z, x, y);
}

Meaning VersatilesVector::Classify(int status, size_t bytes) const noexcept {
  if (status == kHttpOk) { return bytes > 0 ? Meaning::Bytes : Meaning::Retry; }
  if (status == kHttpNotFound) { return Meaning::Absent; }
  if (status == kHttpTimeout || status == kHttpTooMany || status >= kHttpServerFirst) {
    return Meaning::Retry;
  }
  return Meaning::Refused;
}

} // namespace outshine::Data
