#include "GroundAtmosphere.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace outshine {
GroundLight
GroundAtmosphere::Evaluate(const Medium &medium, double cosSunZenith, Work *work) const {
  if (work != nullptr) { *work = {}; }
  const auto cosine = static_cast<float>(cosSunZenith);
  if (cosine != CachedCosine_ || !(medium == CachedMedium_)) {
    constexpr size_t kQueries = static_cast<size_t>(kMultiScatterGrid) *
                                static_cast<size_t>(kMultiScatterGrid) *
                                static_cast<size_t>(kSkyViewSteps);
    std::unordered_map<uint64_t, Vec3f> scattered;
    scattered.reserve(kQueries);
    const float radiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm;
    const auto toSun = [&](MediumLook look) {
      return MediumTransmittance(medium, look, kTransmittanceSteps);
    };
    const auto secondOrder = [&](MediumLook look) {
      const MediumUv unit = {.U = look.CosZenith * 0.5f + 0.5f,
                             .V = (look.RadiusKm - medium.BottomRadiusKm) /
                                  (medium.TopRadiusKm - medium.BottomRadiusKm)};
      const uint64_t key = (static_cast<uint64_t>(std::bit_cast<uint32_t>(unit.U)) << 32u) |
                           std::bit_cast<uint32_t>(unit.V);
      if (work != nullptr) { ++work->SkyQueries; }
      if (const auto found = scattered.find(key); found != scattered.end()) {
        return found->second;
      }
      const MultiScatterSample sample = MediumMultiScatterTexel(medium, unit, toSun);
      if (work != nullptr) { ++work->ScatterEvaluations; }
      Vec3f result;
      for (int channel = 0; channel < 3; ++channel) {
        result[channel] = sample.Luminance[channel] / (1.0f - sample.Transfer[channel]);
      }
      scattered.emplace(key, result);
      return result;
    };
    const MediumLook ground = {.RadiusKm = radiusKm, .CosZenith = cosine};
    CachedLight_.SkyIrradiance = MediumSkyIrradiance(medium, ground, toSun, secondOrder);
    CachedLight_.SunTransmittance = toSun(ground);
    CachedCosine_ = cosine;
    CachedMedium_ = medium;
    ++Integrations_;
  }
  return CachedLight_;
}
}
