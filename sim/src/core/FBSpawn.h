/* FlightBox — FBSpawn: a unit's declarative initial condition (doc/mission-format.md's `spawn` line) —
 * position, heading, speed, and whether altitude resolves from the ground (gear-down sit) or is a
 * literal ASL value. Pure data, no modes/phases (core/ architecture banner): the Runner/Boot turns this
 * into exactly ONE JSBSim IC application (FBFdmBoot::Spawn already applies position+attitude+velocity
 * together — see FBMissionBoot.h's FBMissionApplySpawn) plus the module's own starting FBPilot phase.
 * There is no separate ground/air code path here beyond this one bool. */
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
