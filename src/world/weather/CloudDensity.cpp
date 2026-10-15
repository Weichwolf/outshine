#include "CloudLayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace outshine {
namespace {

constexpr std::array<uint32_t, 4> kCloudHashSalts{1597334677, 3812015801, 2798796415, 2246822519};
constexpr double kCloudHashMaximum = 16777215.0;
constexpr double kCloudProfileIntegralScale = 6.0;

double CloudHash(const std::array<int64_t, 3> &cell) noexcept {
  uint32_t bits = static_cast<uint32_t>(cell[0]) * kCloudHashSalts[0];
  bits ^= static_cast<uint32_t>(cell[1]) * kCloudHashSalts[1];
  bits ^= static_cast<uint32_t>(cell[2]) * kCloudHashSalts[2];
  bits ^= bits >> 16;
  bits *= kCloudHashSalts[3];
  bits ^= bits >> 13;
  return static_cast<double>(bits >> 8) / kCloudHashMaximum;
}

double CloudNoise(const Vec3 &position) noexcept {
  std::array<int64_t, 3> cell{};
  Vec3 blend;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double floor = std::floor(position[axis]);
    cell[axis] = static_cast<int64_t>(floor);
    const double fraction = position[axis] - floor;
    blend[axis] = fraction * fraction * (3.0 - 2.0 * fraction);
  }
  double result = 0.0;
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        const double weight = (x != 0 ? blend[0] : 1.0 - blend[0]) *
                              (y != 0 ? blend[1] : 1.0 - blend[1]) *
                              (z != 0 ? blend[2] : 1.0 - blend[2]);
        result += weight * CloudHash({cell[0] + x, cell[1] + y, cell[2] + z});
      }
    }
  }
  return result;
}

}

double CloudExtinction(const CloudLayer &layer, const Vec3 &positionM, double seconds) noexcept {
  if (layer.Cover <= 0.0) { return 0.0; }
  const double height = (CloudAltitude(positionM, layer.RadiusM) - layer.BaseM) / layer.ThicknessM;
  if (height <= 0.0 || height >= 1.0) { return 0.0; }
  const Vec3 advected = (positionM - layer.WindMs * seconds) * (1.0 / 1400.0);
  const double broad = CloudNoise(advected);
  const double detail = CloudNoise(advected * 3.1 + Vec3{{19.0, 7.0, 3.0}});
  const double structure = 0.8 * broad + 0.2 * detail;
  const double amount = std::clamp((structure - (1.0 - layer.Cover)) * 2.0, 0.0, 1.0);
  return layer.ExtinctionPerM * kCloudProfileIntegralScale * height * (1.0 - height) * amount;
}

double CloudOpticalDepth(const CloudLayer &layer,
                         const Vec3 &originM,
                         const Vec3 &direction,
                         const CloudTrace &trace) noexcept {
  if (layer.Cover <= 0.0) { return 0.0; }
  double optical = 0.0;
  for (const CloudSegment &segment : CloudShell(layer, originM, direction)) {
    const double endM = std::min(segment.EndM, trace.MaximumM);
    if (endM <= segment.BeginM) { continue; }
    const double stepM = (endM - segment.BeginM) / static_cast<double>(kCloudIntegrationSamples);
    for (size_t sample = 0; sample < kCloudIntegrationSamples; ++sample) {
      const double distance = segment.BeginM + (static_cast<double>(sample) + 0.5) * stepM;
      optical += CloudExtinction(layer, originM + direction * distance, trace.Seconds) * stepM;
    }
  }
  return optical;
}

}
