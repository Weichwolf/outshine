/* FlightBox — FBMissileSeeker: the AMRAAM's own active radar, and structurally the SAME CLASS the jet's
 * FCR is (systems/FBRadarSystem). That is not a shortcut, it is the point: the missile is a unit of the
 * world like any other, so it perceives the world the only way anything in this simulator may — through
 * a simulated sensor that scans a volume, needs consecutive looks to call something a track, and
 * publishes anonymous geometry into its own FBState (CLAUDE.md "Kein Cheaten"). Nothing in
 * modules/missile/ ever reads the unit registry directly; this class is the one thing that holds it,
 * exactly as FBRadarSystem's banner requires.
 *
 * WHAT MAKES IT A SEEKER RATHER THAN A FIGHTER RADAR, all of it expressed in the ONE hook
 * FBRadarSystem gives a module (ActiveVolume):
 *   - A NARROW FIELD OF VIEW. A missile seeker looks through a small gimballed dish in a 7 in nose:
 *     kFovHalfDeg either side of wherever it is pointed, against the FCR's 60 deg sweep.
 *   - SLAVED, NOT SEARCHING. Until it acquires, the volume's CENTRE is the direction the guidance tells
 *     it to look — the body-referenced angles of the target the launcher's uplink last reported (the
 *     "SLAVE" line-of-sight mode of doc/f16/weapons.md §2.5). A seeker that scanned on its own would be
 *     a different weapon; BORE launch (the other documented mode) is the same class with SlewTo() left
 *     at zero, which is why the mode is a pair of numbers and not a subclass.
 *   - A SHORT RANGE AND A FAST FRAME. kFrameS is a stare, not a sweep: the seeker looks at its narrow
 *     volume many times a second, so the terminal phase has fresh geometry to home on. Its detection
 *     range (the round's own catalogue figure, core/FBStore.h) is a fraction of the launcher's — which
 *     is exactly why the shot needs a midcourse phase at all.
 *   - IT IS OFF UNTIL IT IS TURNED ON. A real AMRAAM does not radiate for most of its flight; the
 *     guidance switches this on at the activation range, and that switch is what "the missile goes
 *     active" MEANS in the DLZ's countdown (weapons.md §2.5's Radar Activation Range).
 *   - IT LOCKS WHAT IT FINDS. AutoAcquire + SingleTarget: the first firm track becomes an STT and the
 *     seeker then stares at it. There is no operator to designate one.
 *
 * IT CANNOT TELL FRIEND FROM FOE, and that is modelled rather than papered over: the contact it
 * publishes is anonymous geometry (core/FBRadarContact.h) and the IFF interrogator is switched OFF (a
 * missile carries none). Whatever it locks in its narrow, slaved cone is what it flies at. */
#ifndef FBMISSILESEEKER_H
#define FBMISSILESEEKER_H

#include "FBRadarSystem.h"

namespace FlightBox {

class FBMissileSeeker : public FBRadarSystem {
public:
  /* +-10 deg of gimballed look [SET]: no public figure exists for the AMRAAM's seeker field of view
   * (doc/f16/weapons.md §4.7 lists exactly this class of number as a gap). Ten degrees is the order for
   * a 7 in dish and it has a consequence the mission can measure — a midcourse that hands over with the
   * target more than 10 deg off the nose does not acquire, which is what makes the midcourse quality
   * matter instead of being decoration. */
  static constexpr double kFovHalfDeg = 10.0;
  /* The dish's mechanical GIMBAL limit [SET], which is a different and much larger angle than the
   * instantaneous field of view above: once the seeker has a track it points the dish AT it, and only
   * runs out at the gimbal stops. Without this distinction a locked seeker would drop its own target
   * the moment the geometry moved 10 deg — which the first flown lost-lock run showed literally (drop
   * and re-acquire at 25 deg off the nose, 3 s before impact). The same shape as the FCR's own
   * search-box/STT-envelope split (modules/f16/FBF16Fcr). */
  static constexpr double kGimbalHalfDeg = 45.0;
  /* 0.05 s per look [SET]: a stare at one narrow volume, i.e. 20 looks a second — fast enough that the
   * terminal guidance is flying on measurements rather than on extrapolation. */
  static constexpr double kFrameS = 0.05;

  FBMissileSeeker();

  /* Range gate + how far the round may actually see, from its own catalogue entry. */
  void SetRangeM(double m);

  /* Where to look, body-referenced (the SLAVE mode above): the guidance points the seeker at the target
   * it is currently flying toward, so the dish is already on the target when the target enters
   * detection range. Both zero = BORE. */
  void SlewTo(double azDeg, double elDeg);

  /* The activation switch. A missile seeker that is off is not merely quiet — it reports nothing, so
   * the guidance cannot accidentally home on a track from before it went active. */
  void SetActive(bool on) { Vol_.Active = on; Track_.Active = on; }
  bool Active() const { return Vol_.Active; }

protected:
  /* The one hook (systems/FBRadarSystem): the slaved search cone while it is looking, the gimbal
   * envelope once it has something. */
  const FBRadarScanVolume &ActiveVolume() const override { return Locked() ? Track_ : Vol_; }
  int ModeOrdinal() const override { return Vol_.Active ? 1 : 0; }
  /* The one thing about this set's EMISSION that differs from a fighter's, and the one that matters to
   * whoever hears it (core/FBEmitter.h): what is behind the antenna is a warhead. A warning receiver
   * classifies this signal as the launch case however it happens to be scanning. */
  FBEmitterKind EmitterKind() const override { return FBEmitterKind::MissileSeeker; }

private:
  FBRadarScanVolume Vol_{};
  FBRadarScanVolume Track_{};   /* the locked pattern: same range/frame, the gimbal's full envelope */
};

} // namespace FlightBox
#endif
