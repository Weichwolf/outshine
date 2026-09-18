#include <cmath>

#include "Atmosphere.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  GroundAtmosphere atmosphere;
  const GroundLight clear = atmosphere.Evaluate(Hazed(kEarthAir, 0.0), 0.5);
  CHECK(atmosphere.Integrations() == 1, "the first state integrates once");
  const GroundLight repeated = atmosphere.Evaluate(Hazed(kEarthAir, 0.0), 0.5);
  CHECK(atmosphere.Integrations() == 1, "an identical state reuses its integration");
  CHECK(repeated.SunTransmittance == clear.SunTransmittance &&
            repeated.SkyIrradiance == clear.SkyIrradiance,
        "the cached state returns the exact previous light");

  const GroundLight hazed = atmosphere.Evaluate(Hazed(kEarthAir, 2.0), 0.5);
  CHECK(atmosphere.Integrations() == 2, "changed air invalidates the cached integration");
  CHECK(hazed.SunTransmittance != clear.SunTransmittance,
        "aerosol density changes direct sunlight at the ground");
  for (int channel = 0; channel < 3; ++channel) {
    CHECK(std::isfinite(hazed.SunTransmittance[channel]) &&
              std::isfinite(hazed.SkyIrradiance[channel]),
          "evaluated ground light stays finite");
  }
  return Report();
}
