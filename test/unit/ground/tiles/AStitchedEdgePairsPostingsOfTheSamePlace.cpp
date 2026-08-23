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
  [[nodiscard]] TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    (void)y;
    const uint32_t side = ((int)x == Coarse) ? (kSide - 1) / 2 + 1 : kSide;
    std::vector<float> metres((size_t)side * side, 0.0f);
    for (uint32_t r = 0; r < side; ++r) {
      const double north = (double)r / (double)(side - 1);
      for (uint32_t c = 0; c < side; ++c) {
        // the tile spans [x, x+1) of the ramp's world, sampled at this tile's own postings
        const double within = (double)c / (double)(side - 1);
        metres[(size_t)r * side + c] = RampM((double)x + within, north);
      }
    }
    return TerrainBytes::From(z, x, y, outshine::Test::TerrariumPng(side, side, metres));
  }
};

[[nodiscard]] double EdgeGap(const TerrainGrid &left, const TerrainGrid &right) {
  const auto *l = left.TryField();
  const auto *r = right.TryField();
  if (!l || !r) { return 1.0e30; }
  // the corner postings belong to TWO seams (this edge and the perpendicular one), so
  // each carries its north/south neighbour's average as well -- and on the coarser side
  // that contamination reaches one COARSE spacing inward. The shared edge proper is what
  // lies past it, and that is what a closed seam means here
  const double margin = 1.0 / (double)(std::min(l->Rows(), r->Rows()) - 1u);
  double worst = 0.0;
  for (uint32_t row = 0; row < l->Rows(); ++row) {
    const double along = (double)row / (double)(l->Rows() - 1);
    if (along < margin + 1e-9 || along > 1.0 - margin - 1e-9) { continue; }
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

    // every fine posting past the corner margin carries the average, not just the first
    // min(rows) of them -- truncation left a step along the edge
    double worstUnstitched = 0.0;
    const double margin = 1.0 / (double)(std::min(l->Rows(), r->Rows()) - 1u);
    size_t stitched = 0;
    for (uint32_t row = 0; row < l->Rows(); ++row) {
      const double along = (double)row / (double)(l->Rows() - 1);
      if (along < margin + 1e-9 || along > 1.0 - margin - 1e-9) { continue; }
      ++stitched;
      const double mine = l->AtM(row, l->Cols() - 1);
      worstUnstitched = std::fmax(worstUnstitched,
                                  std::fabs(mine - r->PostingM(0.0, along)));
    }
    Note("fine postings compared past the corner margin", (double)stitched, "postings");
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
                                           (double)RampM(4.0 + east, north)));
      }
    }
    Note("the worst height error at a vertex's own position", worst, "m");
    CHECK(worst < 1.0e-2,
          "**THE MESH READS ITS HEIGHTS IN THE CURRENCY IT PLACES THEM**: every interior "
          "vertex carries the ramp at ITS OWN position, not the one half a posting "
          "spacing away (board:1750)");
  }

  Covers("I.27 a stitched edge pairs postings of the same PLACE: the fraction along the "
         "shared edge is the currency, every posting of the finer side is covered, and a "
         "resolution boundary meets within a metre (board:1746)");
  return Report();
}
