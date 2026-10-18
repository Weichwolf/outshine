#include "Check.h"
#include "Shape.h"
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<float, 9> vertices{0, 0, 0, 1, 0, 0, 0, 1, 0};
  std::vector<float> shapePositions;
  std::vector<uint32_t> shapeIndices;
  for (uint32_t triangleAt = 0; triangleAt < 260; ++triangleAt) {
    const float along = static_cast<float>(triangleAt % 17);
    shapePositions.insert(shapePositions.end(), {along, 0, 0, along + 0.5f, 0, 0, along, 1, 0});
    shapeIndices.insert(shapeIndices.end(),
                        {triangleAt * 3, triangleAt * 3 + 1, triangleAt * 3 + 2});
  }
  const auto fillsShape = [&](Render::ShapeStore &store) {
    store.PositionsM = shapePositions;
    store.Indices = shapeIndices;
    Render::ShapePart part;
    part.VertexCount = shapePositions.size() / 3;
    part.IndexCount = shapeIndices.size();
    part.PositionsM = store.PositionsM;
    store.Parts.push_back(part);
  };
  Render::ShapeStore uninterrupted;
  Render::ShapeStore interrupted;
  fillsShape(uninterrupted);
  fillsShape(interrupted);
  CHECK(Render::CookShape(uninterrupted, {}).has_value(),
        "one-shot shape cooking accepts the clustered fixture");
  Render::ShapeCookJob shapeJob(interrupted, {});
  for (size_t turn = 0; !shapeJob.Ready() && turn < 100000; ++turn) {
    const auto advanced = shapeJob.Advance(turn % 7 + 1);
    CHECK(advanced.has_value(), "paced shape cooking accepts every intermediate boundary");
  }
  CHECK(shapeJob.Ready(), "paced shape cooking completes under finite changing budgets");
  CHECK(interrupted.Indices == uninterrupted.Indices &&
            interrupted.Clusters == uninterrupted.Clusters &&
            interrupted.ClusterSpheres == uninterrupted.ClusterSpheres,
        "paced shape cooking reproduces the one-shot native buffers exactly");
  CHECK(interrupted.Parts[0].FirstCluster == uninterrupted.Parts[0].FirstCluster &&
            interrupted.Parts[0].ClusterCount == uninterrupted.Parts[0].ClusterCount &&
            interrupted.Clustering.RootClusters == uninterrupted.Clustering.RootClusters &&
            interrupted.Clustering.Clusters == uninterrupted.Clustering.Clusters,
        "paced shape cooking reproduces part ranges and aggregate counts exactly");
  Render::ShapeStore shape;
  shape.PositionsM.assign(vertices.begin(), vertices.end());
  shape.Indices = {0, 1, 9};
  Render::ShapePart part;
  part.VertexCount = 3;
  part.IndexCount = 3;
  shape.Parts.push_back(part);
  const auto rejectedShape = Render::FinalizeShape(shape);
  CHECK(!rejectedShape && rejectedShape.error() == ClusterError::InvalidIndex,
        "render shape finalization propagates invalid-index failure even on the small-mesh path");
  shape.Indices = {0, 1, 2};
  CHECK(Render::FinalizeShape(shape).has_value(),
        "a corrected input can be retried after failed shape preparation");
  CHECK(shape.Clustering.Clusters == 1 && shape.Clustering.RootClusters == 1,
        "cluster metrics belong to the shape that owns the geometry");
  Render::ShapeStore cameraShape;
  CHECK(Render::FinalizeShape(cameraShape).has_value() && cameraShape.Clustering.Clusters == 0,
        "an empty camera helper has its own empty cluster metrics");
  CHECK(shape.Clustering.Clusters == 1 && shape.Clustering.RootClusters == 1,
        "preparing a second shape cannot overwrite the first shape's metrics");
  shape.Parts[0].FirstVertex = std::numeric_limits<size_t>::max();
  const auto invalidRange = Render::FinalizeShape(shape);
  CHECK(!invalidRange && invalidRange.error() == ClusterError::InvalidLayout,
        "vertex ranges are checked before constructing attribute spans");
  shape.Parts[0].FirstVertex = 0;
  shape.Parts[0].HasNormal = true;
  const auto missingNormals = Render::FinalizeShape(shape);
  CHECK(!missingNormals && missingNormals.error() == ClusterError::InvalidLayout,
        "a declared attribute cannot reference missing storage");
  return Report();
}
