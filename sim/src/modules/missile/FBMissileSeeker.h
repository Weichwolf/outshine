/* FlightBox — FBMissileSeeker: the round's own active radar, structurally the SAME CLASS the jet's FCR
 * is. Not a shortcut but the point: a missile is a unit of the world, so it perceives the world the only
 * way anything here may — through a simulated sensor. This class is the one thing in modules/missile/
 * that holds the unit registry.
 *
 * What makes it a SEEKER is all expressed in the one ActiveVolume hook: a narrow field of view, SLAVED
 * to where the guidance points it rather than searching, a stare instead of a sweep, off until the
 * guidance activates it, and auto-locking because there is no operator to designate. It CANNOT tell
 * friend from foe — the contact is anonymous geometry and there is no interrogator.
 * doc/weapons-and-damage.md §10.2. */
#ifndef FBMISSILESEEKER_H
#define FBMISSILESEEKER_H

#include "FBRadarSystem.h"

namespace FlightBox::Modules {

class FBMissileSeeker : public Sensors::FBRadarSystem {
public:
  /* [SET] — no public figure exists. Its measurable CONSEQUENCE is the point: a midcourse that hands
   * over with the target more than this far off the nose does not acquire. */
  static constexpr double kFovHalfDeg = 10.0;
  /* [SET] The dish's mechanical GIMBAL limit, a different and much larger angle than the instantaneous
   * field of view: once locked, the seeker POINTS the dish and only runs out at the stops. Without the
   * distinction a locked seeker drops its own target the moment the geometry moves 10 deg (measured). */
  static constexpr double kGimbalHalfDeg = 45.0;
  /* [SET] A stare, not a sweep — fresh measurements for the terminal phase instead of extrapolation. */
  static constexpr double kFrameS = 0.05;

  FBMissileSeeker();

  void SetRangeM(double m);

  /* Body-referenced SLAVE: the guidance points the dish at the target it is flying toward, so it is
   * already there when the target enters detection range. Both zero = BORE. */
  void SlewTo(double azDeg, double elDeg);

  /* A seeker that is off reports NOTHING, so the guidance cannot home on a pre-activation track. */
  void SetActive(bool on) { Vol_.Active = on; Track_.Active = on; }
  bool Active() const { return Vol_.Active; }

protected:
  /* The one hook: the slaved cone while looking, the gimbal envelope once it has something. */
  const Sensors::FBRadarScanVolume &ActiveVolume() const override { return Locked() ? Track_ : Vol_; }
  int ModeOrdinal() const override { return Vol_.Active ? 1 : 0; }
  /* What differs about this set's EMISSION: behind the antenna is a warhead, so a warning receiver
   * classifies it as the launch case however it happens to be scanning. */
  FBEmitterKind EmitterKind() const override { return FBEmitterKind::MissileSeeker; }

private:
  Sensors::FBRadarScanVolume Vol_{};
  Sensors::FBRadarScanVolume Track_{};   /* the locked pattern: same range/frame, the gimbal's full envelope */
};

} // namespace FlightBox::Modules
#endif
