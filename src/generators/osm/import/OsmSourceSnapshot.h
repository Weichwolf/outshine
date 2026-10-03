#ifndef OUTSHINE_GENERATORS_OSM_IMPORT_OSMSOURCESNAPSHOT_H
#define OUTSHINE_GENERATORS_OSM_IMPORT_OSMSOURCESNAPSHOT_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "OsmElementSet.h"
#include <world/SourceProvider.h>
#include <world/data/GeoCellId.h>

namespace outshine::Generators::Osm {

struct ChunkProvenance {
  std::string Location;
  std::string PayloadSha256;
  bool PinVerified = false;
  bool FromStore = false;
};

struct SourceSnapshot {
  ElementSet Elements;
  std::vector<Data::SourceCoverage> Coverage;
  size_t SourceBytes = 0;
  double ReadMs = 0.0;
  double ParseMs = 0.0;
  std::vector<ChunkProvenance> Chunks;
  std::optional<Data::GeoCellId> Cell = std::nullopt;

  [[nodiscard]] size_t StorageChargeBytes() const noexcept;
};

}

#endif
