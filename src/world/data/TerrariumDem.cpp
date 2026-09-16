#include "TerrariumDem.h"

#include <cstdint>
#include <optional>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <cstdio>

namespace outshine::Data {

constexpr size_t kTypicalPayloadBytes = 60000;

namespace Says {
inline constexpr std::string_view kTile =
    "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{}/{}/{}.png";
}

namespace {

[[nodiscard]] SourceDecl Declared(std::string revision, Rank order, AbsencePolicy absence) {
  SourceDecl d;
  d.Id = "terrarium.s3";
  d.Version = 1;
  d.Revision = std::move(revision);
  d.Kind = DataKind::Elevation;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::TerrariumPng;
  d.Order = order;
  d.OnAbsent = absence;
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

}

TerrariumDem::TerrariumDem(std::string revision, Rank order, AbsencePolicy absence)
    : WebTileSource(Declared(std::move(revision), order, absence)) {}

std::string TerrariumDem::Url(const Address &at) const {
  const std::optional<TileId> tile = at.Tile();
  if (!tile) { return {}; }
  return std::format(Says::kTile, tile->Zoom, tile->X, tile->Y);
}

bool TerrariumDem::CountsAbsent(int status) const noexcept {
  return status == kHttpForbidden || status == kHttpNotFound;
}

}
