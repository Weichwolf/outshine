#include "VersatilesVector.h"

#include <format>
#include <string_view>

#include <cstdio>

namespace outshine::Data {

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

  d.TypicalPayloadBytes = 80000;
  d.RetryBudget = 4;
  return d;
}

}

VersatilesVector::VersatilesVector() : WebTileSource(Declared()) {}

std::string VersatilesVector::Url(const Address &at) const {
  int z = 0;
  uint32_t x = 0, y = 0;
  if (!at.TryTile(&z, &x, &y)) { return std::string(); }
  return std::format(Says::kTile, z, x, y);
}

Meaning VersatilesVector::Classify(int status, size_t bytes) const noexcept {
  if (status == 200) { return bytes > 0 ? Meaning::Bytes : Meaning::Retry; }
  if (status == 404) { return Meaning::Absent; }
  if (status == 408 || status == 429 || status >= 500) { return Meaning::Retry; }
  return Meaning::Refused;
}

}
