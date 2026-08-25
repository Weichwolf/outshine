#include "VersatilesVector.h"

#include <cstdio>

namespace outshine::Data {
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

  d.TypicalPayloadBytes = 80000;
  d.RetryBudget = 4;
  return d;
}

}

VersatilesVector::VersatilesVector() : WebTileSource(Declared()) {}

std::string VersatilesVector::Url(const Address &at) const {
  int z = 0;
  uint32_t x = 0, y = 0;
  if (!at.TryTile(&z, &x, &y)) return std::string();
  char url[128];
  std::snprintf(url, sizeof url, "https://tiles.versatiles.org/tiles/osm/%d/%u/%u", z, x, y);
  return std::string(url);
}

Meaning VersatilesVector::Classify(int status, size_t bytes) const noexcept {

  if (status == 200) return bytes > 0 ? Meaning::Bytes : Meaning::Retry;
  if (status == 404) return Meaning::Absent;
  if (status == 408 || status == 429 || status >= 500) return Meaning::Retry;
  return Meaning::Refused;
}

}
