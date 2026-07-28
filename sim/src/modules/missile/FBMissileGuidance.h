/* FlightBox — FBMissileGuidance: the missile's PILOT, in exactly the slot systems/FBPilot defines. A
 * jet's pilot picks waypoints; this one runs PROPORTIONAL NAVIGATION. Everything below it is unchanged
 * — the commands travel through Autopilot(Manual)/FlightControl into the SIMULATED FINS, so the
 * anti-cheat rule that governs every pilot governs this one.
 *
 * Under the law sit two lateral-acceleration loops (SKID-TO-TURN: a cruciform round pitches and yaws
 * without banking), each closed on what a missile actually measures — accelerometer error plus a GYRO
 * rate term that is not optional, because the airframe's own pitch damping is nearly nil by design.
 *
 * THREE PHASES, and the handover is an EVENT rather than a timer: the seeker is switched on at the
 * activation range, and the phase changes when it actually ACQUIRES — which it can only do if the
 * midcourse pointed it close enough. A bad midcourse produces a miss, not a magic terminal phase, and
 * losing the uplink drops back to Inertial on the last estimate rather than to a failure path.
 *
 * Full derivation of PN, of N, of the gain schedule and of the phase table:
 * doc/weapons-and-damage.md §10.2. */
#ifndef FBMISSILEGUIDANCE_H
#define FBMISSILEGUIDANCE_H

#include "FBPilot.h"
#include "FBStore.h"
#include "FBWeaponUplink.h"

namespace FlightBox::Modules {

class FBMissileSeeker;
class FBMissileIrSeeker;
class FBMissileArSeeker;

class FBMissileGuidance : public Pilot::FBPilot {
public:
  /* Telemetry ordinal — append, never reorder. */
  enum class Phase { Inertial = 0, Midcourse, Terminal };
  static const char *PhaseName(Phase p);

  static constexpr double kNavConstant = 4.0;
  /* Command ceiling [SET]: above what the round can reach except low and fast, so it bounds the ask
   * without ever being the binding limit in a normal engagement. */
  static constexpr double kMaxCommandG = 25.0;
  /* ---- AUTOPILOT GAINS, SCHEDULED ON DYNAMIC PRESSURE. Fin authority is proportional to q, so a fixed
   * gain would be sluggish exactly where the round is slow (right after launch, where the first turn
   * has to be made) and twitchy where it is fast. kFinPerG is the [MESS] inverse of the airframe's
   * measured g-per-fin at kQRefPa; the INTEGRAL is not optional, because a proportional-only loop
   * leaves a steady-state error that the 1 g gravity bias turns into a permanent SINK (measured before
   * the term existed: ~900 m low at the merge). ---- */
  static constexpr double kQRefPa = 119000.0;
  static constexpr double kFinPerG = 1.0 / 13.8;
  /* kLoopI [MESS]: the largest integral gain at which the TERMINAL channel does not oscillate. The
   * criterion is the round's own alpha excursion tick to tick in that phase: 1.75 -> 0.698 deg,
   * 1.5 -> 0.139 deg on bvr-duel-decided (5x, a stability edge, not a trend). The old 2.0 sat on the
   * wrong side of it and only showed once a beaming target drove the demand past 10 g. */
  static constexpr double kLoopP = 1.2, kLoopI = 1.5;
  static constexpr double kRateGain = 0.35;
  static constexpr double kGainScaleMin = 0.15, kGainScaleMax = 20.0;
  static constexpr double kIntegralClamp = 1.0;
  static constexpr double kRollGain = 0.05, kRollRateGain = 0.02;
  /* [SET] The launcher refreshes at its own radar frame time, so a second without a message means the
   * support has STOPPED, not that one was missed. */
  static constexpr double kUplinkTimeoutS = 1.5;

  /* [SET] How long a line-of-sight rate measured from ONE pair of looks stays valid. An angle-only
   * head reports on its own frame grid (0.05 s) while the guidance runs at 100 Hz, so the rate is HELD
   * between looks rather than differentiated to zero; past this it is stale and the round flies
   * straight instead of on an old rate. Two head frames. */
  static constexpr double kLosRateHoldS = 0.1;

  /* [SET] The laser kit's DEAD BAND, and it is the whole of its actuator model beside the two stops:
   * inside this much alignment the canards trail, outside it they are on the stop. 1.5 deg is the
   * smallest band at which the terminal path does not chatter at 100 Hz. There is no commanded
   * acceleration next to it and there must not be — what the fins produce is the DECK's answer.
   * doc/air-to-ground.md §Knowledge 4. */
  static constexpr double kLaserDeadBandDeg = 1.5;
  /* [SET] How far ahead the relay's switching signal is projected with the round's own rate gyros —
   * one actuator lag (the deck's c1 = 20 1/s = 50 ms) plus the airframe's own response, rounded up. */
  static constexpr double kLaserRateLeadS = 0.30;

  /* Wiring, once, by the module; all borrowed. Which seeker is real is the catalogue entry's
   * FBSeekerKind, and it is read here and in FBMissileModule::Run and nowhere else. */
  void Bind(const FBStoreSpec &spec, FBMissileSeeker &seeker, FBMissileIrSeeker &irSeeker,
            FBMissileArSeeker &arSeeker);
  /* The launch programming: where the shooter's fire control last saw the target, and whose uplink this
   * round listens to. Applied once, at spawn. */
  void Program(const FBWeaponTargetState &target, int launcherId, double launchS);
  /* THE WHOLE launch programming of an anti-radiation round: two angles off the shooter's nose and the
   * class it was told to follow. There is no position, because the shooter's receiver measured none. */
  void ProgramCue(bool valid, double azDeg, double elDeg, FBArTargetClass cls);
  int  LauncherId() const { return LauncherId_; }

  /* The same absolute sim time the seeker's grid and the uplink's message ages run on, so nothing in
   * the round has a private notion of now. */
  void SetTime(double simTimeS) { NowS_ = simTimeS; }

  /* The override point: reads its own seeker's and uplink's bus blocks plus the airframe state, returns
   * fin/throttle as a Manual guidance request. `plan`/`runway` unused — a missile has no flight plan. */
  Pilot::FBPilotCommands Run(const FBState &state, FBCommandBus &avionics, const Systems::FBAirframeControls &airframe,
                      const Fdm::fb_fdm_state &st, const FBFlightPlan &plan, const FBRunway *runway,
                      double dt) override;

  /* A missile's trace is not a jet's, and the bus is per-unit, so this REPLACES the pilot's channels
   * rather than appending: no aircraft's telemetry.csv changes by one column. */
  const char *TelemetryName() const override { return "msl"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  Phase GuidancePhase() const { return Phase_; }

private:
  /* Strict priority: own seeker > uplink > last known; each branch sets Phase_. */
  void UpdateTarget(const FBState &state, const Fdm::fb_fdm_state &st);
  void EnterPhase(Phase p, const char *why);

  /* The ANGLE-ONLY law: PURE proportional navigation, because an infrared head measures neither range
   * nor closure and `N*Vc*Omega` therefore has no Vc to put in. The line-of-sight rate is differentiated
   * from consecutive LOOKS of the head (which is what a real seeker's gyro puts out directly) and the
   * command is scaled with the round's OWN speed instead:  a = N * V_own * (Omega x u).
   * Everything below it — the two acceleration loops, the gravity bias, the fins — is unchanged. */
  bool AngleOnlyCommand(const FBState &state, const Fdm::fb_fdm_state &st, double &aE, double &aN,
                        double &aU);
  /* THE SAME angle-only law on a DIFFERENT detector, and the difference is what the round can do when
   * the detector goes quiet. The warning receiver reports BODY-referenced angles (it has no antenna to
   * point), so the line of sight is rotated into the local horizon with the round's OWN attitude — an
   * instrument it carries — before the rate is taken. When the measurement stops, the rate is HELD for
   * the row's SeekerMemoryS and is then ZERO: PN commands nothing lateral, the 1 g bias survives
   * because an accelerometer needs nobody, and the round coasts STRAIGHT. doc/air-to-ground.md §2.2. */
  bool AntiRadiationCommand(const FBState &state, const Fdm::fb_fdm_state &st, double &aE, double &aN,
                            double &aU);
  /* THE PAVEWAY LAW, and it is deliberately NOT proportional navigation: align the velocity vector with
   * the instantaneous line of sight to the spot, with a BANG-BANG actuator and a dead band; when
   * aligned the canards trail and the weapon falls ballistically, gravity-biased. Modelling it as PN
   * would produce a materially better weapon than the real one. doc/air-to-ground.md §Knowledge 4. */
  bool LaserCommand(const FBState &state, const Fdm::fb_fdm_state &st, Pilot::FBPilotCommands &c);
  /* Everything BELOW whichever law produced the command: body resolution, the two acceleration loops,
   * the roll holder, the fins. Shared, so the two laws cannot drift apart in their autopilot. */
  void FlyCommand(Pilot::FBPilotCommands &c, const Fdm::fb_fdm_state &st, double aE, double aN,
                  double aU, double dt);

  const FBStoreSpec *Spec_ = nullptr;   /* borrowed catalogue entry */
  FBMissileSeeker *Seeker_ = nullptr;   /* borrowed, owned by the module */
  FBMissileIrSeeker *IrSeeker_ = nullptr;
  FBMissileArSeeker *ArSeeker_ = nullptr;
  FBSeekerKind Kind_ = FBSeekerKind::None;
  /* SARH: the shooter's illumination is this round's ONLY source of energy on the target, so losing it
   * kills the head for good — there is no transmitter of its own to switch back on. */
  bool SarhDead_ = false;
  /* The angle-only tracker's state: the last measured line of sight, when it was measured, and the
   * rate held from the last pair. */
  bool   IrTerminal_ = false;
  double IrLosE_ = 0.0, IrLosN_ = 0.0, IrLosU_ = 0.0;
  double IrLosStampS_ = -1e9;
  double IrOmE_ = 0.0, IrOmN_ = 0.0, IrOmU_ = 0.0;
  double IrOmStampS_ = -1e9;

  /* ---- THE ANTI-RADIATION ROUND'S OWN STATE. The latch is a SYMBOL number, which is the only handle
   * the receiver publishes: it never gives out a unit id, so "never re-target" can only mean "follow
   * the symbol you took", and a symbol that disappears is a transmitter that stopped. ---- */
  int    ArSymbol_ = 0;                 /* 0 = nothing latched yet */
  bool   ArLost_ = false;               /* logged once per loss, cleared on re-acquisition */
  double ArSigNorm_ = 0.0;              /* the received power — the ONE proximity hint this round has */
  double ArLastLookS_ = -1e9;           /* the memory clock: when the transmitter was last MEASURED */
  int    ArLosSymbol_ = 0;              /* whose line the one below is — a rate needs two of the same */
  double ArLosE_ = 0.0, ArLosN_ = 0.0, ArLosU_ = 0.0;   /* the last measured line, in the local horizon */
  double ArOmE_ = 0.0, ArOmN_ = 0.0, ArOmU_ = 0.0;      /* the HELD rate — zeroed when the memory ends */

  /* ---- THE LASER-GUIDED BOMB'S. `SalDead_` does not exist on purpose: a passive energy detector
   * reacquires a spot that comes back, which is the one place it differs from the radar cousin. ---- */
  bool   SalTerminal_ = false;
  double SalSpotLatDeg_ = 0.0, SalSpotLonDeg_ = 0.0, SalSpotElevM_ = 0.0;

  Phase  Phase_ = Phase::Inertial;
  int    LauncherId_ = 0;
  double NowS_ = 0.0, LaunchS_ = 0.0;

  /* The estimate the law flies against, and WHEN it was last actually measured — the age of this stamp
   * is what "flying on the last information" means. */
  bool   HaveTarget_ = false;
  double TgtLatDeg_ = 0.0, TgtLonDeg_ = 0.0, TgtAltM_ = 0.0;
  double TgtVelE_ = 0.0, TgtVelN_ = 0.0, TgtVelU_ = 0.0;
  double TgtStampS_ = -1e9;
  bool   SeekerLocked_ = false;

  double RangeM_ = 0.0, ClosureMs_ = 0.0, LosRateDegS_ = 0.0;
  double NzCmdG_ = 0.0, NyCmdG_ = 0.0;
  double LosAzDeg_ = 0.0, LosElDeg_ = 0.0;
  double FinPitch_ = 0.0, FinYaw_ = 0.0;
  double NzInt_ = 0.0, NyInt_ = 0.0;   /* the two acceleration loops' integrators */
};

} // namespace FlightBox::Modules
#endif
