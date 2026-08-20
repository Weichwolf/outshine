/* WHAT THE MIP CHAIN AVERAGES AWAY AND WHAT PUTS IT BACK, held as two claims that meet in one channel
 * (board:1130). The chain records the mean resultant length of the normals it averaged; the BRDF turns
 * that length into roughness by Toksvig's factor. Neither half means anything without the other, and
 * neither had a test.
 *
 * THE LIMITS ARE THE WHOLE CLAIM, because the formula was rearranged to reach them without a clamp:
 * `a2' = 2D/(N + 2D)` divides once where the direct `2/(s' + 2)` divides twice and needs a floor at
 * `a2 = 0`. So the three limits are checked as VALUES and not as trends -- an implementation that
 * merely moved roughness in the right direction would pass a trend and fail here.
 *
 * AND ONE OF THEM IS A FLOAT CLAIM RATHER THAN AN ALGEBRA CLAIM. `ToksvigA2(a2, 1) == a2` holds in
 * exact arithmetic, but a caller reaching it through `r*r`, `alpha*alpha` and two roots does not get
 * `r` back in binary32: measured, an UNFILTERED picture moved in its fourth decimal for this code
 * merely being present -- 0.056763454 where it had been 0.056763588. `RoughenedBy` takes the identity
 * as an identity instead, and that is asserted here as bit equality, because "close enough" is exactly
 * what the defect looked like. */
#include <cmath>
#include <cstdint>
#include <vector>

#include "Check.h"
#include "MetalRoughBrdf.h"
#include "TexelChain.h"

namespace {

/* Four unit normals splayed far apart, encoded as a normal map does. Their mean is SHORT, which is the
 * quantity the whole mechanism is about; a gentle set would leave nothing to carry. */
std::vector<float> DivergentDirections() {
  const float d[4][3] = {{0.9f, 0.0f, 0.436f},
                         {-0.9f, 0.0f, 0.436f},
                         {0.0f, 0.9f, 0.436f},
                         {0.0f, -0.9f, 0.436f}};
  std::vector<float> texels;
  for (const auto &v : d) {
    const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (int axis = 0; axis < 3; ++axis) { texels.push_back((v[axis] / length) * 0.5f + 0.5f); }
    texels.push_back(1.0f); /* nothing averaged yet, so the running length is 1 */
  }
  return texels;
}

std::vector<float> OneDirectionFourTimes() {
  std::vector<float> texels;
  for (int texel = 0; texel < 4; ++texel) {
    texels.push_back(0.5f);
    texels.push_back(0.5f);
    texels.push_back(1.0f); /* +z */
    texels.push_back(1.0f);
  }
  return texels;
}

} // namespace

int main() {
  using namespace outshine::Test;
  using outshine::Render::HalveInPlace;
  using outshine::Render::RoughenedBy;
  using outshine::Render::TexelKind;
  using outshine::Render::ToksvigA2;

  /* THE THREE LIMITS, AS VALUES. */
  for (const double a2 : {0.0, 0.01, 0.25, 0.5, 1.0}) {
    CHECK(ToksvigA2(a2, 1.0) == a2,
          "a texel whose normals never diverged is unchanged: the length is 1 and the factor is the "
          "identity, so a chain that is never read cannot change a picture");
  }
  Note("a mirror under a fully cancelled texel", ToksvigA2(0.0, 0.0), "alpha squared");
  CHECK(std::fabs(ToksvigA2(0.0, 0.0) - 1.0) < 1e-12,
        "normals that cancel entirely leave no direction to be smooth about, so even a mirror reads as "
        "fully rough -- the limit the direct form reaches only through a division by zero");
  Note("a mirror at half length", ToksvigA2(0.0, 0.5), "alpha squared");
  CHECK(ToksvigA2(0.0, 0.5) > 0.0 && ToksvigA2(0.0, 0.5) < 1.0,
        "a mirror whose normals merely spread is neither a mirror nor fully rough, and it is finite -- "
        "which is the case the unbounded Blinn-Phong exponent cannot express without a clamp");

  /* MONOTONE IN THE RIGHT DIRECTION, over the whole range rather than at a sample. */
  bool monotone = true;
  double previous = ToksvigA2(0.04, 1.0);
  for (int step = 19; step >= 0; --step) {
    const double next = ToksvigA2(0.04, (double)step / 20.0);
    monotone = monotone && next >= previous;
    previous = next;
  }
  CHECK(monotone, "roughness only ever rises as the averaged normal shortens: lost perturbation is "
                  "returned as spread and never taken away as smoothness");

  /* THE FLOAT CLAIM, AS BIT EQUALITY. */
  bool exact = true;
  for (const double roughness : {0.0, 0.05, 0.3, 0.5, 0.75, 1.0}) {
    exact = exact && RoughenedBy(roughness, 1.0) == roughness;
  }
  CHECK(exact, "an untouched texel returns its roughness BIT FOR BIT rather than nearly: the round trip "
               "through two squarings and two roots is what moved an unfiltered picture in its fourth "
               "decimal, and 'nearly' is precisely what that defect looked like");

  /* THE CHAIN'S HALF: the length is recorded, and it ACCUMULATES rather than being remeasured. */
  uint32_t width = 0, height = 0;
  std::vector<float> level;

  HalveInPlace(OneDirectionFourTimes(), 2, 2, level, width, height, TexelKind::Direction);
  Note("length after halving four identical normals", (double)level[3], "unit lengths");
  CHECK(std::fabs((double)level[3] - 1.0) < 1e-6,
        "normals that agree lose nothing, so a flat region of a map carries no correction at any level");

  HalveInPlace(DivergentDirections(), 2, 2, level, width, height, TexelKind::Direction);
  const double afterOne = (double)level[3];
  Note("length after halving four divergent normals", afterOne, "unit lengths");
  CHECK(afterOne < 0.5,
        "divergent normals average short, and the shortfall is kept instead of being renormalised away "
        "-- a term computed and then discarded is how a term becomes unnamed");

  /* A SECOND HALVING OF ALREADY-RENORMALISED TEXELS, which is what every level above the first is. The
   * four are unit again, so a length remeasured there would report 1 and declare the chain flat. */
  std::vector<float> stacked;
  for (int texel = 0; texel < 4; ++texel) {
    for (int channel = 0; channel < 4; ++channel) { stacked.push_back(level[(size_t)channel]); }
  }
  uint32_t stackedWidth = 0, stackedHeight = 0;
  std::vector<float> deeper;
  HalveInPlace(stacked, 2, 2, deeper, stackedWidth, stackedHeight, TexelKind::Direction);
  Note("length one level deeper", (double)deeper[3], "unit lengths");
  CHECK(std::fabs((double)deeper[3] - afterOne) < 1e-6,
        "the running length is carried through a level whose texels are unit again, so a deep level "
        "reports the divergence of the WHOLE chain under it and not of its last halving alone");

  Covers("board:1130 the mip chain keeps the mean resultant length of the normals it averaged, and the "
         "BRDF returns it as roughness by Toksvig's factor -- with the l = 1 identity exact in floats "
         "rather than nearly, and the length accumulating rather than being remeasured");
  return Report();
}
