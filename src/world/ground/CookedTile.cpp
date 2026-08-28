#include "CookedTile.h"

#include <string>

#include <Geometry.h>
#include <Material.h>

#include "Cooked.h"

namespace outshine::Ground {

namespace {

constexpr size_t kTileSoupFloats = 8;

[[nodiscard]] bool Filled(const float *soup, int gridverts, Geometry &into) {
  const int surface = into.Surface("ground", Material{});
  const int part = into.Part("ground", surface);
  if (part < 0) { return false; }
  std::vector<float> places, uv, normals;
  std::vector<uint32_t> run;
  places.reserve((size_t)gridverts * 3);
  uv.reserve((size_t)gridverts * 2);
  normals.reserve((size_t)gridverts * 3);
  run.reserve((size_t)gridverts);
  for (int vertex = 0; vertex < gridverts; ++vertex) {
    const float *const at = soup + (size_t)vertex * kTileSoupFloats;
    places.insert(places.end(), at, at + 3);
    uv.insert(uv.end(), at + 3, at + 5);
    normals.insert(normals.end(), at + 5, at + 8);
    run.push_back((uint32_t)vertex);
  }
  return into.Positions(part, places) && into.Texture(part, uv) && into.Normals(part, normals) &&
         into.Triangles(part, run);
}

}

void CookTile(const float *soup, int nverts, int gridverts, const double origin[3],
              std::vector<float> &outVerts, std::vector<uint32_t> &outIdx,
              std::vector<DagCluster> &outClusters) {
  outVerts.clear();
  outIdx.clear();
  outClusters.clear();
  if (!soup || nverts <= 0) { return; }
  if (gridverts <= 0 || gridverts > nverts) { gridverts = nverts; }

  ClusterDagOpts opts;
  const double length =
      std::sqrt(origin[0] * origin[0] + origin[1] * origin[1] + origin[2] * origin[2]);
  if (length > 1.0) {
    for (int axis = 0; axis < 3; ++axis) { opts.Up[axis] = (float)(origin[axis] / length); }
  }

  Geometry stood;
  CookedPart cooked;
  std::string why;
  if (Filled(soup, gridverts, stood) && Cook(stood, 0, opts, cooked, why)) {
    outVerts = std::move(cooked.Dag.Verts);
    outIdx = std::move(cooked.Dag.Idx);
    outClusters = std::move(cooked.Dag.Clusters);
  } else {
    outVerts.assign(soup, soup + (size_t)gridverts * kTileSoupFloats);
    outIdx.resize((size_t)gridverts);
    for (int vertex = 0; vertex < gridverts; ++vertex) { outIdx[(size_t)vertex] = (uint32_t)vertex; }
    DagCluster whole{};
    whole.Count = (uint32_t)gridverts;
    whole.ParentErr = kDagRootErr;
    BoundingSphere(soup, (uint32_t)gridverts, (int)kTileSoupFloats, whole.SelfCenter,
                   &whole.SelfRadius);
    outClusters.push_back(whole);
  }

  const int skirt = nverts - gridverts;
  if (skirt <= 0) { return; }
  const uint32_t base = (uint32_t)(outVerts.size() / kTileSoupFloats);
  DagCluster hem{};
  hem.First = (uint32_t)outIdx.size();
  hem.Count = (uint32_t)skirt;
  hem.SelfErr = 0.0f;
  hem.ParentErr = kDagRootErr;
  double centre[3] = {0, 0, 0};
  for (int at = 0; at < skirt; ++at) {
    for (int axis = 0; axis < 3; ++axis) {
      centre[axis] += soup[(size_t)(gridverts + at) * kTileSoupFloats + (size_t)axis];
    }
  }
  for (int axis = 0; axis < 3; ++axis) { hem.SelfCenter[axis] = (float)(centre[axis] / skirt); }
  double furthest = 0.0;
  for (int at = 0; at < skirt; ++at) {
    double away = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
      const double step =
          soup[(size_t)(gridverts + at) * kTileSoupFloats + (size_t)axis] - (double)hem.SelfCenter[axis];
      away += step * step;
    }
    furthest = furthest > away ? furthest : away;
  }
  hem.SelfRadius = (float)std::sqrt(furthest);
  outVerts.insert(outVerts.end(), soup + (size_t)gridverts * kTileSoupFloats,
                  soup + (size_t)nverts * kTileSoupFloats);
  for (int at = 0; at < skirt; ++at) { outIdx.push_back(base + (uint32_t)at); }
  outClusters.push_back(hem);
}

}
