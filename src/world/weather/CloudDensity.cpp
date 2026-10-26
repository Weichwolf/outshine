#include "CloudLayer.h"
#include "CloudIntegration.h"

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
constexpr double kCloudFieldScaleM = 1400.0;
constexpr double kCloudDetailFrequency = 3.1;
constexpr Vec3 kCloudDetailOffset{{19.0, 7.0, 3.0}};
constexpr double kCloudBroadWeight = 0.8;
constexpr double kCloudDetailWeight = 0.2;
constexpr double kCloudBodyJitter = 0.6;
constexpr double kCloudBodyRadius = 0.7;
constexpr double kCloudBodyRadiusBase = 0.85;
constexpr double kCloudBodyRadiusRange = 0.3;
constexpr double kCloudBodyTopBase = 0.55;
constexpr double kCloudBodyTopRange = 0.4;
constexpr double kCloudBodyErosion = 0.18;
constexpr double kCloudBodyBaseFade = 0.08;
constexpr double kCloudBodyDensity = 1.5;
constexpr int64_t kCloudBodySeed = 812;
constexpr double kCloudBodyFeatureMaximum = 255.0;
constexpr double kCloudClosedCoverBegin = 0.75;

uint32_t CloudHashBits(const std::array<int64_t, 3> &cell) noexcept {
  uint32_t bits = static_cast<uint32_t>(cell[0]) * kCloudHashSalts[0];
  bits ^= static_cast<uint32_t>(cell[1]) * kCloudHashSalts[1];
  bits ^= static_cast<uint32_t>(cell[2]) * kCloudHashSalts[2];
  bits ^= bits >> 16;
  bits *= kCloudHashSalts[3];
  bits ^= bits >> 13;
  return bits;
}

double CloudHash(const std::array<int64_t, 3> &cell) noexcept {
  return static_cast<double>(CloudHashBits(cell) >> 8) / kCloudHashMaximum;
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

double CloudUnitProfile(double value) noexcept {
  const double bounded = std::clamp(value, 0.0, 1.0);
  return bounded * bounded * (3.0 - 2.0 * bounded);
}

struct CloudBodyPoint {
  const Vec3 &Advected;
  double Height;
};

double CloudBodyShape(const CloudBodyPoint &point, double cover) noexcept {
  const auto &advected = point.Advected;
  const auto cellX = static_cast<int64_t>(std::floor(advected[0]));
  const auto cellZ = static_cast<int64_t>(std::floor(advected[2]));
  const double radiusScale = kCloudBodyRadius * std::sqrt(cover);
  double body = 0.0;
  for (int x = -1; x <= 1; ++x) {
    for (int z = -1; z <= 1; ++z) {
      const uint32_t bits = CloudHashBits({cellX + x, kCloudBodySeed, cellZ + z});
      const auto feature = [bits](uint32_t shift) noexcept {
        constexpr uint32_t mask = 255;
        return static_cast<double>((bits >> shift) & mask) / kCloudBodyFeatureMaximum;
      };
      constexpr uint32_t kFeatureStrideBits = 8;
      const double centerX = 0.5 + (feature(0) - 0.5) * kCloudBodyJitter;
      const double centerZ = 0.5 + (feature(kFeatureStrideBits) - 0.5) * kCloudBodyJitter;
      const double radius = radiusScale * (kCloudBodyRadiusBase +
                                           kCloudBodyRadiusRange * feature(2 * kFeatureStrideBits));
      const double top = kCloudBodyTopBase + kCloudBodyTopRange * feature(3 * kFeatureStrideBits);
      const double dx = (advected[0] - static_cast<double>(cellX + x) - centerX) / radius;
      const double dz = (advected[2] - static_cast<double>(cellZ + z) - centerZ) / radius;
      const double vertical = point.Height / top;
      body = std::max(body, 1.0 - dx * dx - dz * dz - vertical * vertical);
    }
  }
  return body;
}

double CloudBodyProfile(const CloudBodyPoint &point, double eroded) noexcept {
  return kCloudBodyDensity * CloudUnitProfile(point.Height / kCloudBodyBaseFade) *
         CloudUnitProfile(eroded);
}

double CloudBodyDensity(const CloudBodyPoint &point, double cover) noexcept {
  const double body = CloudBodyShape(point, cover);
  if (body <= 0.0) { return 0.0; }
  const double detail = CloudNoise(point.Advected * kCloudDetailFrequency + kCloudDetailOffset);
  return CloudBodyProfile(point, body - kCloudBodyErosion * (1.0 - detail));
}

}

double CloudExtinction(const CloudLayer &layer, const Vec3 &positionM, double seconds) noexcept {
  if (layer.Cover <= 0.0) { return 0.0; }
  const double height = (CloudAltitude(positionM, layer.RadiusM) - layer.BaseM) / layer.ThicknessM;
  if (height <= 0.0 || height >= 1.0) { return 0.0; }
  const Vec3 advected = (positionM - layer.WindMs * seconds) * (1.0 / kCloudFieldScaleM);
  if (layer.Profile == CloudProfile::CumulusV1 && layer.Cover < kCloudClosedCoverBegin) {
    return layer.ExtinctionPerM *
           CloudBodyDensity({.Advected = advected, .Height = height}, layer.Cover);
  }
  const double broad = CloudNoise(advected);
  const double detail = CloudNoise(advected * kCloudDetailFrequency + kCloudDetailOffset);
  const double structure = kCloudBroadWeight * broad + kCloudDetailWeight * detail;
  const double amount = std::clamp((structure - (1.0 - layer.Cover)) * 2.0, 0.0, 1.0);
  const double stratus =
      layer.ExtinctionPerM * kCloudProfileIntegralScale * height * (1.0 - height) * amount;
  if (layer.Profile == CloudProfile::CumulusV1) {
    const CloudBodyPoint point{.Advected = advected, .Height = height};
    const double body = CloudBodyShape(point, layer.Cover);
    const double cumulus = CloudBodyProfile(point, body - kCloudBodyErosion * (1.0 - detail));
    const double closed =
        CloudUnitProfile((layer.Cover - kCloudClosedCoverBegin) / (1.0 - kCloudClosedCoverBegin));
    return std::max(layer.ExtinctionPerM * cumulus, closed * stratus);
  }
  return stratus;
}

double CloudOpticalDepth(const CloudLayer &layer,
                         const Vec3 &originM,
                         const Vec3 &direction,
                         const CloudTrace &trace) noexcept {
  double optical = 0.0;
  VisitCloudCells(layer, originM, direction, trace, [&](const CloudCell &cell) noexcept {
    optical += cell.OpticalDepth;
  });
  return optical;
}

}
