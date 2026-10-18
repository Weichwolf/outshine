#include "local_light_world_build.h"
#include "local_light_build.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>
#include <span>
#include <string_view>

namespace {
using LightWorld::Builder;
using LightWorld::Light;
using LightWorld::Lookup;
constexpr float kNearPositiveCellEdge = 127.75f;
constexpr float kNegativePosition = -129;
constexpr float kInverseRadius = 0.25f;
constexpr float kPhotometryChange = 0.125f;
constexpr float kOutsidePositionDomain = 0x1p38f;
constexpr size_t kIndexBudget = size_t{1024} * 1024;
constexpr LightWorld::Vector kEarthTranslation{6'000'000, -4'000'000, 3'200'000};
constexpr std::array kMovedPositions{-256.25f, 512.25f, kNearPositiveCellEdge};

struct Checks {
  size_t Passed = 0, Failed = 0;

  void Expect(bool actual, std::string_view name) {
    if (actual) {
      ++Passed;
    } else {
      ++Failed;
      std::println("FAIL {}", name);
    }
  }
};

Light Point(float x = kNearPositiveCellEdge) {
  return {1, 2, 3, 1, x, 0, 0, kInverseRadius, 0, -1, 0, 0, 0.5f, 2, 0, 0};
}

void Compare(Checks &check, Builder &builder, Lookup lookup) {
  Builder full;
  check.Expect(full.Build(builder.Source, builder.Width, builder.IndexBytes, builder.Translation)
                   .has_value(),
               "full rebuild");
  check.Expect(full.Pack(lookup).has_value(), "full pack");
  check.Expect(builder.Pack(lookup).has_value(), "incremental pack");
  check.Expect(builder.Words == full.Words && builder.Local == full.Local &&
                   builder.Coordinates == full.Coordinates,
               "all output bytes match independent full rebuild");
}

void Changes(Checks &check, Lookup lookup) {
  Builder builder;
  std::array source{Point(), Point(kNegativePosition)};
  source[1][7] = 0;
  check.Expect(builder.Build(source, 32, kIndexBudget, kEarthTranslation).has_value(),
               "initial build");
  Compare(check, builder, lookup);
  for (const size_t component : {size_t{0}, size_t{3}, size_t{8}, size_t{12}}) {
    auto changed = builder.Source[0];
    changed[component] = component == 3 ? 2 : changed[component] + kPhotometryChange;
    check.Expect(builder.Update(0, changed).has_value(), "photometry edit");
    Compare(check, builder, lookup);
    check.Expect(builder.PackedWordWrites == 0 && builder.PackRebuilds == 0,
                 "photometry retains index without word stores");
  }
  auto global = builder.Source[1];
  global[4] += 1024;
  check.Expect(builder.Update(1, global).has_value(), "global position cell edit");
  Compare(check, builder, lookup);
  check.Expect(builder.PackedWordWrites == 1 && builder.PackRebuilds == 0,
               "only changed global coordinate word stored");
  const auto allocations = builder.Heap.Allocations;
  const auto writes = builder.PackedWordWrites;
  check.Expect(builder.Pack(lookup).has_value(), "repeat unchanged pack");
  check.Expect(builder.Heap.Allocations == allocations && builder.PackedWordWrites == writes,
               "unchanged pack allocates and writes nothing");
  for (const float position : kMovedPositions) {
    auto moved = builder.Source[0];
    moved[4] = position;
    check.Expect(builder.Update(0, moved).has_value(), "spatial edit");
    global[5] -= 256;
    check.Expect(builder.Update(1, global).has_value(), "batched coordinate edit");
    Compare(check, builder, lookup);
    check.Expect(builder.PackRebuilds == 1, "batched membership edit rebuilds");
  }
  const Lookup other = lookup == Lookup::Hash ? Lookup::Sorted : Lookup::Hash;
  Compare(check, builder, other);
  Compare(check, builder, lookup);
  const auto original = builder.Words;
  auto invalid = builder.Source[0];
  invalid[0] = std::numeric_limits<float>::quiet_NaN();
  check.Expect(!builder.Update(0, invalid), "invalid edit refused");
  check.Expect(!builder.Update(static_cast<uint32_t>(source.size()), source[0]),
               "unknown original ID refused");
  invalid = builder.Source[0];
  invalid[4] = kOutsidePositionDomain;
  check.Expect(!builder.Update(0, invalid), "coordinate overflow refused");
  check.Expect(!builder.Build(std::span{&invalid, size_t{1}}, 32, 1024, {}),
               "invalid rebuild refused");
  check.Expect(builder.Words == original, "refusals preserve current packed bytes");
  Compare(check, builder, lookup);
}

void EmptyAndOverflow(Checks &check, Lookup lookup) {
  Builder builder;
  check.Expect(builder.Build({}, 32, 1, {}).has_value(), "empty build");
  Compare(check, builder, lookup);
  std::array<Light, 1> source{Point()};
  check.Expect(builder.Build(source, 32, 1, {}).has_value(), "bounded fallback build");
  Compare(check, builder, lookup);
  check.Expect(builder.Overflow, "actual complete fallback");
  source[0][4] = -256;
  check.Expect(builder.Update(0, source[0]).has_value(), "fallback coordinate edit");
  Compare(check, builder, lookup);
  check.Expect(builder.PackedWordWrites == 1 && builder.PackRebuilds == 0,
               "fallback patches only changed coordinate word");
  check.Expect(builder.Build(source, 64, kIndexBudget, {}).has_value(), "new spatial build");
  source[0][0] += 3;
  check.Expect(builder.Update(0, source[0]).has_value(), "edit before initial pack");
  Compare(check, builder, lookup);
}
}

int main() {
  Checks check;
  for (const auto lookup : {Lookup::Hash, Lookup::Sorted}) {
    Changes(check, lookup);
    EmptyAndOverflow(check, lookup);
  }
  std::println("{} PASS / {} FAIL", check.Passed, check.Failed);
  return check.Failed == 0 ? 0 : 1;
}
