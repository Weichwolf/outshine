#include "TerrariumDem.h"

#include <cstdint>
#include <format>
#include <string_view>

#include <cstdio>

namespace outshine::Data {

constexpr int kHttpOk = 200;
constexpr int kHttpForbidden = 403;
constexpr int kHttpNotFound = 404;
constexpr int kHttpTimeout = 408;
constexpr int kHttpTooMany = 429;
constexpr int kHttpServerFirst = 500;
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
  int z = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  if (!at.TryTile(&z, &x, &y)) { return {}; }
  return std::format(Says::kTile, z, x, y);
}

Meaning TerrariumDem::Classify(int status, size_t bytes) const noexcept {
  if (status == kHttpOk) { return bytes > 0 ? Meaning::Bytes : Meaning::Retry; }

  if (status == kHttpForbidden || status == kHttpNotFound) { return Meaning::Absent; }
  if (status == kHttpTimeout || status == kHttpTooMany || status >= kHttpServerFirst) {
    return Meaning::Retry;
  }
  return Meaning::Refused;
}

} // namespace outshine::Data
