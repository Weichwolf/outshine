#include <cstdio>
#include <vector>

#include "Check.h"
#include "ClusterDag.h"
#include "GroundPatchwork.h"

// A CUT THROUGH THE DAG IS CHOSEN, AND WHERE THE CAMERA STANDS DECIDES IT.
//
// Unreal's Nanite cooks a mesh into a DAG of ~128-triangle clusters, each carrying its own error
// bound AND its parent's, and selects a cluster when its own screen-space error is under the
// threshold while its parent's is over -- that pair is what makes a cut valid without cracks, and
// it is most of what Karis spends the 2021 talk on. RAGE picks an LOD model per map entity.
// Taking Unreal: a per-OBJECT ladder cannot spend detail where the camera is.
//
// MEASURED BEFORE THIS CASE: `DagSse`, `DagEdgeSq`, `DagCrossFactor` and `DagSelect` -- the whole
// evaluating half of `src/base/spatial/ClusterDag.h` -- had ZERO callers in `src/`, `apps/` and
// `test/`. Outside `TilePool.cpp`, which builds them, nothing read `Clusters` except the byte
// accounting. So the DAG was built for every terrain tile, stored, measured for memory, and used
// to select nothing. CLAUDE.md calls it *"this tree's Nanite, and essential to the frame path"*;
// the expensive half ran and the cheap half did not.
//
// THE ORACLE IS THE CAMERA AND OWES NOTHING TO OUR DESIGN. One tile, one DAG, two eyes: near the
// tile a cut must take the LEAVES, far from it the same cut must take their PARENT, and the
// triangle count must follow. Written as a comparison of two runs over the identical tile rather
// than against a constant, so the number cannot be tuned to whatever the tree happens to do.

namespace {

constexpr float kLeafErr = 0.0f;
constexpr float kParentErr = 0.5f;
constexpr float kFocalPx = 720.0f;

class Layered final : public outshine::TileMeshes {
public:
  [[nodiscard]] Reply Mesh(int, uint32_t, uint32_t, int, outshine::TileBuild *out) override {
    static const float kPlaces[6][3] = {{0.0f, 0.0f, 0.0f},  {10.0f, 0.0f, 0.0f},
                                        {10.0f, 0.0f, 10.0f}, {0.0f, 0.0f, 10.0f},
                                        {5.0f, 0.0f, 0.0f},  {5.0f, 0.0f, 10.0f}};
    out->Verts.clear();
    for (const auto &one : kPlaces) {
      out->Verts.push_back(one[0]);
      out->Verts.push_back(one[1]);
      out->Verts.push_back(one[2]);
      out->Verts.push_back(0.0f);
      out->Verts.push_back(0.0f);
      out->Verts.push_back(0.0f);
      out->Verts.push_back(1.0f);
      out->Verts.push_back(0.0f);
    }
    out->Idx = {0, 4, 5, 0, 5, 3, 4, 1, 2, 4, 2, 5, 0, 1, 2, 0, 2, 3};
    out->Clusters.clear();
    outshine::DagCluster west{};
    west.First = 0;
    west.Count = 6;
    west.SelfErr = kLeafErr;
    west.ParentErr = kParentErr;
    west.SelfRadius = west.ParentRadius = 8.0f;
    west.SelfCenter[0] = west.ParentCenter[0] = 5.0f;
    west.SelfCenter[2] = west.ParentCenter[2] = 5.0f;
    outshine::DagCluster east = west;
    east.First = 6;
    outshine::DagCluster whole{};
    whole.First = 12;
    whole.Count = 6;
    whole.SelfErr = kParentErr;
    whole.ParentErr = outshine::kDagRootErr;
    whole.SelfRadius = whole.ParentRadius = 8.0f;
    whole.SelfCenter[0] = whole.ParentCenter[0] = 5.0f;
    whole.SelfCenter[2] = whole.ParentCenter[2] = 5.0f;
    whole.Level = 1;
    out->Clusters = {west, east, whole};
    out->OriginEcef[0] = 4160000.0;
    out->OriginEcef[1] = 850000.0;
    out->OriginEcef[2] = 4730000.0;
    out->ErrM = 0.25f;
    return Reply::Ready;
  }

  [[nodiscard]] Reply Wants(int z, uint32_t x, uint32_t y, int grid) override {
    outshine::TileBuild aside;
    return Mesh(z, x, y, grid, &aside);
  }

  [[nodiscard]] Reply MeshAwaited(int z, uint32_t x, uint32_t y, int grid,
                                  outshine::TileBuild *out) override {
    return Mesh(z, x, y, grid, out);
  }
};

[[nodiscard]] outshine::Around Standing(double backM) {
  outshine::Around over;
  over.Zoom = 12;
  over.Levels = 1;
  over.Grid = 33;
  over.FocalPx = kFocalPx;
  // THE EYE IS ABSOLUTE ECEF NOW, not an offset from a shift that never meant that (board:2017).
  // `LayPatchwork` differences it against each tile's OWN origin, so a caller states where the
  // camera stands on the Earth and the cut is taken from there. Before, `Around::EyeM` had one
  // reader and no writer at all, so every cut was taken from {0,0,0}.
  over.EyeM[0] = 4160000.0 + 5.0;
  over.EyeM[1] = 850000.0 + backM;
  over.EyeM[2] = 4730000.0 + 5.0;
  return over;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Layered close;
  const auto near = outshine::LayPatchwork(close, Standing(12.0));
  Layered afar;
  const auto far = outshine::LayPatchwork(afar, Standing(4000.0));
  Layered blind;
  outshine::Around uncut = Standing(12.0);
  uncut.FocalPx = 0.0f;
  const auto whole = outshine::LayPatchwork(blind, uncut);

  if (!near || !far || !whole) {
    Unprepared("a single tile would not lay under this ring");
    return Report();
  }

  std::printf("THE TILE HOLDS       %zu cluster(s)\n", whole->ClustersHeld);
  std::printf("AN EYE 12 m AWAY     draws %zu of them, %zu triangle(s)\n", near->ClustersDrawn,
              near->Index.size() / 3);
  std::printf("AN EYE 4000 m AWAY   draws %zu of them, %zu triangle(s)\n", far->ClustersDrawn,
              far->Index.size() / 3);
  std::printf("AND NO CUT AT ALL    draws %zu of them, %zu triangle(s)\n", whole->ClustersDrawn,
              whole->Index.size() / 3);

  // THREE CLUSTERS PER TILE, AND `Levels = 1` LAYS A 4x4 BLOCK (board:2017), so the population is
  // 48. The old `Around::Ring = 0` meant one tile; a cascade level has no such spelling, because
  // its finest level must cover the area its next level out will skip.
  CHECK(whole->ClustersHeld == 3 * whole->Tiles && whole->Tiles == 16,
        "the tiles hand over the DAG they were built with, so what follows is a cut through three "
        "clusters per tile rather than a comparison between two empty lists");

  CHECK(far->Index.size() < near->Index.size(),
        "**THE SAME TILE DRAWS FEWER TRIANGLES FROM FURTHER AWAY**: the cut takes the LEAVES when "
        "their parent's screen-space error is over the threshold and takes the PARENT when it is "
        "not. This is the pair of error bounds Nanite is built on, and it is what a per-object "
        "LOD ladder cannot do -- the choice is per CLUSTER, so a subject half off-screen does not "
        "pay full price for the half nobody sees");

  CHECK(whole->Index.size() > near->Index.size(),
        "and the control is the cut itself: the identical tile laid with no focal length copies "
        "every index of every cluster, so what the check above measures is the SELECTION and not "
        "the tile");

  Covers("the frame path: a cut through the cluster DAG is chosen from where the camera stands, "
         "and the triangle count follows it -- board:1991's evaluating half, which had no caller "
         "at all before this");
  return Report();
}
