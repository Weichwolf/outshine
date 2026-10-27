#ifndef OUTSHINE_RENDER_GEOMETRY_RENDERGEOMETRYVIEW_H
#define OUTSHINE_RENDER_GEOMETRY_RENDERGEOMETRYVIEW_H

#include "Box.h"
#include "ClusterCook.h"
#include "Material.h"
#include "PunctualLight.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace outshine::Render {

struct RenderGeometryPart {
  std::string Name;
  int Material = -1;
  bool HasUv = false;
  bool HasUv1 = false;
  bool HasNormal = false;
  bool HasColour = false;
  bool HasTangent = false;
  size_t FirstVertex = 0;
  size_t VertexCount = 0;
  size_t FirstIndex = 0;
  size_t IndexCount = 0;

  uint32_t FirstCluster = 0;
  uint32_t ClusterCount = 0;

  std::span<const float> PositionsM;
  std::span<const float> Normals;
  std::span<const float> Tangents;
  std::span<const float> Uv;
  std::span<const float> Uv1;
  std::span<const float> Colours;
};

struct RenderGeometryView {
  std::span<const RenderGeometryPart> Parts;
  std::span<const uint32_t> Indices;

  std::span<const Material> Surfaces;

  std::span<const PunctualLight> Lamps;
  std::span<const DagCluster> Clusters;
  std::span<const float> ClusterSpheres;

  bool CarriesUv = false;
  bool CarriesUv1 = false;
  bool CarriesNormal = false;
  bool CarriesTangent = false;
  bool CarriesColour = false;

  [[nodiscard]] size_t VertexCount() const {
    return Parts.empty() ? 0u : Parts.back().FirstVertex + Parts.back().VertexCount;
  }

  [[nodiscard]] size_t TriangleCount() const { return Indices.size() / 3; }

  [[nodiscard]] bool Empty() const { return Parts.empty() || Indices.empty(); }

  [[nodiscard]] Box BoundsOf(size_t parts) const;
};

inline constexpr uint32_t kClusterTriangles = 128;

}
#endif
