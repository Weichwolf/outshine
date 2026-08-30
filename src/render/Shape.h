#ifndef OUTSHINE_RENDER_SHAPE_H
#define OUTSHINE_RENDER_SHAPE_H

#include <cstddef>
#include "ClusterCook.h"
#include "SurfaceState.h"
#include <cstdint>
#include <span>
#include <vector>

#include <Material.h>
#include <PunctualLight.h>
#include <string_view>

namespace outshine::Render {

struct ShapePart {
  std::string_view Name;
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

struct Shape {
  std::span<const ShapePart> Parts;
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

  void BoundsOf(size_t parts, double leastM[3], double mostM[3]) const;
};

struct ShapeStore {
  std::vector<ShapePart> Parts;
  std::vector<float> PositionsM;
  std::vector<float> Normals;
  std::vector<float> Tangents;
  std::vector<float> Uv;
  std::vector<float> Uv1;
  std::vector<float> Colours;
  std::vector<uint32_t> Indices;
  std::vector<Material> Surfaces;
  std::vector<PunctualLight> Lamps;
  std::vector<DagCluster> Clusters;

  std::vector<float> ClusterSpheres;

  void Clear() {
    Parts.clear();
    PositionsM.clear();
    Normals.clear();
    Tangents.clear();
    Uv.clear();
    Uv1.clear();
    Colours.clear();
    Indices.clear();
    Surfaces.clear();
    Lamps.clear();
    Clusters.clear();
    ClusterSpheres.clear();
  }
};

inline constexpr uint32_t kClusterTriangles = 128;
void CookShape(ShapeStore &into, std::span<const Material> surfaces);

[[nodiscard]] double CookedMs();

[[nodiscard]] std::size_t CookedRootless();

} // namespace outshine::Render
#endif
