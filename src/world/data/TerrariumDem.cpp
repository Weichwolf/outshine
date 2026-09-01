#include "TerrariumDem.h"

#include <cstdint>
#include <format>
#include <string_view>

#include <cstdio>

namespace outshine::Data {

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

  d.TypicalPayloadBytes = 60000;
  d.RetryBudget = 4;
  return d;
}

} // namespace

TerrariumDem::TerrariumDem() : WebTileSource(Declared()) {}

std::string TerrariumDem::Url(const Address &at) const {
  int z = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  if (!at.TryTile(&z, &x, &y)) { return {}; }
  return std::format(Says::kTile, z, x, y);
}

Meaning TerrariumDem::Classify(int status, size_t bytes) const noexcept {
  if (status == 200) { return bytes > 0 ? Meaning::Bytes : Meaning::Retry; }

  if (status == 403 || status == 404) { return Meaning::Absent; }
  if (status == 408 || status == 429 || status >= 500) { return Meaning::Retry; }
  return Meaning::Refused;
}

} // namespace outshine::Data
