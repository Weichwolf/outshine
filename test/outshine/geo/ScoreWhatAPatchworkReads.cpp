#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <vector>

#include "Check.h"
#include "GroundPatchwork.h"
#include "TileMeshes.h"

namespace {

// A VERTEX LAYOUT IS ONE TRUTH AND IT WAS SPELLED TWICE.
//
// `TileBuild::Verts` is a flat float array and `kTileVertexFloats` says how many floats one
// vertex takes. `src/world/ground/World.cpp` divided by 8 in four places and
// `src/world/ground/TilePool.cpp` in two more; `src/compositor/GroundPatchwork.cpp` walked the
// same array in steps of THREE. So the compositor read a position, then a normal, then a pair of
// texture coordinates, and called all of them positions.
//
// WHAT THAT LOOKS LIKE, measured on the shipped Munich patch before the fix:
//
//                      before        after
//   vertices            5 194        1 948      = 5194 * 3 / 8
//   height range      151..897 m   489.8..532.1 m
//
// **746 metres of relief over 815 metres of ground, in Munich**, and it drew as a fan of jagged
// triangles across the right half of the windscreen. A reader that takes a normal for a position
// does not fail: it produces geometry, confidently, and the failure is loud only if somebody
// looks at the picture or at the range.
//
// THE ORACLE IS THE TILE THIS CASE HANDS IN. `TileMeshes` is an interface, so the case supplies
// its own: a 2x2 grid of vertices at positions this file chose, each followed by a normal and a
// UV pair that are DELIBERATELY not plausible positions -- (999, -999, 999) and (0.5, 0.25).
// A stride of 3 cannot help but pick them up. What comes back must be the four positions, in
// order, and nothing else.
//
// THE DOUBLE MUST WEAR THE REAL LAYOUT. `ChunkVtx` is `pos[3]; uv[2]; norm[3]` and this case's
// tile used to hand in the normal FIRST and the UV after it. Nothing noticed, because nothing
// downstream read either -- `LayPatchwork` copied three floats per vertex and threw the other
// five away, recomputing normals from the geometry it had just built. So the case written to
// stop one truth being spelled twice was itself the second spelling.
//
// It matters now: the patchwork CARRIES the tile's normals and UVs, because the tile authors
// them properly -- the DEM's own gradient for the surface and the outward radial for a skirt --
// and geometry recomputed from a mesh with vertical skirt quads gives those skirts HORIZONTAL
// normals. Measured on the shipped Munich ring: 7128 of 17532 normals lay sideways, 41 per cent
// of a surface that leans 1.27 degrees on average and 26 degrees at its steepest.
//
// The layout is now named once, in `TileMeshes.h` beside the struct whose layout it is, and every
// site divides or steps by that name.
constexpr float kSentinel = 999.0f;
constexpr float kUvU = 0.5f;
constexpr float kUvV = 0.25f;

class OneTile final : public outshine::TileMeshes {
public:
  [[nodiscard]] Reply Mesh(int, uint32_t, uint32_t, int, outshine::TileBuild *out) override {
    static const float kPlaces[4][3] = {
        {0.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 0.0f}, {10.0f, 2.0f, 10.0f}, {0.0f, 3.0f, 10.0f}};
    out->Verts.clear();
    for (const auto &one : kPlaces) {
      out->Verts.push_back(one[0]);
      out->Verts.push_back(one[1]);
      out->Verts.push_back(one[2]);
      out->Verts.push_back(kUvU);
      out->Verts.push_back(kUvV);
      out->Verts.push_back(kSentinel);
      out->Verts.push_back(-kSentinel);
      out->Verts.push_back(kSentinel);
    }
    out->Idx = {0, 1, 2, 0, 2, 3};
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

}

// AND A RING IS ASKED FOR IN A WAY THAT LETS IT ARRIVE. `TilePool` meshes on worker threads and
// answers `Pending` until one finishes, so a caller that polls gets whatever happened to be ready.
// `Engine::State::Composes` retried only while `LayPatchwork` FAILED -- and a ring with one tile
// and eight pending is not a failure, so the retry never ran and the composed ground was ONE tile
// of nine, laid 287..1102 m east of a car standing at 0.
//
// Polling harder made it worse: thirty seconds of retrying took the ring from one tile to none,
// because a spin on `Mesh` starves the workers it is waiting for. A wait that makes the thing it
// waits for less ready is not a wait.
//
// `TileMeshes` now carries `MeshAwaited` beside `Mesh` -- the twin `TilePool::BytesBlocking`
// already had on the byte path -- and its default is to answer exactly as `Mesh` does, so an
// implementation that cannot wait is unchanged. `Around::Awaited` says which the caller wants.
// On the shipped network: 1 tile of 9 in 41 s of polling, 9 of 9 in 12 s of waiting, and the
// number no longer depends on which thread won.
class Twice final : public outshine::TileMeshes {
public:
  explicit Twice(OneTile &one) : One_(one) {}

  [[nodiscard]] Reply Mesh(int z, uint32_t x, uint32_t y, int grid, outshine::TileBuild *out) override {
    const uint64_t key = ((uint64_t)x << 32) | y;
    if (Asked_.insert(key).second) { return Reply::Pending; }
    return One_.Mesh(z, x, y, grid, out);
  }

  [[nodiscard]] Reply Wants(int z, uint32_t x, uint32_t y, int grid) override {
    outshine::TileBuild aside;
    return Mesh(z, x, y, grid, &aside);
  }

  [[nodiscard]] Reply MeshAwaited(int z, uint32_t x, uint32_t y, int grid,
                                  outshine::TileBuild *out) override {
    const Reply first = Mesh(z, x, y, grid, out);
    if (first != Reply::Pending) { return first; }
    return Mesh(z, x, y, grid, out);
  }

private:
  OneTile &One_;
  std::set<uint64_t> Asked_;
};

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Around over;
  over.LatDeg = 48.1372;
  over.LonDeg = 11.5756;
  over.Zoom = 14;
  over.Levels = 1;

  OneTile everywhere;
  const auto laid = outshine::LayPatchwork(everywhere, over);
  if (!laid) {
    Unprepared(("the patchwork refused: " + laid.error()).c_str());
    return Report();
  }

  const size_t vertices = laid->PositionM.size() / 3;
  double worst = 0.0;
  bool sentinel = false;
  static const float kPlaces[4][3] = {
      {0.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 0.0f}, {10.0f, 2.0f, 10.0f}, {0.0f, 3.0f, 10.0f}};
  for (size_t at = 0; at < laid->PositionM.size(); ++at) {
    if (std::fabs((double)laid->PositionM[at]) > 900.0) { sentinel = true; }
    if (at / 3 < 4) {
      const double owed = (double)kPlaces[at / 3][at % 3];
      const double off = std::fabs((double)laid->PositionM[at] - owed);
      if (off > worst) { worst = off; }
    }
  }

  std::printf("  tiles laid                 %zu\n", laid->Tiles);
  std::printf("  vertices the patch carries %zu   the tile handed in 4\n", vertices);
  std::printf("  worst position error       %.6f m\n", worst);
  std::printf("  a sentinel reached a position: %s\n", sentinel ? "YES" : "no");

  CHECK(laid->Tiles > 0 && vertices > 0,
        "the patchwork laid the tile this case handed it, so there is geometry to judge -- a "
        "patch of nothing would satisfy every check below by silence");

  CHECK(vertices == 4 * laid->Tiles,
        "**A VERTEX LAYOUT IS ONE TRUTH**: each tile hands in FOUR vertices of "
        "kTileVertexFloats floats each, and a reader that steps by three reports 5194 where 1948 "
        "stand -- the ratio 8/3 exactly. On the shipped Munich patch that read 746 m of relief "
        "over 815 m of ground");

  CHECK(!sentinel,
        "and no normal reaches a position: the tile's normals are (999, -999, 999), which no "
        "ground on this planet is, so a stride that slides into them is caught by the value and "
        "not only by the count");

  CHECK(worst < 1.0e-4,
        "and the positions come back as they went in, in order, so the walk reads the array the "
        "struct declares rather than one that happens to be the same length");

  const size_t normals = laid->NormalM.size() / 3;
  const size_t uvs = laid->Uv.size() / 2;
  bool normalsAreTheTiles = normals == vertices;
  for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
    normalsAreTheTiles = normalsAreTheTiles && laid->NormalM[at] == kSentinel &&
                         laid->NormalM[at + 1] == -kSentinel && laid->NormalM[at + 2] == kSentinel;
  }
  bool uvsAreTheTiles = uvs == vertices;
  for (size_t at = 0; at + 1 < laid->Uv.size(); at += 2) {
    uvsAreTheTiles = uvsAreTheTiles && laid->Uv[at] == kUvU && laid->Uv[at + 1] == kUvV;
  }
  std::printf("  normals carried            %zu   uvs carried %zu\n", normals, uvs);

  CHECK(normalsAreTheTiles,
        "**THE PATCH CARRIES THE TILE'S OWN NORMALS**, unchanged, one per vertex. The tile "
        "authors them from the DEM's gradient for its surface and from the outward radial for "
        "its skirt; a patchwork that recomputes them from geometry gives every vertical skirt "
        "quad a HORIZONTAL normal, and 7128 of the shipped Munich ring's 17532 lay sideways on a "
        "surface that leans 1.27 degrees on average");
  CHECK(uvsAreTheTiles,
        "and it carries the tile's UVs, which is what a ground surface will be sampled through "
        "-- a patch that drops them cannot be told road from field however good the table behind "
        "it is");

  {
    outshine::Around ring = over;
    ring.Levels = 2;
    OneTile source;
    Twice polled(source);
    const auto quick = outshine::LayPatchwork(polled, ring);
    OneTile other;
    Twice awaited(other);
    const auto whole = outshine::LayPatchwork(awaited, ring);
    const size_t polledTiles = quick ? quick->Tiles : 0;
    const size_t awaitedTiles = whole ? whole->Tiles : 0;
    std::printf("  a block, asked without waiting  %zu tile(s)\n", polledTiles);
    std::printf("  the same block, asked again    %zu tile(s)\n", awaitedTiles);

    // THIS CHECK WAS WITHDRAWN AND REPLACED, and the reason is an INVARIANT rather than a number.
    //
    // It used to assert that "a caller that waits gets all nine" -- that `Around::Awaited` made
    // `LayPatchwork` block on `MeshAwaited` until every tile had landed. Waiting is exactly what
    // board:2017 removed, and it removed it because a stack sample of a stalled place read
    //
    //     Engine::assemble -> Composes -> LayPatchwork -> TilePool::MeshAwaited -> __psynch_cvwait
    //
    // with every fetch worker parked in `condition_variable::wait`. The main thread waited for the
    // workers and the workers waited for work: a deadlock, not a delay, and it cost the suite
    // 122 s per case. CLAUDE.md carries the rule this broke -- four things run independently and
    // IO is the fourth, because a fetch BLOCKS and a blocking wait on a compute path is a worker
    // doing nothing while holding a slot. Unreal keeps IO off every compute path with
    // `FIoDispatcher`; RAGE runs streaming threads beside `sysTaskManager`.
    //
    // So the claim is inverted: a block is laid COMPLETE on the first ask, whatever has arrived,
    // and a tile that has not arrived stands on the bare WGS84 ellipsoid until it does.
    CHECK(polledTiles == awaitedTiles && polledTiles > 0,
          "**A BLOCK IS COMPLETE ON THE FIRST ASK**: nothing waits, so asking twice returns the "
          "same count -- what changes between the two is how many of those tiles carry ground "
          "rather than the bare ellipsoid. A count that GREW on the second ask would mean the "
          "first had left holes, which is what a caller would have had to wait for");
  }

  {
    class Never final : public outshine::TileMeshes {
    public:
      [[nodiscard]] Reply Mesh(int, uint32_t, uint32_t, int, outshine::TileBuild *) override {
        return Reply::Pending;
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
    outshine::Around ring = over;
    ring.Levels = 2;
    Never never;
    const auto asked = outshine::LayPatchwork(never, ring);
    const size_t bare = asked ? asked->Bare : 0;
    const size_t tiles = asked ? asked->Tiles : 0;
    std::printf("  a block whose tiles never land: %s, %zu of %zu bare\n",
                asked ? "laid" : ("refused -- " + asked.error()).c_str(), bare, tiles);

    // THIS CHECK WAS INVERTED BY THE SAME INVARIANT (board:2017), and the inversion is the point.
    //
    // It used to assert that a ring over tiles that never arrive comes back with a REFUSAL. That
    // was the right answer while `Awaited` existed, because the alternative was holding the caller
    // for ever. With nothing waiting, a refusal is now the WRONG answer: a game engine that cannot
    // reach the network still has to draw the Earth. So every tile that has not arrived stands on
    // the bare WGS84 ellipsoid -- no elevation, correct curvature, correct place -- and a later
    // pass replaces it. The old flightbox streamer wrote the same rule down: "a chunk that has not
    // arrived is covered by its parent and picked up on a later frame, so the world refines
    // progressively and the frame loop never blocks."
    CHECK(asked && tiles > 0 && bare == tiles,
          "**A BLOCK OVER TILES THAT NEVER LAND STILL STANDS**: it comes back complete, with every "
          "tile on the bare ellipsoid, rather than refusing or holding the caller. The negative "
          "control is `bare == tiles` -- if any tile claimed ground from a source that hands back "
          "nothing, the fallback would be inventing terrain rather than standing in for it");
  }

  Covers("compositor: a ground patchwork reads a tile at the stride the tile's own layout "
         "declares, so a normal never reaches a position and the relief a patch reports is the "
         "relief the ground has, and a ring is asked for in a way that lets every tile of it arrive");
  return Report();
}
