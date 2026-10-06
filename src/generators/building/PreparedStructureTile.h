#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTURETILE_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTURETILE_H

#include "StructureBake.h"
#include "BuildingSurface.h"

namespace outshine::Generators {

struct PreparedStructure {
  RawTile::Structure Layout;
  ::outshine::Ground::BuildingFootprint Standing;
  ::outshine::Ground::GeoBounds Bounds;
  size_t CornerFirst = 0;
  double BaseAslM = 0.0, SeatAslM = 0.0;
  double AreaM2 = 0.0;
  double AcrossM = 0.0;
};

struct PreparedStructureTile {
  std::vector<double> PointsLatLon;
  std::vector<GeographicRing> Holes;
  std::vector<double> CornerAslM;
  std::vector<PreparedStructure> Structures;
  std::vector<BuildingSurface> Surfaces;
  Data::ProductOrigin Origin;
  Vec3 AnchorEcef;
  double TileSpanM = 0.0;
  int Extent = 4096;
  bool FallbackHeights = false;
  size_t SkippedRings = 0;
  int NoGround = 0;
};

[[nodiscard]] std::expected<PreparedStructureTile, StructureBakeError>
PrepareStructureTile(const RawTile &raw,
                     const ::outshine::Ground::HeightField &heights,
                     const std::atomic_bool *stopping = nullptr);

[[nodiscard]] std::expected<BakedTile, StructureBakeError>
BakePreparedStructures(const PreparedStructureTile &base,
                       const RawTile &view,
                       const StructureMesher &mesher,
                       MeshScratch &scratch,
                       const std::atomic_bool *stopping = nullptr);

}
#endif
