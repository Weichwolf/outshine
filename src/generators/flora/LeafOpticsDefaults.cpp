#include "LeafOpticsDefaults.h"
#include "TreeSpecies.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr size_t kOwnedTableSide = 33;
constexpr std::array<LeafOpticsTable::Spectrum, kOwnedTableSide * kOwnedTableSide> kOwnedValues{{
#include "LeafOpticsData.inc"
}};
}

const LeafOpticsTable &DefaultLeafOptics() {
  static const LeafOpticsTable table = [] {
    auto made = LeafOpticsTable::Create(kOwnedTableSide, kOwnedValues);
    assert(made.has_value());
    return std::move(*made);
  }();
  return table;
}

namespace {
Vec3f DefaultChannels(size_t first) {
  const auto &shade = TreeSpecies::kShadingUnsaid;
  const auto sample = DefaultLeafOptics().Sample({shade.ChlorophyllUgCm2, shade.AnthocyaninUgCm2});
  assert(sample.has_value());
  return {{static_cast<float>((*sample)[first]),
           static_cast<float>((*sample)[first + 1]),
           static_cast<float>((*sample)[first + 2])}};
}
}

Vec3f DefaultLeafReflection() {
  return DefaultChannels(0);
}

Vec3f DefaultLeafTransmission() {
  return DefaultChannels(3);
}
}
