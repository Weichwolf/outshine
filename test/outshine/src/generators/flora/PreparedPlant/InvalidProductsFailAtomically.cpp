#include "PreparedPlantFixture.h"
#include "PreparedPlant.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr size_t kBudget = 16384;
  auto source = PreparedPlantFixture();
  const auto encoded = EncodePreparedPlant(source, kBudget);
  CHECK(encoded, "valid complete fixture encodes");
  if (!encoded) { return Report(); }
  CHECK(EncodePreparedPlant(source, encoded->size()), "exact encoded budget succeeds");
  CHECK(!EncodePreparedPlant(source, encoded->size() - 1), "short encode budget publishes nothing");
  CHECK(!DecodePreparedPlant(*encoded, encoded->size() - 1),
        "oversized input is refused before decode");
  bool truncated = true;
  for (size_t size = 0; size < encoded->size(); ++size) {
    truncated = truncated && !DecodePreparedPlant(std::span(*encoded).first(size), kBudget);
  }
  CHECK(truncated, "every truncated payload is rejected without a partial product");
  auto bytes = *encoded;
  bytes.push_back(0);
  CHECK(!DecodePreparedPlant(bytes, kBudget), "trailing bytes are not silently accepted");
  bytes = *encoded;
  bytes[0] ^= 1;
  CHECK(!DecodePreparedPlant(bytes, kBudget), "unknown product versions are refused");
  bytes = *encoded;
  std::fill(bytes.end() - 8, bytes.end(), uint8_t{0});
  CHECK(!DecodePreparedPlant(bytes, kBudget), "decoded placements undergo semantic validation");
  bytes = *encoded;
  constexpr size_t kAxesCountOffset = 56;
  for (size_t at = kAxesCountOffset; at < kAxesCountOffset + 4; ++at) {
    bytes[at] = std::numeric_limits<uint8_t>::max();
  }
  CHECK(!DecodePreparedPlant(bytes, kBudget),
        "forged axis count cannot allocate beyond the payload");
  source.Axes[1].FirstIndex = 0;
  CHECK(!EncodePreparedPlant(source, kBudget), "overlapping wood ownership is invalid");
  source.Axes[1].FirstIndex = 3;
  source.Leaves[1].Ids[0] = source.Leaves[0].Ids[0];
  CHECK(!EncodePreparedPlant(source, kBudget),
        "leaf identities are unique across prototype groups");
  source.Leaves[1].Ids[0] = kLeafC;
  source.Leaves[1].Axes[0] = kUnknownAxis;
  CHECK(!EncodePreparedPlant(source, kBudget), "leaf owners must belong to the stored branch");
  source.Leaves[1].Axes[0] = kPlantBranch;
  source.Leaves[1].Transforms[0][0] = -1;
  CHECK(!EncodePreparedPlant(source, kBudget), "reflections cannot reverse leaf winding");
  source.Leaves[1].Transforms[0][0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!EncodePreparedPlant(source, kBudget), "nonfinite placements are rejected");
  source.Leaves[1].Transforms[0][0] = 1;
  source.Leaves[1].Transforms[0][3] = 1;
  CHECK(!EncodePreparedPlant(source, kBudget), "nonaffine leaf placements are rejected");
  source.Leaves[1].Transforms[0][3] = 0;
  source.Leaves[1].Variant = source.Leaves[0].Variant;
  CHECK(!EncodePreparedPlant(source, kBudget),
        "duplicate variant groups cannot split shared prototypes");
  source.Leaves[1].Variant = 7;
  CHECK(source.Prototypes.setNormals(1, {}), "the generic geometry owner permits optional normals");
  CHECK(!EncodePreparedPlant(source, kBudget),
        "a plant prototype requires its prepared lighting normals");
  return Report();
}
