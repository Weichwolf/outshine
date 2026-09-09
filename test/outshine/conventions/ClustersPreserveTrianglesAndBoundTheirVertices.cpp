#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <vector>
#include "Check.h"
#include "ClusterCook.h"
#include "Shape.h"

namespace {
using namespace outshine;
using namespace outshine::Test;

void Verify(ClusterMeshInput input, uint32_t limit) {
  const auto result = CookClusters(input, limit);
  CHECK(result.has_value(), "valid bounded mesh can be clustered");
  if (!result) { return; }
  using Triangle = std::array<uint32_t, 3>;
  std::multiset<Triangle> original, packed;
  for (size_t index = 0; index < input.Indices.size(); index += 3) {
    original.insert({input.Indices[index], input.Indices[index + 1], input.Indices[index + 2]});
  }
  size_t next = 0;
  for (const auto &cluster : result->Clusters) {
    CHECK(cluster.First == next && cluster.Count > 0 && cluster.Count % 3 == 0 &&
              cluster.Count / 3 <= limit,
          "clusters cover disjoint complete triangle ranges within the limit");
    CHECK(std::isfinite(cluster.SelfRadius) && cluster.SelfRadius >= 0,
          "every culling sphere has a finite nonnegative radius");
    for (size_t corner = cluster.First; corner < cluster.First + cluster.Count; ++corner) {
      const auto vertex = result->Index[corner];
      double squared = 0;
      for (size_t axis = 0; axis < 3; ++axis) {
        const double delta =
            static_cast<double>(input.PositionsM[vertex * input.StrideFloats + axis]) -
            cluster.SelfCenter[axis];
        squared += delta * delta;
      }
      CHECK(std::sqrt(squared) <= static_cast<double>(cluster.SelfRadius),
            "the published sphere contains each referenced vertex without a relaxed epsilon");
    }
    next += cluster.Count;
  }
  CHECK(next == input.Indices.size(), "every source index belongs to exactly one cluster");
  for (size_t index = 0; index < result->Index.size(); index += 3) {
    packed.insert({result->Index[index], result->Index[index + 1], result->Index[index + 2]});
  }
  CHECK(packed == original, "the oriented triangle multiset is preserved exactly");
  const auto repeated = CookClusters(input, limit);
  CHECK(repeated && repeated->Index == result->Index, "cluster ordering is repeatable");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<float, 9> vertices{0, 0, 0, 1, 0, 0, 0, 1, 0};
  const std::array<uint32_t, 3> triangle{0, 1, 2};
  Verify({.PositionsM = vertices, .Indices = triangle}, 1);
  const auto empty = CookClusters({}, 1);
  CHECK(empty && empty->Index.empty() && empty->Clusters.empty(),
        "an empty mesh is a valid empty result");
  for (uint32_t stride : {0u, 1u, 2u, 4u}) {
    const auto bad =
        CookClusters({.PositionsM = vertices, .Indices = triangle, .StrideFloats = stride}, 1);
    CHECK(!bad && bad.error() == ClusterError::InvalidLayout,
          "invalid strides are rejected, never corrected silently");
  }
  for (uint32_t limit : {0u, std::numeric_limits<uint32_t>::max()}) {
    const auto bad = CookClusters({.PositionsM = vertices, .Indices = triangle}, limit);
    CHECK(!bad && bad.error() == ClusterError::InvalidLimit,
          "zero or unrepresentable cluster limits are rejected");
  }
  CHECK(!CookClusters({.PositionsM = vertices, .Indices = std::array<uint32_t, 4>{0, 1, 2, 0}}, 1),
        "trailing indices cannot disappear silently");
  const auto invalid =
      CookClusters({.PositionsM = vertices, .Indices = std::array<uint32_t, 3>{0, 1, 9}}, 1);
  CHECK(!invalid && invalid.error() == ClusterError::InvalidIndex,
        "invalid indices cannot be published in a nominally successful cluster");
  for (float value :
       {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
    auto positions = vertices;
    positions[0] = value;
    const auto invalidPosition = CookClusters({.PositionsM = positions, .Indices = triangle}, 1);
    CHECK(!invalidPosition && invalidPosition.error() == ClusterError::NonFinitePosition,
          "nonfinite coordinates are rejected before Morton conversion");
  }
  const std::array<float, 9> extreme{-2e38f, 0, 0, 2e38f, 0, 0, 0, 0, 0};
  Verify({.PositionsM = extreme, .Indices = triangle}, 1);
  Verify({.PositionsM = extreme, .Indices = std::array<uint32_t, 6>{0, 1, 2, 2, 1, 0}}, 1);
  const float huge = std::numeric_limits<float>::max();
  const std::array<float, 9> overflow{-huge, -huge, -huge, huge, huge, huge, 0, 0, 0};
  const auto unbounded = CookClusters({.PositionsM = overflow, .Indices = triangle}, 1);
  CHECK(!unbounded && unbounded.error() == ClusterError::BoundsOverflow,
        "unrepresentable GPU bounds produce an explicit failure");
  std::vector<float> positions;
  std::vector<uint32_t> indices;
  for (uint32_t at = 0; at < 80; ++at) {
    for (size_t vertex = 0; vertex < 3; ++vertex) {
      positions.insert(positions.end(),
                       {vertices[vertex * 3],
                        vertices[vertex * 3 + 1],
                        vertices[vertex * 3 + 2],
                        std::numeric_limits<float>::quiet_NaN(),
                        17});
      indices.push_back(at * 3 + static_cast<uint32_t>(vertex));
    }
  }
  const ClusterMeshInput interleaved{
      .PositionsM = positions, .Indices = indices, .StrideFloats = 5};
  Verify(interleaved, 7);
  const auto ties = CookClusters(interleaved, 7);
  CHECK(ties && ties->Index == indices,
        "Morton ties retain source triangle order independently of std::sort tie behavior");
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
  Covers("complete indexed triangle layouts, finite conservative spheres, deterministic Morton "
         "ties, checked capacity and render-shape error propagation");
  return Report();
}
