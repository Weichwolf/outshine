#include <cmath>
#include <cstdint>
#include <vector>

#include "Check.h"
#include "TexelChain.h"

namespace {

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

}

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

  HalveInPlace(source, 2, 2, level, width, height, TexelKind::Value);
  const double valueLength = LengthOf(level, 0);
  Note("same texels halved as VALUES, length", valueLength, "unit lengths");
  CHECK(valueLength < 0.5,
        "a value chain is the plain mean and is NOT renormalised, so the same input comes back short -- "
        "an implementation that renormalised everything would corrupt metallic-roughness and occlusion");

  const std::vector<float> values = {0.0f, 0.25f, 0.5f, 1.0f, 0.5f, 0.25f, 0.5f, 1.0f,
                                     1.0f, 0.75f, 0.5f, 1.0f, 0.5f, 0.75f, 0.5f, 1.0f};
  HalveInPlace(values, 2, 2, level, width, height, TexelKind::Value);
  Note("value texel red after halving", (double)level[0], "linear");
  CHECK(std::fabs((double)level[0] - 0.5) < 1e-6, "a value level is the box mean of the four above it");
  CHECK(std::fabs((double)level[1] - 0.5) < 1e-6, "and every channel takes the same mean");

  Covers("the mip chain is built here rather than by the device generator, which cannot be "
         "told a texel is a direction: a direction level is renormalised and a value level is not");
  return Report();
}
