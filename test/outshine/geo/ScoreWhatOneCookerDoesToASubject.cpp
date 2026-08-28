#include <cstdio>
#include <cmath>
#include <vector>

#include "Check.h"
#include "ClusterDag.h"
#include "Cooked.h"
#include "CookedTile.h"
#include "Geometry.h"
#include "Material.h"

// ONE COOKER, AND IT TAKES A SUBJECT'S OWN ATTRIBUTE SET.
//
// Unreal cooks every mesh into a DAG of ~128-triangle clusters, each carrying its own error bound
// and its parent's, so a cut through the DAG is a valid LOD chosen per cluster; RAGE keeps LOD
// models per map entity. Taking Unreal -- a per-OBJECT ladder cannot spend detail where the camera
// is. Reference: Karis, Nanite: A Deep Dive, SIGGRAPH 2021.
//
// THIS CASE EXISTS TO CHECK A PREMISE BEFORE ANY CODE MOVES. board:1991 was about to be worked on
// the assumption that `ClusterDagBuild` is fixed at the terrain's eight-float soup -- position, uv
// and normal -- and therefore could not cook a subject, which carries up to seven streams.
// Measured: it takes an arbitrary `stride >= 3`, reads the first three floats as POSITION and
// welds the remainder byte-wise as opaque payload. So the cooker is general and there is nothing
// to widen; what a subject needs is its streams INTERLEAVED, which is the inverse of what
// `Generators::Meshed` already does in the other direction.
//
// The oracle is the grid this case builds and owes nothing to our design: a flat lattice of known
// triangle count, cooked, must come back as clusters whose FINEST level covers every triangle and
// whose coarser levels carry a larger error. A cooker that returns one cluster has not clustered,
// and one whose errors do not grow with level cannot be cut without cracks.

namespace {

constexpr int kSide = 65;
constexpr int kStride = 11;

[[nodiscard]] std::vector<float> Lattice() {
  std::vector<float> soup;
  const auto push = [&soup](float e, float n) {
    soup.push_back(e);
    soup.push_back(0.35f * (float)std::sin(0.4 * (double)e) * (float)std::cos(0.3 * (double)n));
    soup.push_back(n);
    soup.push_back(e / (float)kSide);
    soup.push_back(n / (float)kSide);
    soup.push_back(0.0f);
    soup.push_back(1.0f);
    soup.push_back(0.0f);
    soup.push_back(1.0f);
    soup.push_back(0.0f);
    soup.push_back(0.0f);
  };
  for (int row = 0; row + 1 < kSide; ++row) {
    for (int column = 0; column + 1 < kSide; ++column) {
      const float e = (float)column, n = (float)row;
      push(e, n);
      push(e + 1.0f, n);
      push(e + 1.0f, n + 1.0f);
      push(e, n);
      push(e + 1.0f, n + 1.0f);
      push(e, n + 1.0f);
    }
  }
  return soup;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<float> soup = Lattice();
  const uint32_t verts = (uint32_t)(soup.size() / kStride);

  outshine::ClusterDag dag;
  outshine::ClusterDagOpts opts;
  opts.Up[1] = 1.0f;
  const bool cooked = outshine::ClusterDagBuild(soup.data(), verts, kStride, opts, &dag);

  std::printf("A SUBJECT'S SOUP    %u vertices at stride %d, %u triangle(s)\n", verts, kStride,
              verts / 3u);
  std::printf("COOKS TO            %zu cluster(s) over %d level(s), %u triangle(s) in all\n",
              dag.Clusters.size(), dag.Levels, dag.AllTris);

  CHECK(cooked && dag.Clusters.size() > 1 && dag.Levels > 1,
        "**THE ONE COOKER TAKES A SUBJECT'S OWN STRIDE**: it reads the first three floats as "
        "position and welds the rest byte-wise, so uv, normal, tangent and colour ride through it "
        "untouched. board:1991 was about to be worked on the premise that this is fixed at the "
        "terrain's eight floats and could not cook a subject; the premise was wrong and this is "
        "what checked it");

  int deepest = 0;
  float finestErr = outshine::kDagRootErr, coarsestErr = 0.0f;
  uint32_t finestTris = 0;
  for (const outshine::DagCluster &one : dag.Clusters) {
    deepest = (int)one.Level > deepest ? (int)one.Level : deepest;
    if (one.Level == 0) { finestTris += one.Count / 3u; }
    if (one.SelfErr > 0.0f && one.SelfErr < finestErr) { finestErr = one.SelfErr; }
    if (one.SelfErr < outshine::kDagRootErr && one.SelfErr > coarsestErr) {
      coarsestErr = one.SelfErr;
    }
  }
  std::printf("ITS FINEST LEVEL    covers %u triangle(s); errors run %.6g to %.6g over %d level(s)\n",
              finestTris, (double)finestErr, (double)coarsestErr, deepest + 1);

  CHECK(finestTris == verts / 3u,
        "and its FINEST level covers every triangle the soup handed in, so a cut that takes only "
        "leaves draws the original mesh -- a cooker that loses triangles at level zero is a "
        "cooker that changes the shape before anything has chosen an LOD");

  CHECK(coarsestErr > finestErr,
        "**AND THE ERROR GROWS WITH THE LEVEL**, which is the property a cut is chosen by: "
        "`DagSelect` keeps a cluster whose own error is under the threshold while its PARENT's is "
        "over, and that pair only decides anything if a parent is coarser than its children. This "
        "is what Karis spends the 2021 talk on and it is why the cut needs no crack repair");

  // AND IT TAKES THE DOOR'S OWN VALUE, not a soup a caller had to interleave by hand. `Cook`
  // reads a part's streams through `PositionsOf`, `NormalsOf`, `TextureOf`, `TangentsOf` and
  // `ColoursOf` -- all of them already on `include/Geometry.h` -- and derives the stride from what
  // the part actually carries, so a mesh with tangents cooks eleven floats wide and one without
  // cooks eight. `ClusterDag::Stride` records which, so the cooked form knows its own width.
  outshine::Geometry stood;
  const int surface = stood.Surface("lattice", outshine::Material{});
  const int part = stood.Part("lattice", surface);
  std::vector<float> places, normals, uv;
  std::vector<uint32_t> run;
  for (uint32_t vertex = 0; vertex < verts; ++vertex) {
    const float *const at = soup.data() + (size_t)vertex * kStride;
    places.insert(places.end(), at, at + 3);
    uv.insert(uv.end(), at + 3, at + 5);
    normals.insert(normals.end(), at + 5, at + 8);
    run.push_back(vertex);
  }
  const bool filled = part >= 0 && stood.Positions(part, places) && stood.Texture(part, uv) &&
                      stood.Normals(part, normals) && stood.Triangles(part, run);

  outshine::CookedPart cooked2;
  std::string why;
  const bool tookTheDoor = filled && outshine::Cook(stood, part, opts, cooked2, why);
  uint32_t doorFinest = 0;
  for (const outshine::DagCluster &one : cooked2.Dag.Clusters) {
    if (one.Level == 0) { doorFinest += one.Count / 3u; }
  }
  std::printf("AND FROM THE DOOR   stride %d, %zu cluster(s), finest covers %u triangle(s)%s\n",
              cooked2.Dag.Stride, cooked2.Dag.Clusters.size(), doorFinest,
              tookTheDoor ? "" : (" -- refused: " + why).c_str());

  CHECK(tookTheDoor && cooked2.Dag.Stride == 8 && doorFinest == verts / 3u,
        "**AND THE COOKER TAKES THE DOOR'S OWN VALUE**: `Cook` reads a part through the public "
        "read verbs and derives the stride from what the part carries -- eight floats for "
        "position, normal and texture. So a generator, a reader or a foreign program fills one "
        "`Geometry` and the cooked form comes from that, which is CLAUDE.md's tier chain with no "
        "hand-interleaved soup in the middle");

  std::vector<float> soup8;
  soup8.reserve((size_t)verts * 8);
  for (uint32_t vertex = 0; vertex < verts; ++vertex) {
    const float *const at = soup.data() + (size_t)vertex * kStride;
    soup8.insert(soup8.end(), at, at + 8);
  }

  std::vector<float> tileVerts;
  std::vector<uint32_t> tileIdx;
  std::vector<outshine::DagCluster> tileClusters;
  const double origin[3] = {4160000.0, 850000.0, 4730000.0};
  const int gridverts = (int)verts;
  outshine::Ground::CookTile(soup8.data(), (int)verts, gridverts, origin, tileVerts, tileIdx,
                             tileClusters);
  uint32_t covered = 0;
  for (const outshine::DagCluster &one : tileClusters) { covered += one.Count; }
  std::printf("A GROUND TILE       cooks to %zu cluster(s) covering %u index/indices of %zu\n",
              tileClusters.size(), covered, tileIdx.size());

  // THIS CHECK WAS INVERTED, AND TWO MEASUREMENTS INVERTED IT (board:2017).
  //
  // It used to assert that a ground tile goes through the cluster cooker like a subject, with a
  // SKIRT appended as a zero-error cluster. Its stated benchmark was that "Unreal draws Landscape
  // as a primitive in the base pass and RAGE puts terrain on the same draw list as everything
  // else". That is a category error: sharing a DRAW LIST is not sharing a SIMPLIFIER. Unreal's
  // Landscape carries its own heightmap LOD and does not pass through Nanite's cluster builder at
  // runtime -- Nanite builds its DAG OFFLINE at import. Cesium ships terrain tiles already
  // simplified per zoom level and cuts the quadtree by screen-space error.
  //
  // What the tree measured:
  //
  //   1. the runtime DAG was the ENTIRE cost of standing a world. Sampling a place mid-load put
  //      572 stack samples in `dag::Clustered`, `dag::SimplifyGroup` and `dag::Absorb`, reducing
  //      2 048 triangles per tile down to 8 across 128 tiles -- to rebuild a reduction the TILE
  //      PYRAMID already carries, because zoom z-1 IS the simplification of zoom z
  //   2. the skirt hid NOTHING. Hashing every ring vertex by its east/north to a quarter metre,
  //      the widest height disagreement between two tiles sharing a point was exactly the skirt's
  //      own depth at a 1 m skirt, and 0.00 m over 4 832 shared vertices with no skirt at all --
  //      while 903 of 3 078 clusters were drawn, so the LOD cut was genuinely selective. It cost
  //      18 per cent of the ring's triangles and carved a black trench along every tile border
  //
  // So the ONE COOKER stands and its scope is stated: it is the only cooker, and it cooks
  // SUBJECTS. A ground tile keeps its grid, because the pyramid is its LOD.
  CHECK(tileClusters.size() == 1 && covered == tileIdx.size(),
        "**A GROUND TILE KEEPS ITS GRID**: the pyramid is already the terrain's LOD -- zoom z-1 IS "
        "the simplification of zoom z -- so a tile cooks to ONE cluster covering every triangle "
        "handed in, with no simplification and no skirt. Nanite builds its DAG offline at import "
        "and Cesium ships tiles pre-simplified per level; neither reduces terrain at runtime. The "
        "negative control is the subject above, which still cooks to many");

  Covers("the cooker: it takes a subject's own vertex stride, its finest level covers every "
         "triangle handed in, and its error bounds grow with the level -- board:1991's premise, "
         "measured before the code that depends on it");
  return Report();
}
