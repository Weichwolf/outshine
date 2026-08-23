#include <cmath>
#include <cstdio>
#include <vector>

#include "Check.h"

#include "ChunkSurface.h"
#include "TerrainGrid.h"
#include "TerrainLoader.h"

using outshine::Ground::ChunkNodes;
using outshine::Ground::FillNodeHeights;
using outshine::Ground::TerrainField;
using outshine::Ground::TileHeightAslM;

namespace {

constexpr uint32_t kPostings = 257;

// a ramp in both directions: the height at any place is a known number, so the stream's
// answer can be compared to the truth AT THE PLACE IT WAS ASKED ABOUT
[[nodiscard]] double RampM(double fx, double fy) { return 100.0 + 900.0 * fx + 500.0 * fy; }

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  TerrainField field(kPostings, kPostings);
  for (uint32_t r = 0; r < kPostings; ++r) {
    for (uint32_t c = 0; c < kPostings; ++c) {
      field.SetM(r, c, (float)RampM((double)c / (double)(kPostings - 1),
                                    (double)r / (double)(kPostings - 1)));
    }
  }

  const int nodes = ChunkNodes(kPostings, 64);
  std::vector<float> held;
  FillNodeHeights(field, kPostings, kPostings, nodes, &held);
  CHECK(held.size() == (size_t)nodes * (size_t)nodes,
        "the stream fills one height per chunk node");

  double worst = 0.0;
  double worstAt[2] = {0.0, 0.0};
  for (int j = 1; j < nodes - 1; ++j) {
    for (int i = 1; i < nodes - 1; ++i) {
      const double fx = (double)i / (double)(nodes - 1);
      const double fy = (double)j / (double)(nodes - 1);
      const double answered = TileHeightAslM(held.data(), nodes, kPostings, fx, fy);
      const double truth = RampM(fx, fy);
      if (std::fabs(answered - truth) > worst) {
        worst = std::fabs(answered - truth);
        worstAt[0] = fx;
        worstAt[1] = fy;
      }
    }
  }
  Note("the worst height the stream answers away from the truth", worst, "m");
  std::printf("NOTE worst at fx %.4f fy %.4f\n", worstAt[0], worstAt[1]);
  CHECK(worst < 1.0e-2,
        "**THE STREAM ANSWERS WHERE THE MESH PLACES**: every queried place reads the ramp "
        "at ITS OWN position -- the texel reading answered a place half a posting away, "
        "so drawn ground and queried ground stood apart by construction (board:1752)");

  {
    // the same truth at the boundary fractions the drive actually asks at
    const double corners[4][2] = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}};
    double worstCorner = 0.0;
    for (const auto &at : corners) {
      const double answered = TileHeightAslM(held.data(), nodes, kPostings, at[0], at[1]);
      worstCorner = std::fmax(worstCorner, std::fabs(answered - RampM(at[0], at[1])));
    }
    Note("and at the tile's four corners", worstCorner, "m");
    CHECK(worstCorner < 1.0e-2, "the corners answer their own places too");
  }

  Covers("I.28 the stream answers heights in the currency the mesh places vertices in: a "
         "ramp field read back at every chunk node and at the corners equals the ramp at "
         "those places (board:1750, 1752)");
  return Report();
}
