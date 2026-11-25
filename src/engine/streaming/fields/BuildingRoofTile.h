#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGROOFTILE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGROOFTILE_H

#include "BuildingRoofProxy.h"
#include "GeometryAsset.h"
#include "PreparedBuildingAssets.h"
#include "ProjectedErrorBudget.h"
#include "SourceCoverage.h"
#include "Vec2.h"
#include "ViewPath.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace outshine {
struct BuildingRoofTile {
  struct ReadCost {
    size_t Blocks = 0, DecodedRows = 0;
    Generators::BuildingShadowCell::ReadCost Caster;
  };

  struct Support {
    Box Bounds;
    double DiameterM = 0;
    double ShellErrorM = 0;
    [[nodiscard]] bool
    Allows(const Vec3 &eye, const Vec2 &viewport, ProjectedErrorBudget projection) const;
  };

  Support Projected;
  std::string Key;
  Vec3 Anchor;
  std::vector<Generators::BuildingRoofProxy> Roofs;
  std::optional<SourceCoverage> Coverage;
  uint64_t Cells = 0;
  std::optional<GeometryAsset::Product> Remainder;
  std::optional<Generators::BuildingShadowCell> Caster;

  [[nodiscard]] static std::optional<BuildingRoofTile>
  Load(AssetCache &cache,
       Generators::PreparedBuildingAssets &assets,
       const std::string &key,
       const Box &bounds,
       const Generators::PreparedBuildingAssets::SourceGroups &groups,
       const ViewPath &path,
       const Vec2 &viewport,
       ProjectedErrorBudget projection,
       ReadCost &cost);
};
}
#endif
