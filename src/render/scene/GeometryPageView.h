#ifndef OUTSHINE_RENDER_SCENE_GEOMETRYPAGEVIEW_H
#define OUTSHINE_RENDER_SCENE_GEOMETRYPAGEVIEW_H
#include "SubjectTypes.h"
#include "LevelOfDetail.h"
#include "ViewReuseDomain.h"
#include <cstdint>
#include <optional>
#include <span>

namespace outshine::Render {
struct GeometryPagePartView {
  std::span<const float> Tangents;
  std::span<const StoredVertex> Verts;
  std::span<const uint32_t> Indices;
  std::span<const DagCluster> Clusters;
  std::span<const float> Colours;
  std::span<const float> Uv1;
  PieceSurface Surface;
  bool Textured = false;
};

struct GeometryPageView {
  std::span<const GeometryPagePartView> Parts;
  uint64_t Digest = 0;
  bool FallbackHeights = false;
  std::optional<ViewReuseDomain> SelectedView;
  std::optional<LevelOfDetail> RequestedDetail;
  std::optional<uint32_t> RequestedCell;
  uint64_t OccupiedCells = 0;
};
}
#endif
