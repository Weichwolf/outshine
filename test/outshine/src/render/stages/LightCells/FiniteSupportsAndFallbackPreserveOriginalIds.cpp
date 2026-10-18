#include "LightCells.h"
#include "LightCellFrame.h"
#include "SubjectTypes.h"
#include "Check.h"
#include "math/Vec3.h"
#include "scene/PunctualLight.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kBytes = size_t{16} * 1024 * 1024;
constexpr LightCellLimits kLimits{.EmitterBytes = kBytes,
                                  .IndexBytes = kBytes,
                                  .ScratchBytes = kBytes,
                                  .SphereBoxTests = 1'000'000};
constexpr size_t kCount = 17;
constexpr double kEarthX = 6'000'000, kEarthY = -4'000'000, kEarthZ = 3'200'000;
constexpr double kSpacing = 3.125;
constexpr float kIntensity = 7.25f;

bool Included(const LightCells &data, const Vec3 &position, uint32_t id) {
  const auto &w = data.Words;
  const auto global = std::span(w).subspan(w[0], w[1]);
  for (const auto entry : global) {
    if (entry == id) { return true; }
  }
  std::array<uint32_t, 3> key{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const auto cell = static_cast<int32_t>(std::floor(position[axis] / LightCells::kLookupWidthM));
    key[axis] = std::bit_cast<uint32_t>(cell) ^ 0x80000000u;
  }
  for (size_t slot = 0; slot <= w[3]; ++slot) {
    const size_t at = w[2] + LightCells::kCellWords * slot;
    if (w[at] != key[0] || w[at + 1] != key[1] || w[at + 2] != key[2]) { continue; }
    for (const auto entry : std::span(w).subspan(w[at + 3], w[at + 4])) {
      if (entry == id) { return true; }
    }
  }
  return false;
}

std::array<SubjectLight, kCount> Sources() {
  std::array<SubjectLight, kCount> result{};
  for (size_t id = 0; id < result.size(); ++id) {
    auto &entry = result[id];
    entry.Light.Kind = id % 2 == 0 ? LightKind::Point : LightKind::Spot;
    entry.Light.Intensity = kIntensity;
    entry.Light.RangeM = static_cast<float>(8 + id % 4);
    entry.PositionEcefM = {{kEarthX + kSpacing * static_cast<double>(id),
                            kEarthY,
                            kEarthZ - kSpacing * static_cast<double>(id)}};
  }
  result[0].Light.Kind = LightKind::Directional;
  result.back().Light.RangeM = 0;
  return result;
}

void Ordinary() {
  const auto source = Sources();
  const auto data = BuildLightCells(source, kLimits);
  CHECK(data.has_value(), "seventeen original emitters produce native world-light data");
  if (!data) { return; }
  CHECK(!data->FullEnumeration && data->Words[6] == kCount && data->Words[7] == 1,
        "native v1 index keeps every original ID with spatial selection");
  CHECK(data->Words[1] == 2, "direction and range-zero emitters stay global");
  CHECK(data->Words[data->Words[0]] == 0 && data->Words[data->Words[0] + 1] == kCount - 1,
        "global IDs preserve original order and the seventeenth emitter");
  CHECK(data->Costs.ScratchBytes <= kLimits.ScratchBytes,
        "bounded native assignment scratch respects its declared bytes");
  CHECK(data->Costs.SphereBoxTests <= kLimits.SphereBoxTests,
        "both passes obey the declared work limit");
  for (uint32_t id = 0; id < source.size(); ++id) {
    CHECK(data->Emitters[id].Tint[0] == kIntensity, "photometric intensity is retained");
    CHECK(Included(*data, source[id].PositionEcefM, id), "each emitter covers its own position");
    for (size_t axis = 0; axis < 3; ++axis) {
      auto receiver = source[id].PositionEcefM;
      receiver[axis] += source[id].Light.RangeM * 0.5;
      CHECK(Included(*data, receiver, id), "finite interior support cannot lose an original ID");
      const auto cell = std::bit_cast<int32_t>(data->Words[data->Words[4] + 3 * id + axis]);
      const double reconstructed =
          cell * LightCells::kPositionWidthM + data->Emitters[id].Place[axis];
      CHECK(reconstructed == source[id].PositionEcefM[axis],
            "earth coordinates decompose without a camera-dependent emitter origin");
    }
  }
}

void BudgetsAndRefusals() {
  auto source = Sources();
  for (const auto limit : {LightCellLimits{.EmitterBytes = kBytes, .IndexBytes = kBytes},
                           LightCellLimits{.EmitterBytes = kBytes,
                                           .IndexBytes = kBytes,
                                           .ScratchBytes = kBytes,
                                           .SphereBoxTests = 1},
                           LightCellLimits{.EmitterBytes = kBytes,
                                           .IndexBytes = (18 + 4 * kCount) * sizeof(uint32_t),
                                           .ScratchBytes = kBytes,
                                           .SphereBoxTests = 1'000'000}}) {
    const auto data = BuildLightCells(source, limit);
    CHECK(data.has_value(), "small spatial budgets retain a valid complete light set");
    if (!data) { continue; }
    CHECK(data->FullEnumeration && data->Words[1] == kCount,
          "fallback keeps the full original light population");
    CHECK(data->Words.size() * sizeof(uint32_t) <= limit.IndexBytes &&
              data->Costs.ScratchBytes <= limit.ScratchBytes,
          "fallback retains declared index and scratch budgets");
    for (uint32_t id = 0; id < source.size(); ++id) {
      CHECK(data->Words[data->Words[0] + id] == id, "fallback original IDs have no truncation");
    }
  }
  CHECK(!BuildLightCells(source, {}), "insufficient complete storage is explicitly refused");
  source.back().PositionEcefM[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BuildLightCells(source, kLimits), "nonfinite world positions are refused");
  source = Sources();
  constexpr double kOutsidePositionDomain = 0x1p37;
  source.back().PositionEcefM[0] = kOutsidePositionDomain;
  CHECK(!BuildLightCells(source, kLimits),
        "position cells outside the declared domain are refused");
  CHECK(BuildLightCells({}, kLimits).has_value(), "an empty world has valid native light data");
}

void Frames() {
  constexpr double kBeforePositionBoundary = 127.9999999;
  const auto frame = MakeLightCellFrame({{kBeforePositionBoundary, 0, 0}}, LightCellLookup::Hash);
  CHECK(frame.has_value(), "finite camera has a native48B light frame");
  if (!frame) { return; }
  CHECK(frame->PositionLocal[0] == 128 && frame->LookupCell[0] == 3,
        "FP32 rounding to128 retains the true double-precision lookup cell");
  const int32_t carry = frame->LookupCell[0] - frame->PositionCell[0] * 4;
  const float lookupLocal = frame->PositionLocal[0] - static_cast<float>(carry) * 32;
  CHECK(lookupLocal == 32, "lookup residual preserves a rounded boundary without modulo loss");
  constexpr double kBeyondLookupDomain = 0x1p36;
  const auto full = MakeLightCellFrame({{kBeyondLookupDomain, 0, 0}}, LightCellLookup::Hash);
  CHECK(full && full->PositionCell[3] == static_cast<int32_t>(LightCellLookup::Full),
        "valid position origin with unrepresentable lookup uses complete enumeration");
  constexpr double kBeyondPositionDomain = 0x1p37;
  CHECK(!MakeLightCellFrame({{kBeyondPositionDomain, 0, 0}}, LightCellLookup::Hash),
        "unrepresentable camera position is refused rather than using a wrong full-sum origin");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!MakeLightCellFrame({{nan, 0, 0}}, LightCellLookup::Hash), "nonfinite camera is refused");
}

}

int main() {
  Ordinary();
  BudgetsAndRefusals();
  Frames();
  return Report();
}
