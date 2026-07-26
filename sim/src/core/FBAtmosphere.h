/* FlightBox — FBAtmosphere: the ISA standard atmosphere, header-only, for the two consumers that need
 * air density WITHOUT an FDM to ask.
 *
 * Everything that is flying already has JSBSim's own atmosphere behind it (aero/qbar-psf and friends);
 * this file exists for the two places that have to reason about air they are not currently in:
 *   - modules/f16/FBF16FireControl's launch-zone integration, which predicts a weapon's flight before
 *     that weapon exists, and
 *   - modules/missile/FBMissileGuidance's autopilot gain schedule, which needs the dynamic pressure
 *     acting on its own airframe from the pose it is handed (fb_fdm_state carries no qbar).
 * One definition rather than two private copies of the same four constants.
 *
 * Troposphere to 11 km with the standard 6.5 K/km lapse, then the isothermal layer. No wind, no weather,
 * no non-standard day: the consumers above are a stored fire-control table and a gain schedule, and
 * neither would be improved by a fidelity the rest of the engagement does not have. */
#ifndef FBATMOSPHERE_H
#define FBATMOSPHERE_H

#include <cmath>

namespace FlightBox {

/* Air density (kg/m^3) at a geometric altitude (m ASL). */
inline double FBIsaDensity(double altM) {
  if (altM < 11000.0) {
    double t = 288.15 - 0.0065 * altM;
    if (t < 1.0) t = 1.0;
    return 1.225 * std::pow(t / 288.15, 4.2561);
  }
  return 0.36391 * std::exp(-(altM - 11000.0) / 6341.62);
}

/* Dynamic pressure (Pa) from true airspeed and altitude. */
inline double FBDynamicPressure(double tasMs, double altM) {
  return 0.5 * FBIsaDensity(altM) * tasMs * tasMs;
}

} // namespace FlightBox
#endif
