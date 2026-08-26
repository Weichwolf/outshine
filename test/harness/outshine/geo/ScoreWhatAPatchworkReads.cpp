#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
// The layout is now named once, in `TileMeshes.h` beside the struct whose layout it is, and every
// site divides or steps by that name.
constexpr float kSentinel = 999.0f;

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
      out->Verts.push_back(kSentinel);
      out->Verts.push_back(-kSentinel);
      out->Verts.push_back(kSentinel);
      out->Verts.push_back(0.5f);
      out->Verts.push_back(0.25f);
    }
    out->Idx = {0, 1, 2, 0, 2, 3};
    out->OriginEcef[0] = 4160000.0;
    out->OriginEcef[1] = 850000.0;
    out->OriginEcef[2] = 4730000.0;
    out->ErrM = 0.25f;
    return Reply::Ready;
  }

};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Around over;
  over.LatDeg = 48.1372;
  over.LonDeg = 11.5756;
  over.Zoom = 14;
  over.Ring = 0;

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

  CHECK(vertices == 4,
        "**A VERTEX LAYOUT IS ONE TRUTH**: the tile hands in FOUR vertices of "
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

  Covers("compositor: a ground patchwork reads a tile at the stride the tile's own layout "
         "declares, so a normal never reaches a position and the relief a patch reports is the "
         "relief the ground has");
  return Report();
}
