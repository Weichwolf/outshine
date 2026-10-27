#include "Check.h"
#include "MeshClusterBuildJob.h"
#include "RenderGeometryStorage.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
void Fill(outshine::Render::RenderGeometryStorage &store) {
  for (uint32_t i = 0; i < 260; ++i) {
    const auto x = static_cast<float>(i % 17);
    store.PositionsM.insert(store.PositionsM.end(), {x, 0, 0, x + .5f, 0, 0, x, 1, 0});
    store.Indices.insert(store.Indices.end(), {i * 3, i * 3 + 1, i * 3 + 2});
  }
  outshine::Render::RenderGeometryPart part;
  part.VertexCount = store.PositionsM.size() / 3;
  part.IndexCount = store.Indices.size();
  part.PositionsM = store.PositionsM;
  store.Parts.push_back(part);
}
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  RenderGeometryStorage reference;
  RenderGeometryStorage sliced;
  Fill(reference);
  Fill(sliced);
  CHECK(BuildMeshClusters(reference, {}).has_value(), "the complete reference build succeeds");
  MeshClusterBuildJob job(sliced, {});
  for (size_t call = 0; !job.Ready() && call < 100000; ++call) {
    const size_t budget = call % 7;
    const auto indices = sliced.Indices;
    const auto clusters = sliced.Clusters.size();
    const double time = sliced.Clustering.BuildMs;
    const auto result = job.Advance(budget);
    CHECK(result.has_value(), "each finite slice succeeds");
    if (!result) { return Report(); }
    size_t changes = sliced.Clusters.size() - clusters;
    for (size_t i = 0; i < indices.size(); ++i) {
      changes += static_cast<size_t>(indices[i] != sliced.Indices[i]);
    }
    CHECK(changes <= budget, "indices and cluster records share the requested work budget");
    if (changes > budget) { return Report(); }
    if (budget == 0) {
      CHECK(sliced.Clustering.BuildMs == time, "a zero-work slice performs no build work");
    }
  }
  CHECK(job.Ready(), "changing positive and zero budgets finish in finite calls");
  CHECK(sliced.Indices == reference.Indices && sliced.Clusters == reference.Clusters &&
            sliced.ClusterSpheres == reference.ClusterSpheres,
        "bounded publication preserves every completed index and cluster value");
  CHECK(sliced.Parts.front().ClusterCount == reference.Parts.front().ClusterCount &&
            sliced.Clustering.RootClusters == reference.Clustering.RootClusters,
        "bounded publication preserves part ranges and root counts");
  return Report();
}
