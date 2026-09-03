#include "VersatilesVector.h"

#include <cstdint>
#include <optional>
#include <format>
#include <string_view>

#include <cstdio>

namespace outshine::Data {

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
  const std::optional<TileId> tile = at.Tile();
  if (!tile) { return {}; }
  return std::format(Says::kTile, tile->Zoom, tile->X, tile->Y);
}

} // namespace outshine::Data
