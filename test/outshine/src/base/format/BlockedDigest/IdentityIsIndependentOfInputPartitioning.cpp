#include "BlockedDigest.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::array<uint8_t, 4093> block;
  for (size_t index = 0; index < block.size(); ++index) {
    block[index] = static_cast<uint8_t>(index);
  }
  BlockedDigest continuous, partitioned, changed;
  constexpr size_t repeats = 17000;
  for (size_t index = 0; index < repeats; ++index) {
    CHECK(continuous.Put(block), "continuous feed accepts source bytes");
    CHECK(partitioned.Put(std::span(block).first(17)) &&
              partitioned.Put(std::span(block).subspan(17)),
          "arbitrary producer boundaries do not consume a whole-plan buffer");
    auto altered = block;
    if (index == repeats - 1) { altered.back() ^= 1u; }
    CHECK(changed.Put(altered), "late source changes remain part of the identity");
  }
  CHECK(continuous.Count() > size_t{64} * 1024 * 1024 && continuous.Count() == partitioned.Count(),
        "realistic dense plans can exceed the asset package byte budget");
  const auto digest = continuous.Finish();
  CHECK(digest == partitioned.Finish() && digest != changed.Finish(),
        "streaming identity preserves every byte independently of producer partitioning");
  CHECK(continuous.Finish() == digest, "finishing the same complete byte stream is stable");
  return Report();
}
