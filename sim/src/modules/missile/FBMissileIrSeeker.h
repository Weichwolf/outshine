/* FlightBox — FBMissileIrSeeker: a heat-homing round's own head, and structurally the SAME CLASS the
 * MiG-29's KOLS is (sensors/FBIrstSystem). The reason is the reason FBMissileSeeker derives from the
 * jet's radar: a missile is a unit of the world, so it may perceive the world only the way anything
 * here may — through a simulated sensor with stated limits.
 *
 * WHAT THAT BUYS, and it is not cosmetic: this round sees exactly what an infrared search head sees.
 * ANGLES and nothing else — no range, no closure, no identity, no interrogator — which is why the
 * guidance above it cannot fly true proportional navigation and flies the PURE form instead
 * (modules/missile/FBMissileGuidance). It is subject to the aspect law, the afterburner exception and
 * the cloud deck of the base class, and it is DECEIVED BY FLARES by the same code that decoys any
 * other infrared head, without a line of its own.
 *
 * THE PERCEPTION BOUNDARY DOES NOT GROW. This file names no registry: the scan lives in the base, and
 * tools/verify_layers.py's RESTRICTED list is unchanged. doc/weapons.md §10.2. */
#ifndef FBMISSILEIRSEEKER_H
#define FBMISSILEIRSEEKER_H

#include "FBIrstSystem.h"

namespace FlightBox::Modules {

class FBMissileIrSeeker : public Sensors::FBIrstSystem {
public:
  FBMissileIrSeeker();

  /* The round's own numbers, from its catalogue entry: the instantaneous cone the uncaged head sees,
   * the mechanical gimbal it can hold a target to once locked, and how far its detector reaches
   * against the rear-aspect reference airframe. The FOV/gimbal split is the same distinction the radar
   * seeker makes and for the same measured reason — without it a locked head drops its target the
   * moment the geometry moves by the width of its instantaneous field. */
  void Configure(double fovHalfDeg, double gimbalHalfDeg, double rangeM);

  /* Body-referenced SLAVE, exactly as the radar seeker's: the guidance points the head where it thinks
   * the target is until the head has its own track. Both zero = the launch rail's boresight. */
  void SlewTo(double azDeg, double elDeg);

  /* An uncaged head looks; a caged one reports NOTHING, so the guidance cannot home on a stale mark. */
  void SetUncaged(bool on);
  bool Uncaged() const { return Field_.Active; }

protected:
  /* The one hook, as in every sensor slot: the slaved cone while looking, the gimbal envelope once it
   * has something. NOT SingleTarget — a passive head has no power to concentrate (base class banner). */
  const Sensors::FBIrstFieldOfRegard &ActiveField() const override {
    return Locked() ? Track_ : Field_;
  }
  int ModeOrdinal() const override { return Field_.Active ? (Locked() ? 2 : 1) : 0; }
  /* No laser: a seeker head has no rangefinder, and this is the one place the tree could have been
   * tempted to give a missile a range it does not have. */
  double LaserRangeM() const override { return 0.0; }

private:
  Sensors::FBIrstFieldOfRegard Field_{};   /* the slaved acquisition cone */
  Sensors::FBIrstFieldOfRegard Track_{};   /* same reach, the gimbal's full envelope */
};

} // namespace FlightBox::Modules
#endif
