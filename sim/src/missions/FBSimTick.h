/* THE SIMULATION'S DECISION RATE — a property of the simulation, not a choice a client makes. Every
 * throttled system integrates and clocks against the dt its client hands down (FBPilot::TimeS_ is the
 * sharpest case: it advances by dt per SLOT tick), so a client stepping a different dt flies a
 * different jet out of the same file. Measured: cbu87-footprint releases at aimMissM 22.5 m under
 * 0.1 s and 117.9 m under 1/60 s. doc/clients/clients.md §5.5. */
#ifndef FBSIMTICK_H
#define FBSIMTICK_H

namespace FlightBox::Missions {

inline constexpr double kSimTickS = 0.1;

} // namespace FlightBox::Missions
#endif
