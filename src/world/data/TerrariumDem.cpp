#include "TerrariumDem.h"

#include <cstdint>
#include <optional>
#include <format>
#include <string_view>

#include <cstdio>

namespace outshine::Data {

constexpr size_t kTypicalPayloadBytes = 60000;

namespace Says {
inline constexpr std::string_view kTile =
    "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{}/{}/{}.png";
}

namespace {

[[nodiscard]] SourceDecl Declared() {
  SourceDecl d;
  d.Id = "terrarium.s3";
  d.Version = 1;
  d.Kind = DataKind::Elevation;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::TerrariumPng;
  d.Order = Rank{0};
  d.MinZoom = 0;

  d.MaxZoom = 15;
  d.AncestorFill = true;
  d.Keeps = Cacheability::Forever;
  d.Need = Necessity::Required;
  d.Latency = LatencyClass::Distant;

  d.TypicalPayloadBytes = kTypicalPayloadBytes;
  d.RetryBudget = 4;
  return d;
}

} // namespace

TerrariumDem::TerrariumDem() : WebTileSource(Declared()) {}

std::string TerrariumDem::Url(const Address &at) const {
  const std::optional<TileId> tile = at.Tile();
  if (!tile) { return {}; }
  return std::format(Says::kTile, tile->Zoom, tile->X, tile->Y);
}

bool TerrariumDem::CountsAbsent(int status) const noexcept {
  return status == kHttpForbidden || status == kHttpNotFound;
}

} // namespace outshine::Data
