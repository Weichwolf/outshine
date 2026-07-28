/* A unit's declarative initial condition (doc/missions/syntax.md's `spawn` line). Pure data: the Boot
 * turns it into exactly ONE JSBSim IC application, and there is no separate ground/air code path
 * beyond the one bool below. */
#ifndef FBSPAWN_H
#define FBSPAWN_H

namespace FlightBox {

struct FBSpawn {
  double LatDeg = 0.0, LonDeg = 0.0;
  bool   Ground = true;    /* true: 'ground' keyword — sit on gear at the resolved terrain elevation.
                            * false: AltM is a literal ASL altitude (an airborne spawn). */
  double AltM = 0.0;       /* literal target altitude, m ASL — meaningful only when !Ground */
  double HeadingDeg = 0.0;
  double SpeedKt = 0.0;
};

} // namespace FlightBox
#endif
