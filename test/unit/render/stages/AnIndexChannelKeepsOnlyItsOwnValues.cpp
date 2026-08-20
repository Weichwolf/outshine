#include <cmath>
#include <cstdint>
#include <vector>

#include "Check.h"
#include "TexelChain.h"

namespace {

std::vector<float> MixedChannels() {
  return {0.0f, 0.0f, 0.0f, 1.0f,
          1.0f, 0.0f, 0.5f, 1.0f,
          1.0f, 1.0f, 1.0f, 1.0f,
          1.0f, 1.0f, 1.0f, 1.0f};
}

std::vector<float> ColumnsExchanged(const std::vector<float> &block) {
  std::vector<float> mirrored(block.size(), 0.0f);
  for (size_t row = 0; row < 2; ++row) {
    for (size_t column = 0; column < 2; ++column) {
      for (size_t channel = 0; channel < 4; ++channel) {
        mirrored[(row * 2 + column) * 4 + channel] = block[(row * 2 + (1 - column)) * 4 + channel];
      }
    }
  }
  return mirrored;
}

}

int main() {
  using namespace outshine::Test;
  using outshine::Render::HalveInPlace;
  using outshine::Render::IndexChannelsOf;
  using outshine::Render::TexelKind;

  const std::vector<float> block = MixedChannels();

  const uint32_t mask = IndexChannelsOf(block);
  Note("index channels found", (double)mask, "bitmask");
  CHECK((mask & 1u) != 0u, "a channel taking two values is an index: it carries a choice between two "
                           "materials, not a measured amount");
  CHECK((mask & 2u) != 0u, "a two-valued channel is an index however its texels are split, so the 2-2 "
                           "case is classified by its population and not by its arrangement");
  CHECK((mask & 4u) == 0u,
        "a channel taking THREE values is a quantity and is filtered as one -- the predicate's domain is "
        "two values and a wider one would need a threshold, which this rule deliberately does not have");
  CHECK((mask & 8u) != 0u, "a constant channel is an index, where snapping and averaging agree anyway");

  uint32_t width = 0, height = 0;
  std::vector<float> level;

  HalveInPlace(block, 2, 2, level, width, height, TexelKind::Value, mask);
  CHECK(width == 1 && height == 1, "halving a 2x2 gives one texel, so the chain descends");
  Note("index channel with a 3-1 majority", (double)level[0], "linear");
  Note("index channel with a 2-2 split", (double)level[1], "linear");
  Note("quantity channel of the same block", (double)level[2], "linear");
  CHECK(std::fabs((double)level[0] - 1.0) < 1e-6,
        "an index channel returns a value its four source texels contain: the 3-1 majority is 1, where "
        "the mean would be 0.75 -- a material the asset does not declare");
  CHECK(std::fabs((double)level[1] - 0.0) < 1e-6,
        "a 2-2 split resolves to the smaller value by a declared rule, where the mean would be 0.5 -- "
        "exactly the half-metal that saturated six render tails");
  CHECK(std::fabs((double)level[2] - 0.625) < 1e-6,
        "a quantity channel of the SAME block still takes the plain mean, so the index rule is applied "
        "per channel and does not quantise a texture because one of its channels is binary");

  std::vector<float> averaged;
  HalveInPlace(block, 2, 2, averaged, width, height, TexelKind::Value, 0u);
  Note("same channels with no index mask, 3-1", (double)averaged[0], "linear");
  Note("same channels with no index mask, 2-2", (double)averaged[1], "linear");
  CHECK(std::fabs((double)averaged[0] - 0.75) < 1e-6 && std::fabs((double)averaged[1] - 0.5) < 1e-6,
        "the plain mean of a two-valued channel is a value it never takes, so the check above is a "
        "difference and not a restatement of the input");

  const std::vector<float> mirrored = ColumnsExchanged(block);
  CHECK(IndexChannelsOf(mirrored) == mask, "the predicate counts a population, so exchanging columns "
                                           "does not change which channels are an index");
  std::vector<float> mirroredLevel;
  HalveInPlace(mirrored, 2, 2, mirroredLevel, width, height, TexelKind::Value, mask);
  CHECK(std::fabs((double)mirroredLevel[0] - (double)level[0]) < 1e-6 &&
            std::fabs((double)mirroredLevel[1] - (double)level[1]) < 1e-6,
        "a mirrored block filters to the same texel, so an island and its mirror image cannot take "
        "different materials from one map -- which is the case this tree already renders twice");

  Covers("a channel taking at most two values is an index, measured from the texels rather "
         "than read off the slot, and it snaps to a value the source contains instead of averaging into "
         "one it does not");
  return Report();
}
