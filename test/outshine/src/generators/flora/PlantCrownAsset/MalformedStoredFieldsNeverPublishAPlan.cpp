#include "CrownAssetFixture.h"
#include "PlantCrownAsset.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
struct Patch {
  size_t Offset = 0;
  uint64_t Value = 0;
  size_t Bytes = 4;
};

void Word(std::vector<uint8_t> &bytes, Patch patch) {
  for (size_t at = 0; at < patch.Bytes; ++at) {
    bytes[patch.Offset + at] = static_cast<uint8_t>(patch.Value >> (8 * at));
  }
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto source = CrownAssetFixture();
  if (!source) { return 1; }
  const auto encoded = EncodePlantCrownPlan(*source, kCrownAssetBudget);
  if (!encoded) { return 1; }
  constexpr size_t kHeaderBytes = 152;
  constexpr size_t kNodeBytes = 28;
  constexpr size_t kHostFirst = kHeaderBytes + 5 * kNodeBytes;
  constexpr size_t kVariantFirst = kHostFirst + 3 * sizeof(uint32_t);
  const std::array<Patch, 10> edits{{{.Offset = 0, .Value = 0},
                                     {.Offset = 8, .Value = std::numeric_limits<uint32_t>::max()},
                                     {.Offset = 12, .Value = 0},
                                     {.Offset = 24, .Value = 0},
                                     {.Offset = 84, .Value = 6},
                                     {.Offset = kHeaderBytes + 24, .Value = 0},
                                     {.Offset = kHostFirst, .Value = 0},
                                     {.Offset = kHostFirst, .Value = 99},
                                     {.Offset = kHostFirst + 4, .Value = 2},
                                     {.Offset = kVariantFirst, .Value = 64, .Bytes = 1}}};
  for (const auto edit : edits) {
    auto changed = *encoded;
    Word(changed, edit);
    CHECK(!DecodePlantCrownPlan(changed, kCrownAssetBudget),
          "unsupported format/count/stop, invalid parent/host and unknown variants return no plan");
  }
  auto nonfinite = *encoded;
  constexpr uint64_t kQuietNan = 0x7ff8000000000000ULL;
  Word(nonfinite, {.Offset = kHeaderBytes, .Value = kQuietNan, .Bytes = 8});
  CHECK(!DecodePlantCrownPlan(nonfinite, kCrownAssetBudget),
        "nonfinite source positions are refused");
  auto trailing = *encoded;
  trailing.push_back(0);
  CHECK(!DecodePlantCrownPlan(trailing, kCrownAssetBudget),
        "trailing bytes cannot hide a second payload");
  return Report();
}
