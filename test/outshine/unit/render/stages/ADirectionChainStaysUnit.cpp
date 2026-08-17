/* THE CLAIM THAT JUSTIFIED BUILDING THE MIP CHAIN OURSELVES (board:1130). The device's own generator
 * would have been free, and it was refused for one reason: it has no way to be told that a texel is a
 * direction, so it averages a normal map without renormalising. That is a different picture arrived at
 * silently — no flag records it, no number reports it, and the surface simply shades flatter with
 * distance than the map declares.
 *
 * SO THE THING TO TEST IS NOT THAT A CHAIN EXISTS. It is that the two kinds are treated differently and
 * that the difference is the right one: a DIRECTION comes back unit at every level, and a VALUE comes
 * back as the plain mean and is NOT renormalised. Testing only the first would pass on an
 * implementation that renormalised everything, which would corrupt the metallic-roughness and occlusion
 * maps in exactly the way `TexelKind` exists to prevent.
 *
 * IT NEEDS NO DEVICE, which is why `HalveInPlace` lives in a header rather than in its caller's
 * anonymous namespace: arrays in, arrays out. Left where it was, this claim could only have been a
 * comment. */
#include <cmath>
#include <cstdint>
#include <vector>

#include "Check.h"
#include "TexelChain.h"

namespace {

/* Texels encoded the way a normal map is: a unit direction mapped into [0,1]. The four here point far
 * apart on purpose — the mean of two divergent unit vectors is SHORT, which is the whole hazard, and a
 * gentle set would pass even without renormalising. */
std::vector<float> DivergentDirections() {
  const float d[4][3] = {{0.9f, 0.0f, 0.436f},
                         {-0.9f, 0.0f, 0.436f},
                         {0.0f, 0.9f, 0.436f},
                         {0.0f, -0.9f, 0.436f}};
  std::vector<float> texels;
  for (const auto &v : d) {
    float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (int axis = 0; axis < 3; ++axis) { texels.push_back((v[axis] / length) * 0.5f + 0.5f); }
    texels.push_back(1.0f);
  }
  return texels;
}

double LengthOf(const std::vector<float> &texels, size_t at) {
  double length = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double component = (double)texels[at + (size_t)axis] * 2.0 - 1.0;
    length += component * component;
  }
  return std::sqrt(length);
}

} // namespace

int main() {
  using namespace outshine::Test;
  using outshine::Render::HalveInPlace;
  using outshine::Render::TexelKind;

  const std::vector<float> source = DivergentDirections();
  uint32_t width = 0, height = 0;
  std::vector<float> level;

  HalveInPlace(source, 2, 2, level, width, height, TexelKind::Direction);
  CHECK(width == 1 && height == 1, "halving a 2x2 gives one texel, so the chain descends");
  const double directionLength = LengthOf(level, 0);
  Note("direction texel length after halving", directionLength, "unit lengths");
  CHECK(std::fabs(directionLength - 1.0) < 1e-6,
        "a direction chain stays unit: the mean of divergent unit vectors is short, and renormalising "
        "is what keeps the level a direction rather than a shorter one that shades flatter");

  /* WITHOUT THE RENORMALISATION THE SAME INPUT IS MEASURABLY SHORT, which is what makes the check
   * above a claim rather than a tautology: the plain mean of these four is well below unit. */
  HalveInPlace(source, 2, 2, level, width, height, TexelKind::Value);
  const double valueLength = LengthOf(level, 0);
  Note("same texels halved as VALUES, length", valueLength, "unit lengths");
  CHECK(valueLength < 0.5,
        "a value chain is the plain mean and is NOT renormalised, so the same input comes back short -- "
        "an implementation that renormalised everything would corrupt metallic-roughness and occlusion");

  /* A VALUE IS THE MEAN, EXACTLY. Four texels whose mean is known by hand, so the box filter is held to
   * arithmetic rather than to being merely unchanged. */
  const std::vector<float> values = {0.0f, 0.25f, 0.5f, 1.0f, 0.5f, 0.25f, 0.5f, 1.0f,
                                     1.0f, 0.75f, 0.5f, 1.0f, 0.5f, 0.75f, 0.5f, 1.0f};
  HalveInPlace(values, 2, 2, level, width, height, TexelKind::Value);
  Note("value texel red after halving", (double)level[0], "linear");
  CHECK(std::fabs((double)level[0] - 0.5) < 1e-6, "a value level is the box mean of the four above it");
  CHECK(std::fabs((double)level[1] - 0.5) < 1e-6, "and every channel takes the same mean");

  Covers("board:1130 the mip chain is built here rather than by the device generator, which cannot be "
         "told a texel is a direction: a direction level is renormalised and a value level is not");
  return Report();
}
