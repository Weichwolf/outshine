/* FlightBox — FBMig29Director: the MiG-29's unguided air-to-ground delivery, and the ONE thing that
 * makes it not a CCIP/CCRP block with different constants.
 *
 * A release cue answers "when". This answers "how", and the pilot answers "when" only once — at the
 * trigger press, which on this aircraft is a CONSENT and not a release (`DCS-EA p.101`: *press the
 * missile launch trigger and hold it UNTIL the ABSP is released*). At that instant this class PLANS a
 * trajectory, commits to a release time on it, publishes the plan as a countdown plus a commanded
 * normal load factor (the "ring of the specified overload") — and never solves again. The aircraft
 * lets go by itself. What the pilot then does with the ring is the accuracy.
 *
 * WHY FROZEN AND NOT RE-SOLVED: a computer that re-solved every sweep would have no use for a
 * commanded g, and this one displays one. The inference, the counter-reading and what rides on it are
 * doc/modules/mig29/weapons.md §5.4.1 and gap 8.11 — it is the most load-bearing [ABL] in the file.
 *
 * IT KNOWS NO TERRAIN AND NO REGISTRY. The aim point is the active steerpoint off the nav block, the
 * impact plane is that steerpoint's own elevation, and the ballistics are core/FBBallistics — the same
 * integration the F-16's box uses, because a falling bomb does not care who dropped it. */
#ifndef FBMIG29DIRECTOR_H
#define FBMIG29DIRECTOR_H

#include "FBBallistics.h"
#include "FBDirector.h"
#include "FBState.h"
#include "FBStore.h"
#include "FBFdm.h"

namespace FlightBox::Modules {

class FBMig29Director {
public:
  /* [DOC, carried across] The ranging device's reach. `DCS-EA p.101` gives 11,500 ft only as the
   * AUTO-start condition, never as a maximum; the KOLS laser's own documented air-to-air maximum is
   * 6 km (doc/sensors.md §6, measured in stage 2b as `LASER_RANGE maxM=6000`) and it is the same
   * device. Registered as weapons.md gap 8.12, because it is what caps this jet's bombing altitude. */
  static constexpr double kLaserReachM = 6000.0;
  /* [DOC DCS-EA p.99, p.101, verbatim] "the minimum time should be more than 1 second, the maximum
   * time - less than 10 seconds. The highest shooting accuracy is achieved in the range of 1.5...4." */
  static constexpr double kConsentMinAgeS = 1.0;
  static constexpr double kConsentMaxAgeS = 10.0;
  /* [DOC DCS-EA p.101] "1.5...3 seconds before the remaining time is out, an audio signal is sent". */
  static constexpr double kAudioLeadS = 3.0;
  /* The plan's own march: 0.1 s steps out to 60 s. The step is a PLANNING resolution, not physics —
   * at 230 m/s it quantises the release point to 23 m, so the crossing is interpolated. */
  static constexpr double kPlanStepS = 0.1;
  static constexpr double kPlanMaxS = 60.0;
  /* The module's avionics sweep, which is how often this box can look at its own clock. It is the
   * QUANTISATION of the release moment and nothing else — see the rounding rule in Run(). */
  static constexpr double kSweepS = 0.1;

  /* WHICH ROUND is under the trigger, from the SMS via the module — the A/A-A/G switch of
   * `DCS-EA p.59` in the only form this tree can state it: a guided round is an air-to-air problem
   * and this box declines it. */
  void SetSelected(const FBStoreSpec *s) { Sel_ = s; }
  bool Engaged() const { return Sel_ != nullptr && !Sel_->Guided; }

  /* LOCKON — the laser fires at the aim point. false = there was nothing to range. */
  bool Range(const FBState &state, const Fdm::fb_fdm_state &own, double nowS);
  /* The trigger. `down` true = consent (plans, freezes, starts the countdown); false = the pilot let
   * go, which `DCS-EA p.104` explicitly allows and which abandons the countdown. false return = the
   * consent was refused; Refusal() says which documented boundary it hit. */
  bool Consent(bool down, const FBState &state, const Fdm::fb_fdm_state &own, double nowS);
  FBDirectorRefusal Refusal() const { return Refusal_; }

  /* THE AIRCRAFT'S OWN RELEASE MOMENT. The module drains it into FBStoresSystem::Release — which stays
   * the one and only way a store leaves this jet. */
  bool ReleaseDue() const { return State_ == FBDirectorState::Release; }
  void NotifyRelease(bool accepted, double nowS);

  /* What the released store carries out of the jet: the plan's own prediction, so the measured impact
   * can be put beside what the computer promised. Invalid until a plan exists. */
  const FBReleaseSolution &Solution() const { return Solution_; }

  void Run(FBState &state, const Fdm::fb_fdm_state &own, double nowS);

private:
  /* The reference trajectory the plan is made on: straight, wings level, holding the flight-path angle
   * and the true airspeed the aircraft had at consent. It is what the commanded g COMMANDS. */
  struct Plan {
    bool   Valid = false;
    double ReleaseAtS = 0.0;
    double CmdG = 1.0;
    double FpaDeg = 0.0, TasMs = 0.0, TrackDeg = 0.0, AltM = 0.0;
    double PredLatDeg = 0.0, PredLonDeg = 0.0, PredTofS = 0.0, ArmMarginS = 0.0;
  };

  bool AimPoint(const FBState &state, const Fdm::fb_fdm_state &own, double &latDeg, double &lonDeg,
                double &planeM) const;
  /* March the reference trajectory until a release now would land on the aim point. */
  Plan Solve(const FBState &state, const Fdm::fb_fdm_state &own, double nowS) const;

  const FBStoreSpec *Sel_ = nullptr;
  FBDirectorState State_ = FBDirectorState::Off;
  FBDirectorRefusal Refusal_ = FBDirectorRefusal::None;

  bool   HaveRange_ = false;
  double RangeM_ = 0.0, RangeAtS_ = 0.0;
  Plan   Plan_{};
  FBReleaseSolution Solution_{};

  /* What the pilot did with the ring, accumulated over the countdown — the delivery's OWN account of
   * why it missed, written once at the release rather than sampled per tick. */
  double TrackSumGErr_ = 0.0, TrackMaxGErr_ = 0.0, TrackSumS_ = 0.0, TrackMaxBankDeg_ = 0.0;

  /* THE OPEN-LOOP ERROR, ISOLATED — and it is the one number that measures the PROCEDURE rather than
   * the ballistic table. At the release instant the same integration is run twice: once as the plan
   * predicted it (frozen at consent) and once from the state the aircraft actually has. Both use the
   * identical stored table, so their difference contains NO table error at all: it is exactly what
   * committing to a plan and not re-solving cost, in metres along the track. A computer that re-solves
   * has this term equal to zero by construction, which is what makes the two procedures comparable.
   * doc/modules/mig29/weapons.md §5.4.4. */
  double OpenLoopAlongM_ = 0.0, OpenLoopCrossM_ = 0.0;
  double DevAltM_ = 0.0, DevSpeedMs_ = 0.0, DevFpaDeg_ = 0.0;
};

} // namespace FlightBox::Modules
#endif
