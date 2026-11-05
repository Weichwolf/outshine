#include "Check.h"
#include "MeshClusterBuildJob.h"
#include "RenderGeometryStorage.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  RenderGeometryStorage store;
  store.PositionsM.reserve(size_t{2} * 130 * 9);
  GeometryClusters prepared;
  std::vector<DagCluster> expected;
  for (uint32_t part = 0; part < 2; ++part) {
    const size_t firstVertex = store.PositionsM.size() / 3;
    const size_t firstIndex = store.Indices.size();
    std::vector<uint32_t> local;
    for (uint32_t i = 0; i < 130; ++i) {
      const auto x = static_cast<float>((i * 47) % 130);
      store.PositionsM.insert(store.PositionsM.end(), {x, 0, 0, x + .5f, 0, 0, x, 1, 0});
      local.insert(local.end(), {i * 3, i * 3 + 1, i * 3 + 2});
    }
    const auto positions = std::span<const float>(store.PositionsM).subspan(firstVertex * 3);
    auto product = CookClusters({.PositionsM = positions, .Indices = local}, 17);
    CHECK(product, "the asset producer supplies an explicit nondefault cluster partition");
    if (!product) { return Report(); }
    for (const auto index : product->Index) {
      store.Indices.push_back(index + static_cast<uint32_t>(firstVertex));
    }
    for (auto cluster : product->Clusters) {
      cluster.First += static_cast<uint32_t>(firstIndex);
      expected.push_back(cluster);
    }
    prepared.push_back(std::move(product->Clusters));
    store.Parts.push_back({.Name = "prepared",
                           .FirstVertex = firstVertex,
                           .VertexCount = 390,
                           .FirstIndex = firstIndex,
                           .IndexCount = 390,
                           .PositionsM = positions,
                           .Normals = {},
                           .Tangents = {},
                           .Uv = {},
                           .Uv1 = {},
                           .Colours = {}});
  }
  const auto indices = store.Indices;
  const auto original = prepared;
  MeshClusterBuildJob restore(store, {}, std::move(prepared));
  for (size_t calls = 0; !restore.Ready() && calls < 1000; ++calls) {
    const size_t before = store.Clusters.size();
    const auto advanced = restore.Advance(1);
    CHECK(advanced, "ready products restore with a bounded publication budget");
    if (!advanced) { return Report(); }
    CHECK(store.Clusters.size() - before <= 1, "a slice publishes at most one cluster");
    CHECK(store.Indices == indices, "restoration never rewrites the prepared index order");
  }
  CHECK(restore.Ready() && store.Clusters == expected,
        "both cached partitions survive with correct global index bases");
  CHECK(store.Clustering.RestoredParts == 2 && store.Clustering.CookedParts == 0,
        "cache products bypass every clustering pass");
  CHECK(store.Parts[0].ClusterCount == 8 && store.Parts[1].FirstCluster == 8 &&
            store.Parts[1].ClusterCount == 8,
        "renderer part ranges retain the producer's partition");
  auto malformed = original;
  malformed.front().front().First = 1;
  MeshClusterBuildJob invalidRange(store, {}, std::move(malformed));
  CHECK(!invalidRange.Advance(100000), "misaligned prepared ranges cannot become render products");
  malformed = original;
  malformed.pop_back();
  MeshClusterBuildJob invalidParts(store, {}, std::move(malformed));
  CHECK(!invalidParts.Advance(100000), "prepared tables must cover the same geometry parts");
  return Report();
}
