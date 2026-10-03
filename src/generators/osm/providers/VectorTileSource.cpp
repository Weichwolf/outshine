#include "VectorTileSource.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {

using namespace outshine::Data;

constexpr size_t kTypicalPayloadBytes = 80000;

namespace {

[[nodiscard]] SourceDecl Declared(std::string revision,
                                  Rank order,
                                  AbsencePolicy absence,
                                  std::string dataset,
                                  std::string endpoint,
                                  MvtSchema schema) {
  SourceDecl d;
  d.Id = std::move(dataset);
  d.Version = schema == MvtSchema::Shortbread ? 1 : 2;
  d.Schema = schema == MvtSchema::Shortbread ? "shortbread" : "openmaptiles";
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

VectorTileSource::VectorTileSource(std::string revision,
                                   Rank order,
                                   AbsencePolicy absence,
                                   std::string dataset,
                                   const std::string &endpoint,
                                   MvtSchema schema)
    : WebTileSource(
          Declared(std::move(revision), order, absence, std::move(dataset), endpoint, schema),
          endpoint) {}

}
