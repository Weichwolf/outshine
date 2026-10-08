#include "PreparedBuildingBasis.h"
#include "PreparedStructureTile.h"

namespace outshine::Generators {
PreparedBuildingBasis PreparedBuildingBasis::Of(const PreparedStructureTile &base) {
  PreparedBuildingBasis basis{.PointsLatLon = base.PointsLatLon,
                              .Holes = base.Holes,
                              .Sources = {},
                              .Origin = base.Origin,
                              .HeightSources = base.HeightSources,
                              .HeightRequest = base.HeightRequest,
                              .HeightRasterDigest = base.HeightRasterDigest,
                              .HeightQualified = base.HeightQualified,
                              .AnchorEcef = base.AnchorEcef,
                              .TileSpanM = base.TileSpanM,
                              .Extent = base.Extent,
                              .FallbackHeights = base.FallbackHeights,
                              .SkippedRings = base.SkippedRings,
                              .NoGround = base.NoGround};
  basis.Sources.reserve(base.Structures.size());
  for (const auto &structure : base.Structures) {
    basis.Sources.push_back({.Cell = structure.Layout.Cell.Index, .Id = structure.Layout.SourceId});
  }
  return basis;
}
}
