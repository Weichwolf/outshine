#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGBASIS_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDBUILDINGBASIS_H

#include "HeightField.h"
#include "GeographicRing.h"
#include "SourceProvenance.h"
#include "Capacity.h"
#include <math/Vec3.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine::Generators {
struct PreparedStructureTile;

struct PreparedBuildingBasis {
  struct Source {
    uint32_t Cell = 0;
    Data::SourceObjectId Id;
  };

  std::vector<double> PointsLatLon;
  std::vector<GeographicRing> Holes;
  std::vector<Source> Sources;
  Data::ProductOrigin Origin;
  std::vector<Data::TileSourceIdentity> HeightSources;
  ::outshine::Ground::HeightField::Request HeightRequest;
  uint64_t HeightRasterDigest = 0;
  bool HeightQualified = false;
  Vec3 AnchorEcef;
  double TileSpanM = 0.0;
  int Extent = 4096;
  bool FallbackHeights = false;
  size_t SkippedRings = 0;
  int NoGround = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = sizeof(*this) + CapacityBytes(PointsLatLon) + CapacityBytes(Holes) +
                   CapacityBytes(Sources) + CapacityBytes(HeightSources) +
                   CapacityBytes(HeightRequest.Tiles);
    for (const auto &source : HeightSources) {
      bytes += source.SourceId.capacity() + 1u + source.Revision.capacity() + 1u;
    }
    return bytes;
  }

  [[nodiscard]] static PreparedBuildingBasis Of(const PreparedStructureTile &base);
};
}
#endif
