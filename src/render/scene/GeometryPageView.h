#ifndef OUTSHINE_RENDER_SCENE_GEOMETRYPAGEVIEW_H
#define OUTSHINE_RENDER_SCENE_GEOMETRYPAGEVIEW_H
#include "PieceMesh.h"
#include "PrismInstance.h"
#include "GeometryErrorBound.h"
#include "SourceCoverage.h"
#include "GeometryPasses.h"
#include "MaterialRegistration.h"
#include "LevelOfDetail.h"
#include "ViewReuseDomain.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace outshine {
struct PrismGrid;
}

namespace outshine::Render {
struct ConvexPrism;

struct GeometryPagePrismView {
  // Placements are relative to the page, like mesh vertices.
  std::span<const ConvexPrism> Shapes;
  MaterialRegistration Material;
  GeometryPasses Passes = GeometryPasses::MainAndShadow;
};

struct GeometryPageGridView {
  std::shared_ptr<const PrismGrid> Grid;
  std::span<const PrismInstance> SceneInstances;
  MaterialRegistration Material;
  GeometryPasses Passes = GeometryPasses::MainAndShadow;
};

struct GeometryPagePartView {
  std::span<const float> Tangents;
  std::span<const StoredVertex> Verts;
  std::span<const uint32_t> Indices;
  std::span<const DagCluster> Clusters;
  std::span<const float> Colours;
  std::span<const float> Uv1;
  PieceSurface Surface;
  bool Textured = false;
  GeometryPasses Passes = GeometryPasses::MainAndShadow;
};

struct GeometryPageView {
  std::span<const GeometryPagePartView> Parts;
  uint64_t Digest = 0;
  std::string_view AssetKey;
  bool FallbackHeights = false;
  std::optional<ViewReuseDomain> SelectedView;
  std::optional<GeometryErrorBound> ErrorBound = std::nullopt;
  std::optional<LevelOfDetail> RequestedDetail;
  std::optional<uint32_t> RequestedCell;
  uint64_t OccupiedCells = 0;
  std::optional<SourceCoverage> Coverage = std::nullopt;
  std::shared_ptr<const Mat4> Placement = nullptr;
  std::optional<uint64_t> DrawOrderGroup = std::nullopt;
  std::optional<GeometryPagePrismView> Prism = std::nullopt;
  std::optional<GeometryPageGridView> Grid = std::nullopt;
};
}
#endif
