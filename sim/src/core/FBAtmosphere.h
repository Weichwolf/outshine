/* The ISA standard atmosphere, header-only, for the two consumers that must reason about air they are
 * not currently flying in (a launch-zone integration and a missile gain schedule) — everything that
 * flies has JSBSim's own atmosphere behind it. Troposphere to 11 km at the standard 6.5 K/km lapse,
 * then the isothermal layer. No wind, no weather, no non-standard day.
 * doc/flightbox/core.md, Abschnitt 10.2. */
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
