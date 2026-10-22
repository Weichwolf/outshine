#include "GroundAtmosphere.h"
#include "Check.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {
using namespace outshine;

GroundLight Unmemoized(const Medium &medium, double cosine) {
  const auto toSun = [&](MediumLook look) {
    return MediumTransmittance(medium, look, kTransmittanceSteps);
  };
  const auto scatter = [&](MediumLook look) {
    const MediumUv unit{.U = look.CosZenith * 0.5f + 0.5f,
                        .V = (look.RadiusKm - medium.BottomRadiusKm) /
                             (medium.TopRadiusKm - medium.BottomRadiusKm)};
    const auto sample = MediumMultiScatterTexel(medium, unit, toSun);
    Vec3f result;
    for (int channel = 0; channel < 3; ++channel) {
      result[channel] = sample.Luminance[channel] / (1.0f - sample.Transfer[channel]);
    }
    return result;
  };
  const MediumLook ground{.RadiusKm = medium.BottomRadiusKm + kMediumGroundLiftKm,
                          .CosZenith = static_cast<float>(cosine)};
  return {.SunTransmittance = toSun(ground),
          .SkyIrradiance = MediumSkyIrradiance(medium, ground, toSun, scatter)};
}

bool SameBits(const GroundLight &left, const GroundLight &right) {
  for (size_t channel = 0; channel < 3; ++channel) {
    if (std::bit_cast<uint32_t>(left.SunTransmittance[channel]) !=
            std::bit_cast<uint32_t>(right.SunTransmittance[channel]) ||
        std::bit_cast<uint32_t>(left.SkyIrradiance[channel]) !=
            std::bit_cast<uint32_t>(right.SkyIrradiance[channel])) {
      return false;
    }
  }
  return true;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<std::array<double, 2>, 4> states{
      {{0.0, 0.741652548}, {2.0, -0.1}, {1.0, 0.0}, {0.0, 1.0}}};
  constexpr size_t queries = static_cast<size_t>(kSkyViewSteps) *
                             static_cast<size_t>(kMultiScatterGrid) *
                             static_cast<size_t>(kMultiScatterGrid);
  GroundAtmosphere atmosphere;
  size_t integrations = 0;
  for (const auto &state : states) {
    const auto medium = Hazed(kEarthAir, state[0]);
    const auto reference = Unmemoized(medium, state[1]);
    GroundAtmosphere::Work work;
    const auto result = atmosphere.Evaluate(medium, state[1], &work);
    std::printf("SKY_WORK haze=%.9g cosine=%.9g queries=%zu evaluations=%zu\n",
                state[0],
                state[1],
                work.SkyQueries,
                work.ScatterEvaluations);
    CHECK(SameBits(result, reference),
          "exact memoization preserves every native sun and sky float word");
    CHECK(work.SkyQueries == queries && work.ScatterEvaluations > 0 &&
              work.ScatterEvaluations < work.SkyQueries,
          "all original samples remain but repeated texel inputs execute once");
    CHECK(atmosphere.Integrations() == ++integrations,
          "changed physical state performs exactly one complete integration");
    const auto repeated = atmosphere.Evaluate(medium, state[1], &work);
    CHECK(SameBits(repeated, result) && work.SkyQueries == 0 && work.ScatterEvaluations == 0 &&
              atmosphere.Integrations() == integrations,
          "the existing whole-state cache bypasses temporary maps and all integrations");
    GroundAtmosphere copied = atmosphere;
    const auto restored = copied.Evaluate(medium, state[1], &work);
    CHECK(SameBits(restored, result) && work.SkyQueries == 0 && work.ScatterEvaluations == 0 &&
              copied.Integrations() == integrations,
          "scene replacement can copy the complete light cache without new scattering work");
  }
  return Report();
}
