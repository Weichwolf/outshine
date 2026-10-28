#include "BuildingPageAdapter.h"
#include "ClusterCook.h"
#include <cstdint>
#include <span>

namespace outshine {
namespace {
Render::GeometryPagePartView BuildingPart(std::span<const StoredVertex> vertices,
                                          std::span<const uint32_t> indices,
                                          const ClusteredMesh &mesh,
                                          const Render::PieceSurface &surface,
                                          std::span<const float> colours = {},
                                          std::span<const float> uv = {}) {
  const bool cooked = mesh.Index.size() == indices.size() && !mesh.Clusters.empty();
  return {.Tangents = {},
          .Verts = vertices,
          .Indices = cooked ? std::span<const uint32_t>(mesh.Index) : indices,
          .Clusters =
              cooked ? std::span<const DagCluster>(mesh.Clusters) : std::span<const DagCluster>(),
          .Colours = colours,
          .Uv1 = uv,
          .Surface = surface,
          .Textured = true};
}
}

BuildingPageView::BuildingPageView(const Generators::BakedTile &baked,
                                   const BuildingPageMaterials &materials) {
  const auto &built = baked.Built;
  Parts_[0] =
      BuildingPart(built.WallCorners,
                   built.WallRun,
                   baked.Walls,
                   built.WallMaterialM.empty() ? materials.Walls
                                               : materials.MetreWalls.value_or(materials.Walls),
                   built.WallColours,
                   built.WallMaterialM);
  Parts_[1] = BuildingPart(built.RoofCorners, built.RoofRun, baked.Roofs, materials.Roofs);
  Page_ = {.Parts = {},
           .Digest = baked.Digest,
           .AssetKey = baked.GeometryAssetKey,
           .FallbackHeights = baked.FallbackHeights,
           .SelectedView = baked.SelectedView,
           .RequestedDetail = baked.RequestedDetail,
           .RequestedCell = baked.RequestedCell,
           .OccupiedCells = baked.OccupiedCells};
}

Render::GeometryPageView BuildingPageView::View() const noexcept {
  auto result = Page_;
  result.Parts = Parts_;
  return result;
}
}
