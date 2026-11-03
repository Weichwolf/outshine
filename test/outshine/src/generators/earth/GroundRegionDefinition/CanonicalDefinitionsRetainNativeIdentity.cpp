#include "GroundRegionDefinition.h"
#include "TerrainLoader.h"
#include "Check.h"
#include "Sha256.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

GroundRegionDefinition Example() {
  return {.Coverage = {.LatitudeDeg = 12.25, .LongitudeDeg = -45.5, .Zoom = 14, .Levels = 2},
          .Anchor = {.LongitudeDeg = -2, .LatitudeDeg = 1},
          .EyeM = {123.25, -456.5, 78.125},
          .YfovRad = 1,
          .HeightPx = 720,
          .ErrorPx = 0.5,
          .PagesMost = 32768};
}

void CanonicalBytes() {
  const auto definition = Example();
  const auto encoded = EncodeGroundRegionDefinition(definition);
  CHECK(encoded && encoded->size() == 109 &&
            Sha256Hex(encoded->data(), encoded->size()) ==
                "f12efea9d4b4795b539c72867113c8916da896bcf5732108d9baa0552ea86abd",
        "definition retains the frozen legacy C++ and independent Python byte vector");
  if (!encoded) { return; }
  auto changed = definition;
  changed.Coverage.Access = TerrainTileAccess::Inspect;
  CHECK(EncodeGroundRegionDefinition(changed) == encoded,
        "source access policy is not a different native product");
  changed.DeclaredDigest = "";
  const auto declaredEmpty = EncodeGroundRegionDefinition(changed);
  CHECK(declaredEmpty && declaredEmpty->size() == encoded->size() + sizeof(uint32_t) &&
            *declaredEmpty != *encoded,
        "an explicitly empty declared binding differs from an absent binding");
  changed.DeclaredDigest = "native-fixture";
  const auto declared = EncodeGroundRegionDefinition(changed);
  CHECK(declared && declared->size() == encoded->size() + sizeof(uint32_t) + 14,
        "declared provenance retains its original bounded text encoding");
  changed.DeclaredDigest = std::string(4096, 'x');
  CHECK(!EncodeGroundRegionDefinition(changed), "oversized definitions fail before lookup");
}

void IndependentDemand() {
  const auto definition = Example();
  const auto encoded = EncodeGroundRegionDefinition(definition);
  std::array<GroundRegionDefinition, 7> changed;
  changed.fill(definition);
  changed[0].EyeM[0] += 1;
  changed[1].YfovRad *= 0.5;
  changed[2].HeightPx *= 2;
  changed[3].ErrorPx *= 0.5;
  changed[4].PagesMost /= 2;
  changed[5].Quality = GroundQuality::Playable;
  changed[6].Projection = GroundProjection::Orthographic;
  changed[6].YMagM = 25;
  for (const auto &value : changed) {
    const auto result = EncodeGroundRegionDefinition(value);
    CHECK(result && encoded && *result != *encoded,
          "existing camera, quality, error and page guarantees remain in identity");
  }
}

void InvalidDemand() {
  const auto definition = Example();
  std::array<GroundRegionDefinition, 7> invalid;
  invalid.fill(definition);
  invalid[0].EyeM[1] = std::numeric_limits<double>::quiet_NaN();
  invalid[1].Projection = static_cast<GroundProjection>(255);
  invalid[2].Quality = static_cast<GroundQuality>(255);
  invalid[3].HeightPx = 0;
  invalid[4].ErrorPx = 0;
  invalid[5].PagesMost = 0;
  invalid[6].Projection = GroundProjection::Orthographic;
  for (const auto &value : invalid) {
    CHECK(!EncodeGroundRegionDefinition(value),
          "invalid projection and nonfinite or unbounded demand cannot acquire a cache identity");
  }
  CHECK(::outshine::Ground::kStreamGrid == 2 * kPatchGrid,
        "a stream one zoom below the patchwork preserves the same metric posting");
}
}

int main() {
  CanonicalBytes();
  IndependentDemand();
  InvalidDemand();
  return outshine::Test::Report();
}
