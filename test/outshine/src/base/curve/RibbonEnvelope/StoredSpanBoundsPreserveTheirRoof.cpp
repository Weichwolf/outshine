#include "RibbonEnvelope.h"
#include "TriangleDistance.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <print>
#include <ranges>
#include <span>
#include <string_view>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Ring = std::array<Vec3f, 4>;

Vec3 Position(std::span<const Vec3f, 8> rings, uint8_t index) {
  const auto &p = rings[index];
  return {{p[0], p[1], p[2]}};
}

void Samples(std::span<const Vec3f, 8> rings, const RibbonEnvelope &envelope) {
  constexpr std::array<uint8_t, 18> fine{0, 4, 1, 1, 4, 5, 1, 5, 2, 2, 5, 6, 2, 6, 3, 3, 6, 7};
  const double scale = std::ranges::max(
      rings | std::views::transform([](const auto &p) {
        return std::max(
            {1., std::abs(double(p[0])), std::abs(double(p[1])), std::abs(double(p[2]))});
      }));
  const double slack = 256 * std::numeric_limits<double>::epsilon() * scale;
  for (size_t at = 0; at < fine.size(); at += 3) {
    const auto a = Position(rings, fine[at]);
    const auto b = Position(rings, fine[at + 1]);
    const auto c = Position(rings, fine[at + 2]);
    for (int x = 0; x <= 8; ++x) {
      for (int y = 0; y <= 8 - x; ++y) {
        const Vec3 p = a + (b - a) * (x / 8.) + (c - a) * (y / 8.);
        double nearest = std::numeric_limits<double>::infinity();
        for (size_t coarse = 0; coarse < envelope.Triangles.size(); coarse += 3) {
          const auto distance =
              BoundPointTriangleDistance(p,
                                         Position(rings, envelope.Triangles[coarse]),
                                         Position(rings, envelope.Triangles[coarse + 1]),
                                         Position(rings, envelope.Triangles[coarse + 2]));
          CHECK(distance, "an independent triangle query has finite source operands");
          if (distance) { nearest = std::min(nearest, distance->UpperDistanceM); }
        }
        CHECK(nearest <= envelope.PositionBoundM + slack,
              "the declared complete-source error encloses independent interior samples");
      }
    }
  }
}

void Controls() {
  const Ring first{{{{-2, -1, 0}}, {{-1, -.5f, 0}}, {{1, .5f, 0}}, {{2, 1, 0}}}};
  const Ring second{{{{-2, 1, 4}}, {{-1, .5f, 4}}, {{1, -.5f, 4}}, {{2, -1, 4}}}};
  std::array<Vec3f, 8> rings;
  std::ranges::copy(first, rings.begin());
  std::ranges::copy(second, rings.begin() + 4);
  for (const auto roof : {RibbonRoof::Upper, RibbonRoof::Lower}) {
    const auto result = BoundRibbonSpan(first, second, roof);
    CHECK(result && result->PositionBoundM >= 2 && result->PositionBoundM < 2.000001,
          "both saddle roofs have an outward finite geometric bound");
    if (!result) { continue; }
    const std::array<uint8_t, 2> diagonal =
        roof == RibbonRoof::Upper ? std::array<uint8_t, 2>{3, 4} : std::array<uint8_t, 2>{0, 7};
    for (const auto index : diagonal) {
      CHECK(std::ranges::count(result->Triangles, index) == 2,
            "the known saddle selects its upper or lower diagonal, not just either roof");
    }
    Samples(rings, *result);
    for (size_t at = 0; at < result->Triangles.size(); at += 3) {
      const Vec3 a = Position(rings, result->Triangles[at]);
      const Vec3 b = Position(rings, result->Triangles[at + 1]);
      const Vec3 c = Position(rings, result->Triangles[at + 2]);
      CHECK(Cross(b - a, c - a)[1] * (roof == RibbonRoof::Upper ? 1 : -1) > 0,
            "outer and underside triangles retain opposite winding");
    }
  }
  auto flatFirst = first;
  auto flatSecond = second;
  for (auto &p : flatFirst) { p[1] = 0; }
  for (auto &p : flatSecond) { p[1] = 0; }
  const auto flat = BoundRibbonSpan(flatFirst, flatSecond);
  CHECK(flat && flat->PositionBoundM < 1e-10,
        "an exactly horizontal roof remains an eligible source");
  CHECK(!BoundRibbonSpan(first, first), "a collapsed projected source cannot qualify");
  auto reversed = second;
  std::ranges::reverse(reversed);
  CHECK(!BoundRibbonSpan(first, reversed), "a folded source keeps its original");
  auto unordered = first;
  std::swap(unordered[1], unordered[2]);
  CHECK(!BoundRibbonSpan(unordered, second), "unordered transverse source points cannot qualify");
  auto invalid = first;
  invalid[1][1] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!BoundRibbonSpan(invalid, second), "nonfinite stored source words cannot qualify");
}

bool ReadStoredRings(std::span<Vec3f, 8> rings) {
  for (auto &p : rings) {
    for (float &v : p.Axis) {
      if (!(std::cin >> v)) { return false; }
    }
  }
  return true;
}

int Probe() {
  unsigned roof = 0;
  while (std::cin >> roof) {
    std::array<Vec3f, 8> rings;
    if (roof > 1 || !ReadStoredRings(rings)) { return 2; }
    const auto bound = BoundRibbonSpan(std::span<const Vec3f, 4>(rings.data(), 4),
                                       std::span<const Vec3f, 4>(rings.data() + 4, 4),
                                       roof == 0 ? RibbonRoof::Upper : RibbonRoof::Lower);
    std::print("{} ", bound.has_value() ? 1 : 0);
    for (const auto &p : rings) {
      for (const float v : p) { std::print("{} ", std::bit_cast<uint32_t>(v)); }
    }
    if (bound) {
      std::print("{:.17g} {:.17g}", bound->PositionBoundM, bound->RingResidualM);
      for (const auto index : bound->Triangles) { std::print(" {}", index); }
    }
    std::println();
  }
  return std::cin.eof() ? 0 : 2;
}
}

int main(int argc, const char *const *argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--stdin") { return Probe(); }
  Controls();
  return Report();
}
