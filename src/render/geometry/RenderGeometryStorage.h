#ifndef OUTSHINE_RENDER_GEOMETRY_RENDERGEOMETRYSTORAGE_H
#define OUTSHINE_RENDER_GEOMETRY_RENDERGEOMETRYSTORAGE_H

#include "RenderGeometryView.h"
#include <expected>
#include <memory>
#include <vector>

namespace outshine {
class Geometry;
}

namespace outshine::Render {

struct ClusterBuildMetrics {
  double BuildMs = 0;
  size_t RootClusters = 0;
  size_t Clusters = 0;
};

struct RenderGeometryStorage {
  std::shared_ptr<const Geometry> AttributeSource;
  ClusterBuildMetrics Clustering;
  std::vector<RenderGeometryPart> Parts;
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
    Clustering = {};
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
    AttributeSource.reset();
  }
};

void AppendGeometry(const Geometry &from, RenderGeometryStorage &into);
[[nodiscard]] std::expected<void, ClusterError>
PrepareRenderGeometryStorage(std::shared_ptr<const Geometry> from, RenderGeometryStorage &into);
[[nodiscard]] std::expected<void, ClusterError>
BindRenderGeometryStorage(RenderGeometryStorage &into);
[[nodiscard]] RenderGeometryView
MakeRenderGeometryView(const RenderGeometryStorage &store) noexcept;
[[nodiscard]] std::expected<RenderGeometryView, ClusterError>
FinalizeRenderGeometry(RenderGeometryStorage &into);
[[nodiscard]] std::expected<RenderGeometryView, ClusterError>
PrepareRenderGeometry(const Geometry &from, RenderGeometryStorage &into);
[[nodiscard]] std::expected<RenderGeometryView, ClusterError>
PrepareRenderGeometry(std::shared_ptr<const Geometry> from, RenderGeometryStorage &into);

}
#endif
