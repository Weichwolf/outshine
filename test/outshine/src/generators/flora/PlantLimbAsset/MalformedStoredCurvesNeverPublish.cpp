#include "Check.h"
#include "PlantLimbAsset.h"
#include "PlantLimbPlan.h"
#include "ByteArchive.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace {
template <typename Number> void Replace(std::vector<uint8_t> &bytes, size_t at, Number value) {
  const auto encoded = outshine::LittleEndian(value);
  std::memcpy(bytes.data() + at, &encoded, sizeof(encoded));
}

void CheckBrokenJoins() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr size_t kBudget = 4096;
  constexpr size_t kSecondControls = 16 + 4 + 3 * 4 + 18 * sizeof(double) + 2 * sizeof(double);
  constexpr size_t kSecondRadius = kSecondControls + 12 * sizeof(double);
  constexpr std::array anchors{
      PlantLimbAnchor{.Node = 3, .PositionM = {{0, 0, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 19, .PositionM = {{0, 1, 0}}, .RadiusM = 0.05},
      PlantLimbAnchor{.Node = 81, .PositionM = {{0, 2, 0}}, .RadiusM = 0.02}};
  const auto plan = PlantLimbPlan::Create(anchors);
  CHECK(plan, "joined source control prepares before byte corruption");
  if (!plan) { return; }
  const auto encoded = EncodePlantLimbs(std::span(&*plan, 1), kBudget);
  CHECK(encoded && DecodePlantLimbs(*encoded, kBudget), "unmodified joined coefficients restore");
  if (!encoded) { return; }
  auto broken = *encoded;
  Replace(broken, kSecondControls, 0.25);
  CHECK(!DecodePlantLimbs(broken, kBudget),
        "discontinuous source positions cannot enter a raw asset");
  broken = *encoded;
  Replace(broken, kSecondControls + 3 * sizeof(double), 0.05);
  CHECK(!DecodePlantLimbs(broken, kBudget),
        "individually forward cubics still need a continuous tangent");
  broken = *encoded;
  Replace(broken, kSecondRadius, 0.1);
  CHECK(!DecodePlantLimbs(broken, kBudget), "positive radius intervals cannot hide a seam");
  broken = *encoded;
  Replace(broken, kSecondRadius + sizeof(double), 0.01);
  CHECK(!DecodePlantLimbs(broken, kBudget), "radius derivatives must agree at the shared knot");
}
}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr size_t kBudget = 4096;
  constexpr size_t kCount = 16, kNodes = kCount + 4, kSegment = kNodes + 2 * 4;
  constexpr size_t kControls = kSegment + 2 * sizeof(double);
  constexpr size_t kRadius = kControls + 12 * sizeof(double);
  constexpr std::array anchors{
      PlantLimbAnchor{.Node = 3, .PositionM = {{0, 0, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 19, .PositionM = {{0, 1, 0}}, .RadiusM = 0.05}};
  auto plan = PlantLimbPlan::Create(anchors);
  CHECK(plan, "valid source control exists");
  if (!plan) { return Report(); }
  std::vector<PlantLimbPlan> source;
  source.push_back(std::move(*plan));
  const auto encoded = EncodePlantLimbs(source, kBudget);
  CHECK(encoded, "valid reference bytes exist before corruption");
  if (!encoded) { return Report(); }
  auto broken = *encoded;
  broken.pop_back();
  CHECK(!DecodePlantLimbs(broken, kBudget), "truncation cannot publish a partially read plan");
  broken = *encoded;
  broken.push_back(0);
  CHECK(!DecodePlantLimbs(broken, kBudget), "trailing fields cannot silently change the format");
  broken = *encoded;
  Replace(broken, 8, std::numeric_limits<uint32_t>::max());
  CHECK(!DecodePlantLimbs(broken, kBudget), "hostile limb counts fail before vector allocation");
  broken = *encoded;
  Replace(broken, kCount, uint32_t{66});
  CHECK(!DecodePlantLimbs(broken, kBudget), "per-limb counts cannot exceed the prepared header");
  broken = *encoded;
  Replace(broken, kNodes + 4, anchors[0].Node);
  CHECK(!DecodePlantLimbs(broken, kBudget), "duplicate source IDs cannot forge a valid path");
  broken = *encoded;
  Replace(broken, kSegment, std::numeric_limits<double>::quiet_NaN());
  CHECK(!DecodePlantLimbs(broken, kBudget), "nonfinite parameter intervals cannot be restored");
  broken = *encoded;
  Replace(broken, kControls + 3 * sizeof(double) + sizeof(double), -1.0);
  CHECK(!DecodePlantLimbs(broken, kBudget),
        "backward derivative controls cannot enter a cached plan");
  broken = *encoded;
  Replace(broken, kRadius, 1.0);
  Replace(broken, kRadius + sizeof(double), -8.0);
  Replace(broken, kRadius + 2 * sizeof(double), 8.0);
  Replace(broken, kRadius + 3 * sizeof(double), 0.0);
  CHECK(!DecodePlantLimbs(broken, kBudget),
        "positive endpoints do not conceal a negative radius inside the interval");
  broken = *encoded;
  Replace(broken, 0, uint64_t{0});
  CHECK(!DecodePlantLimbs(broken, kBudget), "unknown wire versions do not reinterpret old fields");
  CheckBrokenJoins();
  return Report();
}
