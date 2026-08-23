#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "TerrariumPng.h"

#include "TerrainTiles.h"

using outshine::Ground::EnuFrame;
using outshine::Ground::TerrainBytes;
using outshine::Ground::TerrainGrid;
using outshine::Ground::TerrainSource;
using outshine::Ground::TerrainTiles;

namespace {

constexpr uint32_t kSide = 17;

// a ramp in BOTH directions: the height varies ALONG the shared edge as well as across
// it, which is what makes a mispaired posting visible -- on an edge-constant field the
// wrong pairing averages to the right answer and proves nothing
[[nodiscard]] float RampM(double fracEast, double fracNorth) {
  return (float)(100.0 + 900.0 * fracEast + 500.0 * fracNorth);
}

class Ramps : public TerrainSource {
public:
  int Coarse = -1; // the x whose tile is served at half the postings, or -1 for none
  // a tile served from a DIFFERENT vintage: real DEM sources disagree at the metre where
  // they meet, which is the whole reason a stitcher exists -- and the reason a corner's
  // four copies are four different numbers
  int OffsetX = -1, OffsetY = -1;
  float OffsetM = 0.0f;
  [[nodiscard]] TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    const uint32_t side = ((int)x == Coarse) ? (kSide - 1) / 2 + 1 : kSide;
    std::vector<float> metres((size_t)side * side, 0.0f);
    for (uint32_t r = 0; r < side; ++r) {
      // the ramp is ONE world: a tile's postings sample [x, x+1) x [y, y+1) of it, so
      // neighbours agree where they touch and any disagreement is the stitcher's
      const double north = (double)y + (double)r / (double)(side - 1);
      for (uint32_t c = 0; c < side; ++c) {
        const double within = (double)c / (double)(side - 1);
        metres[(size_t)r * side + c] = RampM((double)x + within, north);
        if ((int)x == OffsetX && (int)y == OffsetY) {
          metres[(size_t)r * side + c] += OffsetM;
        }
      }
    }
    return TerrainBytes::From(z, x, y, outshine::Test::TerrariumPng(side, side, metres));
  }
};

[[nodiscard]] double EdgeGap(const TerrainGrid &left, const TerrainGrid &right) {
  const auto *l = left.TryField();
  const auto *r = right.TryField();
  if (!l || !r) { return 1.0e30; }
  // EVERY posting of the shared edge, corners included: the corner pass gives all four
  // tiles that share a corner one average over the same four raw fields, so the margin
  // this proof once excluded (a quarter of the seam at 17 against 9) is gone (board:1756)
  double worst = 0.0;
  for (uint32_t row = 0; row < l->Rows(); ++row) {
    const double along = (double)row / (double)(l->Rows() - 1);
    const double mine = l->AtM(row, l->Cols() - 1);
    const double theirs = r->PostingM(0.0, along);
    worst = std::fmax(worst, std::fabs(mine - theirs));
  }
  return worst;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const EnuFrame frame = EnuFrame::At(48.1, 11.5);
  CHECK(frame.Where() == EnuFrame::State::Usable, "the test's frame stands");

  {
    Ramps even;
    TerrainTiles tiles(even, frame, TerrainTiles::Config{});
    const TerrainGrid left = tiles.StitchedGrid(12, 4, 8);
    const TerrainGrid right = tiles.StitchedGrid(12, 5, 8);
    const double gap = EdgeGap(left, right);
    Note("the worst height gap along the shared edge, equal postings", gap, "m");
    CHECK(gap < 1.0e-2,
          "two tiles of equal posting count meet with a closed seam -- the baseline");
  }
  {
    // the case the crop path manufactures: one side served at HALF the postings. Index
    // pairing put a fine posting against a coarse one and left the rest unstitched
    Ramps mixed;
    mixed.Coarse = 5;
    TerrainTiles tiles(mixed, frame, TerrainTiles::Config{});
    const TerrainGrid left = tiles.StitchedGrid(12, 4, 8);
    const TerrainGrid right = tiles.StitchedGrid(12, 5, 8);
    const auto *l = left.TryField();
    const auto *r = right.TryField();
    CHECK(l != nullptr && r != nullptr && l->Cols() != r->Cols(),
          "the two neighbours really do carry different posting counts");
    const double gap = EdgeGap(left, right);
    Note("the worst height gap across a resolution boundary", gap, "m");
    CHECK(gap < 1.0,
          "**A STITCHED EDGE PAIRS POSTINGS OF THE SAME PLACE**: a native tile beside a "
          "coarser one meets within a metre at EVERY shared fraction -- index pairing "
          "moved heights to places they do not belong and left half the fine edge "
          "unstitched (board:1746)");

    // every fine posting carries the average, not just the first min(rows) of them --
    // truncation left a step along the edge
    double worstUnstitched = 0.0;
    size_t stitched = 0;
    for (uint32_t row = 0; row < l->Rows(); ++row) {
      const double along = (double)row / (double)(l->Rows() - 1);
      ++stitched;
      const double mine = l->AtM(row, l->Cols() - 1);
      worstUnstitched = std::fmax(worstUnstitched,
                                  std::fabs(mine - r->PostingM(0.0, along)));
    }
    Note("fine postings compared, corners included", (double)stitched, "postings");
    CHECK(worstUnstitched < 1.0,
          "and EVERY posting of the fine edge was stitched, not the first min(rows) of "
          "them -- a step along the edge is what truncation left behind");
  }

  {
    // the mesh reads heights where it PLACES vertices: a ramp field meshes to vertices
    // whose height is the ramp at their own position, which the texel reading (half a
    // posting spacing off) cannot satisfy (board:1750)
    Ramps even;
    TerrainTiles tiles(even, frame, TerrainTiles::Config{});
    const auto mesh = tiles.MeshOf(12, 4, 8);
    CHECK(mesh.Where() == outshine::Ground::TerrainMesh::State::Built, "the tile meshes");
    const std::vector<float> *held = mesh.TryPositionsEnuM();
    CHECK(held != nullptr, "the mesh hands its positions");
    if (held == nullptr) { return Report(); }
    const std::vector<float> &positions = *held;
    CHECK(mesh.VertexCount() == kSide * kSide, "one vertex per posting");
    double worst = 0.0;
    for (uint32_t r = 1; r + 1 < kSide; ++r) {
      for (uint32_t c = 1; c + 1 < kSide; ++c) {
        const size_t vi = (size_t)r * kSide + c;
        const double north = (double)r / (double)(kSide - 1);
        const double east = (double)c / (double)(kSide - 1);
        worst = std::fmax(worst, std::fabs((double)positions[vi * 3 + 2] -
                                           (double)RampM(4.0 + east, 8.0 + north)));
      }
    }
    Note("the worst height error at a vertex's own position", worst, "m");
    CHECK(worst < 1.0e-2,
          "**THE MESH READS ITS HEIGHTS IN THE CURRENCY IT PLACES THEM**: every interior "
          "vertex carries the ramp at ITS OWN position, not the one half a posting "
          "spacing away (board:1750)");
  }

  {
    // a stranger's 1x1 PNG: the field decodes, cannot mesh, and must never reach the
    // stitcher -- PostingFrac(k, 1) once minted 0 * inf and the NaN cast to an unsigned
    // index, undefined behaviour the sanitised arm now watches (board:1755)
    class OnePosting : public TerrainSource {
    public:
      [[nodiscard]] TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
        const std::vector<float> one{42.0f};
        return TerrainBytes::From(z, x, y, outshine::Test::TerrariumPng(1, 1, one));
      }
    } tiny;
    TerrainTiles tiles(tiny, frame, TerrainTiles::Config{});
    const TerrainGrid grid = tiles.StitchedGrid(12, 4, 8);
    CHECK(grid.TryField() == nullptr && grid.Where() == TerrainGrid::State::NotHere,
          "**A FIELD TOO SMALL TO MESH REFUSES BEFORE THE STITCHER READS IT**: a 1x1 "
          "terrarium tile answers NotHere, the verdict the crop arm already gave "
          "(board:1755)");
    const auto mesh = tiles.MeshOf(12, 4, 8);
    CHECK(mesh.Where() != outshine::Ground::TerrainMesh::State::Built,
          "and it meshes into nothing rather than into a NaN");
  }
  {
    // the degenerate lattice cannot mint a NaN fraction even if a caller asks
    outshine::Ground::TerrainField one(1, 1);
    one.SetM(0, 0, 7.0f);
    CHECK(one.PostingM(0.0, 0.0) == 7.0f && one.PostingM(1.0, 0.5) == 7.0f,
          "a one-posting lattice answers its only place at every fraction, never an "
          "infinity scaled into an index cast");
    CHECK(outshine::Ground::PostingFrac(0, 1) == 0.0,
          "and PostingFrac of a single posting is its own place, not 0 * inf");
  }

  {
    // the corner four tiles share reads as ONE height from every one of them (board:1756)
    Ramps mixed;
    mixed.Coarse = 5;
    mixed.OffsetX = 4;
    mixed.OffsetY = 9;
    mixed.OffsetM = 300.0f; // the south-west tile of the shared corner comes from another vintage
    TerrainTiles tiles(mixed, frame, TerrainTiles::Config{});
    const TerrainGrid nw = tiles.StitchedGrid(12, 4, 8);
    const TerrainGrid ne = tiles.StitchedGrid(12, 5, 8);
    const TerrainGrid sw = tiles.StitchedGrid(12, 4, 9);
    const TerrainGrid se = tiles.StitchedGrid(12, 5, 9);
    const auto *a = nw.TryField();
    const auto *b = ne.TryField();
    const auto *c = sw.TryField();
    const auto *d = se.TryField();
    CHECK(a && b && c && d, "the four tiles around one corner all stand");
    if (a && b && c && d) {
      // the corner they share: south-east of nw, south-west of ne, north-east of sw,
      // north-west of se
      const double heights[4] = {(double)a->AtM(a->Rows() - 1, a->Cols() - 1),
                                 (double)b->AtM(b->Rows() - 1, 0),
                                 (double)c->AtM(0, c->Cols() - 1),
                                 (double)d->AtM(0, 0)};
      double worst = 0.0;
      for (const double one : heights) { worst = std::fmax(worst, std::fabs(one - heights[0])); }
      Note("the worst disagreement at the shared corner", worst, "m");
      CHECK(worst < 1.0e-3,
            "**A STITCHED CORNER IS THE SAME PLACE FROM ALL FOUR TILES**: one average over "
            "the four raw fields, not two sequential edge passes that each overwrite the "
            "other and never ask the diagonal (board:1756)");
    }
  }

  Covers("I.27 a stitched edge pairs postings of the same PLACE: the fraction along the "
         "shared edge is the currency, every posting of the finer side is covered, and a "
         "resolution boundary meets within a metre (board:1746)");
  return Report();
}
