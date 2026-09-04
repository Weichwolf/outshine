#include "CookedTile.h"

#include <cstring>

#include <generate/Generate.h>

#include <span>

#include "spatial/ClusterCook.h"
#include "math/Vec3.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include <scene/Geometry.h>
#include <scene/Material.h>
#include <vector>

namespace outshine::Ground {

namespace {

constexpr size_t kTileSoupFloats = kTileVertexFloats;

constexpr uint32_t kTileClusterTriangles = 128;

constexpr uint32_t kTileDagLevels = static_cast<uint32_t>(Generators::Detail::Skyline) -
                                    static_cast<uint32_t>(Generators::Detail::Fine);

} // namespace

void CookTile(std::span<const ChunkVtx> soup,
              int gridverts,
              [[maybe_unused]] const Vec3 &origin,
              std::vector<TileVertex> &outVerts,
              std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters) {
  outVerts.clear();
  outIdx.clear();
  outClusters.clear();
  const auto nverts = static_cast<int>(soup.size());
  if (soup.empty()) { return; }
  if (gridverts <= 0 || gridverts > nverts) { gridverts = nverts; }

  outVerts.assign(soup.begin(), soup.begin() + gridverts);
  outIdx.resize(static_cast<size_t>(gridverts));
  for (int vertex = 0; vertex < gridverts; ++vertex) {
    outIdx[static_cast<size_t>(vertex)] = static_cast<uint32_t>(vertex);
  }

  const Cooked cut =
      CookDag(std::span<const float>(reinterpret_cast<const float *>(soup.data()),
                                     static_cast<size_t>(gridverts) * kTileSoupFloats),
              outIdx,
              {.MostTriangles = kTileClusterTriangles, .MostLevels = kTileDagLevels},
              static_cast<int>(kTileSoupFloats));
  if (cut.Clusters.empty() || cut.Index.size() < outIdx.size()) {
    DagCluster whole{};
    whole.Count = static_cast<uint32_t>(gridverts);
    whole.ParentErr = kDagRootErr;
    const Bounding around = BoundingSphere({.Floats = reinterpret_cast<const float *>(soup.data()),
                                            .Count = static_cast<uint32_t>(gridverts),
                                            .Stride = static_cast<int>(kTileSoupFloats)});
    whole.SelfCenter = around.CentreM;
    whole.SelfRadius = around.RadiusM;
    outClusters.push_back(whole);
    return;
  }
  const auto ownFrom = static_cast<uint32_t>(gridverts);
  for (const uint32_t from : cut.MadeFrom) {
    const size_t at = from < ownFrom ? from : outVerts.size() - (ownFrom - from);
    ChunkVtx made = outVerts[std::min(at, outVerts.size() - 1)];
    const size_t which = outVerts.size();
    made.pos = Vec3f{
        {cut.PositionsM[which * 3], cut.PositionsM[which * 3 + 1], cut.PositionsM[which * 3 + 2]}};
    outVerts.push_back(made);
  }
  outIdx = cut.Index;
  outClusters = cut.Clusters;
}

} // namespace outshine::Ground
