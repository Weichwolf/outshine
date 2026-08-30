#include "TerrariumDem.h"

#include <cstdio>

namespace outshine::Data {
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

}

TerrariumDem::TerrariumDem() : WebTileSource(Declared()) {}

std::string TerrariumDem::Url(const Address &at) const {
  int z = 0;
  uint32_t x = 0, y = 0;
  if (!at.TryTile(&z, &x, &y)) { return std::string(); }
  char url[160];
  std::snprintf(url,
                sizeof url,
                "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%u/%u.png",
                z,
                x,
                y);
  return std::string(url);
}

Meaning TerrariumDem::Classify(int status, size_t bytes) const noexcept {
  if (status == 200) { return bytes > 0 ? Meaning::Bytes : Meaning::Retry; }

  if (status == 403 || status == 404) { return Meaning::Absent; }
  if (status == 408 || status == 429 || status >= 500) { return Meaning::Retry; }
  return Meaning::Refused;
}

}
