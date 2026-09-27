#include "VersatilesVector.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Data {

constexpr size_t kTypicalPayloadBytes = 80000;

namespace Says {
inline constexpr std::string_view kTile = "https://tiles.versatiles.org/tiles/osm/{z}/{x}/{y}";
}

namespace {

[[nodiscard]] SourceDecl Declared(std::string revision,
                                  Rank order,
                                  AbsencePolicy absence,
                                  std::string dataset,
                                  std::string endpoint) {
  SourceDecl d;
  d.Id = dataset.empty() ? "versatiles.osm" : std::move(dataset);
  d.Version = 1;
  d.Revision = std::move(revision);
  d.Endpoint = std::move(endpoint);
  d.Kind = DataKind::VectorMap;
  d.How = Scheme::TileZxy;
  d.Wire = WireFormat::MapboxVectorTile;
  d.Order = order;
  d.OnAbsent = absence;
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

}

VersatilesVector::VersatilesVector(std::string revision,
                                   Rank order,
                                   AbsencePolicy absence,
                                   std::string dataset,
                                   const std::string &endpoint)
    : WebTileSource(Declared(std::move(revision), order, absence, std::move(dataset), endpoint),
                    endpoint.empty() ? std::string(Says::kTile) : endpoint) {}

}
