#include <Geometry.h>
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <random>
#include <span>
#include <vector>

namespace {
using namespace outshine;
constexpr double kTolerance = 1.0e-3;

std::optional<GeometryAttributeError> Original(std::span<const float> values) {
  if (values.size() % 3 != 0) { return GeometryAttributeError::IncompleteTuple; }
  for (size_t at = 0; at < values.size(); at += 3) {
    const double length = std::sqrt(static_cast<double>(values[at]) * values[at] +
                                    static_cast<double>(values[at + 1]) * values[at + 1] +
                                    static_cast<double>(values[at + 2]) * values[at + 2]);
    if (!std::isfinite(length)) { return GeometryAttributeError::NonFiniteValue; }
    if (std::abs(length - 1.0) > kTolerance) { return GeometryAttributeError::NonUnitNormal; }
  }
  return std::nullopt;
}

bool Matches(Geometry &geometry, int part, std::span<const float> values) {
  const auto expected = Original(values);
  const auto result = geometry.setNormals(part, values);
  return expected ? !result && result.error() == *expected : result.has_value();
}

size_t Probe(int part, Geometry &geometry, uint64_t seed) {
  constexpr std::array<std::array<uint32_t, 3>, 7> boundary{
      std::array<uint32_t, 3>{1065336438u, 966800254u, 880787445u},
      {1065336438u, 966800254u, 880787446u},
      {0u, 0u, 0u},
      {0x3f800000u, 0u, 0u},
      {0x7f800000u, 0u, 0u},
      {0x7fc00001u, 0u, 0u},
      {0x7f7fffffu, 0x7f7fffffu, 0x7f7fffffu}};
  constexpr size_t kSamples = 65536;
  size_t mismatches = 0;
  for (const auto &bits : boundary) {
    const std::array values{std::bit_cast<float>(bits[0]),
                            std::bit_cast<float>(bits[1]),
                            std::bit_cast<float>(bits[2])};
    if (!Matches(geometry, part, values)) { ++mismatches; }
  }
  std::mt19937_64 random(seed);
  for (size_t sample = 0; sample < kSamples; ++sample) {
    std::array<float, 3> values{};
    for (float &value : values) { value = std::bit_cast<float>(static_cast<uint32_t>(random())); }
    if (sample % 4 == 0) {
      const double length = std::sqrt(static_cast<double>(values[0]) * values[0] +
                                      static_cast<double>(values[1]) * values[1] +
                                      static_cast<double>(values[2]) * values[2]);
      if (std::isfinite(length) && length > 0) {
        for (float &value : values) { value = static_cast<float>(value / length); }
      }
    }
    if (!Matches(geometry, part, values)) { ++mismatches; }
  }
  return mismatches;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Geometry geometry;
  const auto material = geometry.addSurface("surface", Material{});
  CHECK(material.has_value(), "the numeric validation target owns a material");
  if (!material) { return Report(); }
  const auto part = geometry.addPart("normal", *material);
  CHECK(part.has_value(), "the numeric validation target owns its part");
  if (!part) { return Report(); }
  const int savedRound = std::fegetround();
  constexpr std::array modes{FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
  const auto seed = std::random_device{}();
  std::println("NORMAL_SEED {}", seed);
  size_t mismatches = 0;
  for (const int mode : modes) {
    const int changed = std::fesetround(mode);
    CHECK(changed == 0, "the target supports the requested IEEE rounding mode");
    if (changed != 0) { continue; }
    mismatches += Probe(*part, geometry, seed);
  }
  CHECK(std::fesetround(savedRound) == 0, "the original rounding mode is restored");
  CHECK(mismatches == 0, "bit-range and threshold inputs preserve the original numeric decision");
  const std::array normal{0.0f, 1.0f, 0.0f};
  CHECK(geometry.setNormals(*part, normal),
        "a complete valid attribute is retained before failure");
  const auto *pointer = geometry.normalsOf(*part).data();
  const auto nan = std::bit_cast<float>(uint32_t{0x7fc00001});
  const std::array firstBad{0.0f, 0.0f, 2.0f, nan, 0.0f, 0.0f};
  const std::array firstNan{nan, 0.0f, 0.0f, 0.0f, 0.0f, 2.0f};
  const std::array incomplete{0.0f, 1.0f};
  CHECK(Matches(geometry, *part, firstBad) && Matches(geometry, *part, firstNan) &&
            Matches(geometry, *part, incomplete),
        "the first invalid normal and tuple error retain their original priority");
  CHECK(geometry.normalsOf(*part).data() == pointer &&
            std::ranges::equal(geometry.normalsOf(*part), normal),
        "every failure preserves the previously owned attribute");
  CHECK(geometry.setNormals(*part, {}) && geometry.normalsOf(*part).empty(),
        "the empty attribute remains a valid clear operation");
  return Report();
}
