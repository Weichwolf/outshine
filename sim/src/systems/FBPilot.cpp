#include "FBPilot.h"
#include "FBGeodesy.h"
#include <cmath>

namespace FlightBox {

namespace {
const double kPreflightHoldS = 2.0;    /* brief preflight pause before the roll (class banner) */

/* Centerline steering law gains (generic — the regulated quantity is cross-track/heading error to the
 * runway axis, not an airframe-specific gain; doc/f16/procedures-takeoff-taxi.md's NWS is the actuator,
 * not this law). Small: the F-16's own steer-cmd-norm -> deg schedule is itself very sensitive at low
 * taxi speed (80 deg/unit at ~6 kt, f16.xml), so a gentle norm command is already a firm correction. */
const double kSteerXtGainPerM   = 0.01;
const double kSteerHdgGainPerDeg = 0.02;
const double kSteerCmdMax = 0.6;

/* Pitch-attitude-hold stick: a simple PD to a target pitch (mirrors flightctl.h's own Raw-path pitch
 * PD) — Takeoff's rotation pull, Flare's touchdown flare, and Rollout's aerobrake/derotate all reduce to
 * "hold this pitch attitude", just with a different target/stick-authority cap per phase (kRotateStickMax
 * below is Takeoff/Rollout's full-authority pull; Flare uses a gentler cap, see kFlareStickMax). */
const double kRotateKp = 0.15, kRotateKd = 0.02, kRotateStickMax = 1.0;
const double kFlareStickMax = 0.6;      /* gentler near the ground — the flare is a soft correction, not
                                          * a rotation pull */
const double kPositiveRateMs = 0.5;      /* liftoff-to-gear-up guard: a real climb, not ground noise */
const double kGearUpAglFt = 10.0;        /* + a firm AGL margin: JSBSim's FGLGear freezes WOW the instant
                                          * gear-pos-norm first drops <=0.99 (no gear-up branch recomputes
                                          * it after that) — starting retraction mid-bounce would freeze a
                                          * stale WOW=true, so wait for a confirmed climb-away first. Kept
                                          * small (not a large altitude buffer): the model accelerates
                                          * through the 5 s gear-transit fast enough in full afterburner
                                          * that starting late risks the 300 kt gear-transit speed limit
                                          * (doc/f16/procedures-takeoff-taxi.md) — matches real technique
                                          * (gear up within a couple of seconds of a positive-rate liftoff,
                                          * not after a long climb-away). */

/* BFM inner loop (the Bfm phase). Two axes, two laws, both commanding the airframe's OWN flight control
 * system through the stick — this phase hand-flies (Guidance::Manual) exactly as Takeoff/Flare/Rollout
 * do, and for the same reason: FBAutopilot's Direct/Course are navigation modes whose 60-deg bank cap
 * and deliberately gentle roll-in (FBFlightControl::F16's RollStickMax = 0.15, a cruise number) are
 * structurally wrong for a fight, and re-tuning them would move every existing mission's numbers.
 *
 * THE LAW, in one paragraph. An aircraft can only accelerate along its own lift axis (belly to canopy),
 * so a turn is first a ROLL that puts that axis where the acceleration is wanted and then a PULL. What
 * the pilot wants is (a) to bend the velocity vector towards the aim point at a rate that takes the
 * steering error out in about kBfmTurnTimeS seconds — a_turn = V * err / t — and (b) to keep gravity
 * from bending it somewhere else meanwhile. Both are vectors in the plane perpendicular to the velocity,
 * so add them: the lift the pilot needs is
 *      L = a_turn * (sin phi, cos phi) + (-g sin(roll) cos(pitch), +g cos(roll) cos(pitch))
 * in body (right, up) axes, where phi is the direction of the steering error in those same axes. The
 * roll command is then simply "put body-up along L" and the load factor is |L|/g. Nothing else is
 * needed, and three behaviours fall out of that one expression rather than being coded as cases:
 *   - zero error at any bank -> L is straight up in the WORLD -> the jet rolls wings level and holds 1 g
 *     (the tracking solution, with no separate tracking mode);
 *   - a pure azimuth error in level flight -> roll = atan(a_turn/g) and n = 1/cos(roll), which IS the
 *     coordinated-turn relation, arrived at rather than assumed;
 *   - a hard turn at 90 deg of bank -> gravity has no component along the lift axis at all, so the turn
 *     costs only its own g and the nose falls. That is correct, and it is what makes an energy fight
 *     possible — a law that insisted on level flight would silently spend 5.7 g holding an altitude
 *     nobody asked for.
 * The pull is capped by the g the CURRENT speed can actually buy and tracked by a PI on the load-factor
 * error (an FLCS's stick-to-g authority varies with speed). Deliberately asymmetric — full stick to
 * pull, a capped push: a fighter pulls and unloads, it does not bunt. The roll axis needs no damping
 * term: an FLCS airframe's lateral stick IS a rate command (f16.xml differences fcs/aileron-cmd-norm
 * against the measured roll rate), so a proportional law on the roll ANGLE error is already one. */
const double kBfmRollFullDeg = 60.0;      /* roll error that earns full lateral stick */
const double kBfmTurnTimeS = 2.0;         /* how quickly the pilot wants the steering error gone */
const double kBfmGKp = 0.25, kBfmGKi = 0.5, kBfmGIMax = 0.6;
const double kBfmPushMax = 0.3;           /* the push half of the stick (see above) */
const double kBfmSearchRangeM = 5556.0;   /* 3 nm: where a cold search aims, purely to have a point */
/* A search climbs firmly and descends gently, and the asymmetry is the point (see below): pulling UP is
 * an upright pull at any angle, while a steep DOWN demand is what makes the lift-vector law roll
 * inverted. Height lost in a bank is given back by climbing, not by diving after it. */
const double kBfmSearchUpMaxDeg = 20.0, kBfmSearchDownMaxDeg = 5.0;
const double kBfmFloorPullDeg = 30.0;     /* full nose-up bias when the AGL floor is reached */
const double kBfmG0 = 9.80665;
const double kBfmClosureDeadKt = 40.0;    /* how far off the closure schedule is worth reacting to */
const double kBfmOverspeedFrac = 1.15;    /* above this multiple of corner speed the throttle comes back */
const double kBfmThrTrim = 0.6, kBfmThrKpPerKt = 0.006;   /* +-67 kt of speed error = idle..full */
const double kBfmSpeedbrakeKt = 40.0;     /* boards out only when the throttle alone cannot do it */

double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
} // namespace

const char *FBPilot::PhaseName(Phase p) {
  switch (p) {
    case Phase::Idle: return "Idle";
    case Phase::Preflight: return "Preflight";
    case Phase::Takeoff: return "Takeoff";
    case Phase::Climb: return "Climb";
    case Phase::Route: return "Route";
    case Phase::Approach: return "Approach";
    case Phase::Flare: return "Flare";
    case Phase::Rollout: return "Rollout";
    case Phase::Shutdown: return "Shutdown";
    case Phase::Bfm: return "Bfm";
  }
  return "?";
}

/* along=0 at the threshold, +down the runway heading; +across = right of the runway axis — the SAME
 * convention FBMissionMonitor::OnRunway and FBAutopilot::SetCourse use (class banner), so Takeoff's
 * ground steering, Rollout's reuse of it, and the mission verdict all agree on "on the line". */
void FBPilot::RunwayAxis(const FBRunway &rwy, double lat, double lon, double &alongM, double &acrossM) {
  FBTrackProjectM(rwy.ThresholdLatDeg, rwy.ThresholdLonDeg, rwy.TrueHeadingDeg, lat, lon, alongM, acrossM);
}

double FBPilot::NosewheelSteerCmd(const FBRunway &rwy, double lat, double lon, double yawDeg) const {
  double along, across;
  RunwayAxis(rwy, lat, lon, along, across);
  (void)along;
  double hdgErr = FBWrap180(yawDeg - rwy.TrueHeadingDeg);
  return Clamp(-(kSteerXtGainPerM * across + kSteerHdgGainPerDeg * hdgErr), -kSteerCmdMax, kSteerCmdMax);
}

double FBPilot::PitchHoldStick(double targetDeg, double pitchDeg, double qDegS, double stickMax) const {
  return Clamp(kRotateKp * (targetDeg - pitchDeg) - kRotateKd * qDegS, -stickMax, stickMax);
}

/* ONE decision tick of the fight. The whole phase is "look at the picture, decide what kind of pursuit
 * this geometry calls for, aim at the point that pursuit implies, and fly the jet there with the energy
 * still available" — everything below is those four steps in order.
 *
 * THE PICTURE IS A RADAR TRACK AND NOTHING ELSE (systems/FBBfmTrack, CLAUDE.md "Kein Cheaten"): the
 * pilot never learns where the other jet IS, only where its own estimate says it should be. Losing the
 * lock therefore does not blind the pilot instantly, it starts a clock — the estimate keeps moving on
 * its last known vector and the nose keeps following it, and only once the coast has run long enough for
 * that prediction to be worth less than a look does the scan start weaving to walk the (body-fixed)
 * radar box across the uncertainty. Getting the target back into the scan volume is itself a manoeuvre
 * goal here, which is exactly what flying without eyes means. */
FBPilotCommands FBPilot::BfmCommands(const FBState &state, const fb_fdm_state &st, double dt) {
  Bfm_.Update(state, st, TimeS_);
  const FBBfmBlock &g = Bfm_.Block();
  /* The head answers the two questions this whole law branches on: is there anything at all (Readable)
   * and is it young enough to lead on (IsValid); Held = the frozen last measured datum, which is worth
   * turning back toward but not worth pulling lead on. */
  const bool haveTrack = g.H.Readable(), validTrack = g.H.IsValid();
  const double trackAgeS = haveTrack ? TimeS_ - g.H.StampS : 0.0;
  FBPilotCommands c{};
  c.Guidance = FBPilotGuidance::Manual;
  c.ManualYaw = 0.0;

  double casKt = st.cas * kMsToKt;
  double rngNm = g.RangeM * kMToNm;
  double closKt = g.ClosureMs * kMsToKt;
  double tgtSpeedMs = std::sqrt(g.VelE * g.VelE + g.VelN * g.VelN + g.VelU * g.VelU);

  /* OUT OF ENERGY is a relative statement, not an absolute one. Below the minimum manoeuvring speed the
   * jet has to stop spending height and start rebuilding speed — UNLESS the whole fight is being flown
   * that slowly, i.e. the target itself is slower still. A pursuer sitting in the control position behind
   * a defender in a hard, decelerating turn IS below its own corner band, and that is not a mistake to be
   * corrected with full afterburner: doing so throws the jet straight out in front of him (measured — the
   * absolute rule cost 250 of 268 seconds of control position). */
  bool lowEnergy = casKt < BfmMinSpeedKt() && (!validTrack || tgtSpeedMs > st.speed);

  /* ---- 1. what kind of pursuit does this geometry call for? ----
   * The overtake is a SCHEDULE, not a threshold: the closure a pursuer wants is proportional to how far
   * it still has to go, so it arrives at the control range with the range rate already killed. More
   * closure than the schedule allows is an overshoot in the making, and an overshoot hands the fight to
   * the defender — so it buys LAG (aim behind him), which both stops the nose going out in front and
   * costs nothing in energy. LEAD (aim where he will be) while the ANGLES are still the problem: a high
   * aspect means this jet is not on his tail yet and cutting the corner is what fixes that. PURE (aim at
   * him) in between, which is also the tracking solution once the control position is reached. */
  double ctrlMidNm = 0.5 * (BfmControlMinNm() + BfmControlMaxNm());
  double schedKt = Clamp((rngNm - ctrlMidNm) * BfmClosureGainKtPerNm(), -BfmMaxClosureKt(),
                         BfmMaxClosureKt());
  bool overtaking = validTrack && closKt > schedKt + kBfmClosureDeadKt;

  FBBfmPursuit mode;
  if (!validTrack) mode = FBBfmPursuit::Search;
  else if (rngNm < BfmControlMinNm() || overtaking) mode = FBBfmPursuit::Lag;
  else if (g.AspectDeg > BfmLeadAspectDeg() || rngNm > BfmLeadRangeNm()) mode = FBBfmPursuit::Lead;
  else mode = FBBfmPursuit::Pure;

  /* ---- 2. the aim point, as an offset from own position ---- */
  double aimE = g.EastM, aimN = g.NorthM, aimU = g.UpM;
  if (mode == FBBfmPursuit::Lead) {
    /* Collision-course lead: where he will be by the time this jet could be there. A TIME, not a fixed
     * angle, so the lead shrinks as the range closes and never asks for a turn into empty sky. */
    double tLead = Clamp(g.RangeM / std::fmax(st.speed, 1.0), 0.0, BfmLeadMaxS());
    aimE += g.VelE * tLead; aimN += g.VelN * tLead; aimU += g.VelU * tLead;
  } else if (mode == FBBfmPursuit::Lag) {
    /* Lag is two displacements, not one. BEHIND him along his own flight path is what stops the nose
     * going out in front; ABOVE him is what actually kills the overtake — pulling up out of his turn
     * plane trades the excess speed into height instead of burning it off with drag the jet does not
     * have, and hands it straight back on the way down. That is the high yo-yo, and it is the only
     * answer to a 100-kt overtake inside a mile that does not throw the fight away. The height scales
     * with how far over the closure schedule this jet actually is, so it unwinds by itself. */
    double excess = Clamp((closKt - schedKt) / std::fmax(BfmMaxClosureKt(), 1.0), 0.0, 1.0);
    aimE -= g.VelE * BfmLagTimeS(); aimN -= g.VelN * BfmLagTimeS(); aimU -= g.VelU * BfmLagTimeS();
    aimU += BfmYoYoHeightM() * excess;
  } else if (mode == FBBfmPursuit::Search) {
    /* SEARCH is flown as a DIRECTION and an ALTITUDE, never as a point. The estimate is too old to steer
     * a pursuit at, so what is left is "he was over there, at about that height": turn back onto the
     * bearing, hold the height, hold corner speed, and let the weave below walk the box across the
     * uncertainty. Aiming at the stale POINT instead would have the jet diving or zooming at a place the
     * target left long ago (measured: a 1500 m dive to a datum whose owner had flown a quarter of a turn
     * circle away from it), and leaving the altitude unreferenced would let the search spiral into the
     * ground — this law deliberately does not hold altitude in a bank (see the file's BFM banner), so
     * during a long search the aim point is the only thing that can.
     * With nothing ever seen there is no bearing either, so heading AND altitude are ANCHORED at the
     * moment the cold search begins and never re-read from the jet afterwards: aiming at "wherever my
     * nose points right now" is a control loop with no reference at all, and it does exactly what such a
     * loop does — the weave starts a roll, the roll turns the jet, the aim follows the turn, and the
     * search settles into a steady 80-deg-banked orbit that searches nothing (measured, before this). */
    double brgDeg;
    if (haveTrack) {
      brgDeg = std::atan2(g.EastM, g.NorthM) * kRad2Deg;
      aimU = g.UpM;
    } else {
      if (!BfmSearchAnchored_ && st.speed > 1.0) {
        BfmSearchHdgDeg_ = st.yaw;
        BfmSearchAltM_ = st.elev;
        BfmSearchAnchored_ = true;
      }
      brgDeg = BfmSearchAnchored_ ? BfmSearchHdgDeg_ : st.yaw;
      aimU = BfmSearchAnchored_ ? BfmSearchAltM_ - st.elev : 0.0;
    }
    double brg = brgDeg * kDeg2Rad;
    aimE = kBfmSearchRangeM * std::sin(brg);
    aimN = kBfmSearchRangeM * std::cos(brg);
    /* A search is flown UPRIGHT. Left unclamped, a datum a few hundred metres below the jet is a
     * steep-enough demand that the lift-vector law answers it the way it answers any large downward
     * demand — roll inverted and pull, a split-S — and a rolling jet searches nothing (measured: 2,000 m
     * of zoom and roll, with the target sitting at the datum's altitude in plain view of nobody). Capping
     * the demand at kBfmSearchElMaxDeg keeps the required turn under 1 g, which the law then flies as a
     * gentle upright climb or descent. */
    aimU = Clamp(aimU, -std::tan(kBfmSearchDownMaxDeg * kDeg2Rad) * kBfmSearchRangeM,
                 std::tan(kBfmSearchUpMaxDeg * kDeg2Rad) * kBfmSearchRangeM);
  }

  if (haveTrack) BfmSearchAnchored_ = false;   /* a datum exists again: the next cold search re-anchors */

  /* THE SCAN. Once the picture is older than BfmScanAfterS the aim DIRECTION is swept about the vertical
   * — the radar's volume is bolted to the nose, so walking the nose across the uncertainty is the only
   * way to search more of the sky than the mode's own box covers. Rotating the aim point in the world
   * rather than adding degrees to the body-frame azimuth keeps the steering error a coherent direction
   * whatever bank the jet is in. The weave must stay GENTLE: its own heading rate (2*pi*A/T) is a
   * steering error like any other, and a fast wide scan simply has the pilot flying a hard turn chasing
   * its own search pattern — measured, a 20 deg / 10 s weave settled the jet into a permanent 77 deg
   * banked orbit and acquired nothing at all. */
  if (!validTrack || trackAgeS > BfmScanAfterS()) {
    double w = BfmScanAmplitudeDeg() * std::sin(2.0 * kPi * TimeS_ / BfmScanPeriodS()) * kDeg2Rad;
    double cw = std::cos(w), sw = std::sin(w);
    double e = aimE * cw + aimN * sw, n = -aimE * sw + aimN * cw;
    aimE = e; aimN = n;
  }

  /* ENERGY, expressed in the aim point rather than as a mode of its own: below the minimum manoeuvring
   * speed the fight does not go uphill any more — the aim point is dropped to the horizon so height stops
   * being bought with speed the jet no longer has. Deliberately a CLAMP and not a nose-down bias: a
   * negative elevation demand rolls the lift vector INVERTED (this law points the LIFT axis at the aim
   * point, and "below" means 180 deg of roll), which is a split-S, not an unload — measured, and it
   * dumped the jet 2,900 m while the pilot was only trying to regain 50 kt. */
  if (lowEnergy && aimU > 0.0) aimU = 0.0;

  double azErr = 0.0, elErr = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, aimE, aimN, aimU, azErr, elErr);

  /* The AGL floor outranks everything above it: a fight flown into the ground is not a fight won. */
  const FBRadarAltBlock &ra = state.RadarAlt;
  if (BfmFloorFt() > 0.0 && ra.H.Readable() && ra.AglFt < BfmFloorFt())
    elErr += kBfmFloorPullDeg * Clamp(1.0 - ra.AglFt / BfmFloorFt(), 0.0, 1.0);

  /* ---- 4. fly it: one lift vector, one load factor (see the file's BFM banner for the derivation) ---- */
  double errMag = std::sqrt(azErr * azErr + elErr * elErr);
  double vRatio = casKt / std::fmax(BfmCornerSpeedKt(), 1.0);
  double gAvail = Clamp(BfmCornerG() * vRatio * vRatio, 1.0, BfmMaxG());
  double aTurn = std::fmin(st.speed * (errMag * kDeg2Rad) / kBfmTurnTimeS, gAvail * kBfmG0);
  double dirRight = errMag > 1e-6 ? azErr / errMag : 0.0;
  double dirUp = errMag > 1e-6 ? elErr / errMag : 1.0;
  double gravRight = -kBfmG0 * std::sin(st.roll * kDeg2Rad) * std::cos(st.pitch * kDeg2Rad);
  double gravUp = kBfmG0 * std::cos(st.roll * kDeg2Rad) * std::cos(st.pitch * kDeg2Rad);
  double liftRight = aTurn * dirRight + gravRight;
  double liftUp = aTurn * dirUp + gravUp;

  double phiCmd = std::atan2(liftRight, liftUp) * kRad2Deg;
  c.ManualRoll = Clamp(phiCmd / kBfmRollFullDeg, -1.0, 1.0);

  double gCmd = Clamp(std::sqrt(liftRight * liftRight + liftUp * liftUp) / kBfmG0, 0.0, gAvail);
  if (lowEnergy) gCmd = std::fmin(gCmd, BfmUnloadG());

  double gErr = gCmd - st.nz;
  BfmGIterm_ = Clamp(BfmGIterm_ + kBfmGKi * gErr * dt, -kBfmGIMax, kBfmGIMax);
  c.ManualPitch = Clamp(kBfmGKp * gErr + BfmGIterm_, -kBfmPushMax, 1.0);

  /* THROTTLE IS THE OTHER HALF OF THE CLOSURE PROBLEM. Aiming behind him stops the nose going out in
   * front; it does not stop a jet that is simply 100 kt faster than the one it is trying to sit behind
   * — that one swings out of the control zone every time, however well it is aiming. So the pilot flies
   * the SPEED the geometry wants: the target's own speed (the track estimate knows it — that is what a
   * velocity estimate is for) plus exactly the overtake the closure schedule allows at this range, which
   * goes NEGATIVE inside the control zone. With no picture at all there is nothing to match, so the
   * pilot parks the jet at corner speed, where any fight it might find is best entered. */
  double speedErrKt;
  if (validTrack) {
    speedErrKt = (tgtSpeedMs - st.speed) * kMsToKt + schedKt;
  } else {
    speedErrKt = BfmCornerSpeedKt() - casKt;
  }
  c.ManualThr = Clamp(kBfmThrTrim + kBfmThrKpPerKt * speedErrKt, 0.0, 1.0);
  if (lowEnergy) c.ManualThr = 1.0;   /* out of energy outranks the geometry (see lowEnergy above) */
  if (casKt > BfmCornerSpeedKt() * kBfmOverspeedFrac)
    c.ManualThr = std::fmin(c.ManualThr, kBfmThrTrim);   /* past corner the extra knots buy no turn rate */
  c.Speedbrake = speedErrKt < -kBfmSpeedbrakeKt ? 1.0 : 0.0;

  bool inControl = validTrack && g.Locked && rngNm >= BfmControlMinNm() && rngNm <= BfmControlMaxNm() &&
                   g.AspectDeg <= BfmControlAspectDeg() && std::fabs(g.AzDeg) <= BfmControlAtaDeg();
  Bfm_.Report(mode, inControl, gCmd, dt);
  return c;
}

/* The cockpit half of a decision tick (see the header's brief block). One post per tick, in a fixed
 * order, so the stream is deterministic and so the pilot behaves like a pilot: it works one control at
 * a time. Everything here goes through the same bus a human's hands would drive — there is no other
 * path from this class to an avionics box, by construction (the pilot holds no system pointers). */
void FBPilot::EnterBriefedItems(FBCommandBus &avionics) {
  if (TimeS_ < BriefNextTryS_) return;
  BriefNextTryS_ = TimeS_ + kBriefRetryS;
  if (BriefAlowPending_) {
    if (avionics.Post(FBCommandTarget::AlowFt, BriefAlowFt_, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefAlowPending_ = false;
    return;
  }
  if (BriefBingoPending_) {
    if (avionics.Post(FBCommandTarget::BingoLbs, BriefBingoLbs_, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefBingoPending_ = false;
    return;
  }
  if (BriefArmPending_) {
    if (avionics.Post(FBCommandTarget::MasterArm, BriefArm_ ? 1.0 : 0.0, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefArmPending_ = false;
    return;
  }
  if (BriefWeaponPending_) {
    if (avionics.Post(FBCommandTarget::WeaponSelect, BriefWeapon_, TimeS_).Outcome != FBCommandOutcome::Rejected)
      BriefWeaponPending_ = false;
    return;
  }
}

bool FBPilot::BriefRelease(double atS) {
  if (ReleaseCount_ >= kMaxBriefedReleases) return false;
  ReleaseAtS_[ReleaseCount_++] = atS;
  return true;
}

/* The pickle, when the brief says so: ONE command, posted once, never retried (see BriefRelease). The
 * station is the SMS's own selection — this stage has no aiming and therefore no reason for the pilot
 * to prefer a pylon; what it does have is the rest of the chain (master arm, the interlocks, the
 * receipt), which is exactly what a release over the bus is here to exercise. */
void FBPilot::ReleaseBriefedStores(FBCommandBus &avionics) {
  while (ReleaseNext_ < ReleaseCount_ && TimeS_ >= ReleaseAtS_[ReleaseNext_]) {
    ReleaseNext_++;
    avionics.Post(FBCommandTarget::WeaponRelease, 1.0, TimeS_);
  }
}

bool FBPilot::BriefChaff(double atS) {
  if (DispenseCount_ >= kMaxBriefedDispenses) return false;
  DispenseAtS_[DispenseCount_++] = atS;
  return true;
}

/* The CMS switch, when the brief says so: one HOTAS-class command per briefed moment, posted once and
 * never retried (see BriefChaff). Value 0 = the program the PRGM knob selects, which is what CMS
 * Forward means on the real switch — the pilot throws what the jet was set up with, they do not pick a
 * program in the middle of a manoeuvre. */
void FBPilot::DispenseBriefedCm(FBCommandBus &avionics) {
  while (DispenseNext_ < DispenseCount_ && TimeS_ >= DispenseAtS_[DispenseNext_]) {
    DispenseNext_++;
    avionics.Post(FBCommandTarget::CmDispense, 0.0, TimeS_);
  }
}

FBPilotCommands FBPilot::Run(const FBState &state, FBCommandBus &avionics,
                             const FBAirframeControls &airframe, const fb_fdm_state &st,
                             const FBFlightPlan &plan, const FBRunway *runway, double dt) {
  PhaseElapsedS += dt;
  TimeS_ += dt;
  FBPilotCommands c{};

  /* Cockpit work happens once the jet is flying itself — not while the phase machine is Idle (nobody
   * is in the seat yet) and not with weight on the wheels, where the real checklist order puts these
   * entries before engine start, outside this class's phases entirely. */
  if (CurPhase != Phase::Idle && !airframe.GetWeightOnWheels()) {
    EnterBriefedItems(avionics);
    ReleaseBriefedStores(avionics);
    DispenseBriefedCm(avionics);
  }

  /* Mission waypoint bookkeeping (telemetry cache — class banner): the same active-waypoint distance
   * the mission runner used to compute itself from the outside. */
  ActiveWpCache = plan.ActiveIndex();
  DistToWpCache = -1.0;
  if (const FBWaypoint *awp = plan.ActiveWaypoint()) {
    DistToWpCache = FBPlanarDistM(st.lat, st.lon, awp->LatDeg, awp->LonDeg);
  }

  switch (CurPhase) {
    case Phase::Idle:
      return c;   /* neutral — untouched (see the class banner) */

    case Phase::Preflight: {
      /* A ground-spawn check: not on the ground (e.g. an airborne --fly demo that reaches Preflight
       * without a runway) has nothing sane for a preflight to do — stay neutral (Guidance::None, no
       * airframe demand), matching the pre-Phase-1 contract for every caller that isn't a runway
       * ground-start. */
      if (!airframe.GetWeightOnWheels()) return c;
      c.Guidance = FBPilotGuidance::Manual;   /* idle throttle, wings level, wheels chocked */
      c.GearDown = true;
      c.WheelBrakeLeft = 1.0; c.WheelBrakeRight = 1.0;
      bool engineOk = airframe.GetEngineRunning(0);
      if (engineOk && PhaseElapsedS >= kPreflightHoldS) Transition(Phase::Takeoff);
      return c;
    }

    case Phase::Takeoff: {
      c.Guidance = FBPilotGuidance::Manual;
      c.GearDown = true;
      c.WheelBrakeLeft = 0.0; c.WheelBrakeRight = 0.0;   /* brakes released */
      c.ManualThr = TakeoffThrottleNorm();
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;

      if (runway) c.NosewheelSteer = NosewheelSteerCmd(*runway, st.lat, st.lon, st.yaw);

      double vr = RotationSpeedKt(airframe.GetGrossWeightLbs());
      double casKt = st.cas * kMsToKt;
      if (casKt >= vr - RotationLeadKt())
        c.ManualPitch = PitchHoldStick(RotationPitchDeg(), st.pitch, st.q, kRotateStickMax);
      else
        c.ManualPitch = 0.0;   /* stick neutral until the rotate call */

      if (!airframe.GetWeightOnWheels()) Transition(Phase::Climb);
      return c;
    }

    case Phase::Climb: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = ClimbSpeedKt();
      /* Positive rate + a confirmed AGL margin (kGearUpAglFt) + below the gear-transit speed limit ->
       * gear up; otherwise leave GearDown unset (it is still commanded down from Takeoff, the safe
       * default) rather than force a retraction. */
      /* Every AGL gate below asks the radar-altitude block's HEAD first. Without a valid one the
       * pilot cannot confirm the height and does NOT act on a number it has no reason to trust — the
       * gear stays down, the flare does not trigger, the BFM floor stops pulling (doc/f16/
       * controls-commands.md §6.4: the sensor gates the effect, not the command). */
      if (st.vy > kPositiveRateMs && state.RadarAlt.H.Readable() &&
          state.RadarAlt.AglFt > kGearUpAglFt && st.cas * kMsToKt < GearUpLimitKt())
        c.GearDown = false;
      if (airframe.GetGearPosition() <= 0.02) Transition(Phase::Route);
      return c;
    }

    case Phase::Route: {
      const FBWaypoint *wp = plan.ActiveWaypoint();
      if (!wp) { Transition(Phase::Shutdown); return c; }   /* no waypoints left -> the mission SUCCESS gate */
      if (wp->Type == FBWaypointType::Land) { Transition(Phase::Approach); return c; }
      c.Guidance = FBPilotGuidance::Direct;
      c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
      c.TargetAltM = wp->AltM;
      c.TargetSpeedKt = wp->SpeedKt;
      return c;
    }

    case Phase::Approach: {
      /* No assigned runway -> nothing sane to land on; stay neutral rather than guess (mirrors
       * Preflight's own "nothing to do" contract). */
      if (!runway) { Transition(Phase::Shutdown); return c; }
      /* Touched down before FlareStartAglFt tripped (a short/high-sink final) — Rollout still handles
       * it correctly (aerobrake-or-derotate is purely a CAS schedule, not conditioned on having flared). */
      if (airframe.GetWeightOnWheels()) { Transition(Phase::Rollout); return c; }

      c.Guidance = FBPilotGuidance::Course;
      c.TargetLatDeg = runway->ThresholdLatDeg; c.TargetLonDeg = runway->ThresholdLonDeg;
      c.CourseDeg = runway->TrueHeadingDeg;
      c.TargetAltM = runway->ThresholdElevM;
      c.GlidepathDeg = GlidepathAngleDeg();
      c.TargetSpeedKt = ApproachSpeedKt();
      c.Speedbrake = ApproachSpeedbrakeNorm();
      if (st.cas * kMsToKt < GearUpLimitKt()) c.GearDown = true;

      if (state.RadarAlt.H.Readable() && state.RadarAlt.AglFt <= FlareStartAglFt())
        Transition(Phase::Flare);
      return c;
    }

    case Phase::Flare: {
      if (airframe.GetWeightOnWheels()) { Transition(Phase::Rollout); return c; }
      c.Guidance = FBPilotGuidance::Manual;
      c.ManualThr = 0.0;      /* throttle to idle (doc/f16/procedures-landing.md's Short Final) */
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;
      c.ManualPitch = PitchHoldStick(FlareTargetPitchDeg(), st.pitch, st.q, kFlareStickMax);
      c.Speedbrake = ApproachSpeedbrakeNorm();
      return c;
    }

    case Phase::Rollout: {
      c.Guidance = FBPilotGuidance::Manual;
      c.ManualThr = 0.0;
      c.ManualRoll = 0.0; c.ManualYaw = 0.0;
      c.Speedbrake = 1.0;   /* fully open (doc/f16/procedures-landing.md's Roll-Out) */

      /* Two-point aerodynamic braking above AerobrakeSpeedKt (target held at AerobrakePitchDeg, well
       * under FBFlightMonitor's 15-deg attitude-contact K.O.), a proportional derotate to nose-wheel-down
       * below it — same PD as Takeoff's rotate/Flare's flare, just a speed-scheduled target. */
      double casKt = st.cas * kMsToKt;
      double brakeKt = AerobrakeSpeedKt();
      double targetPitch = casKt > brakeKt ? AerobrakePitchDeg()
                                            : AerobrakePitchDeg() * Clamp(casKt / brakeKt, 0.0, 1.0);
      c.ManualPitch = PitchHoldStick(targetPitch, st.pitch, st.q, kRotateStickMax);

      if (runway) c.NosewheelSteer = NosewheelSteerCmd(*runway, st.lat, st.lon, st.yaw);

      /* Wheel brakes: negligible while the nose is still held up (two-point stance, doc: "F-16 wheel
       * brakes are weak" — aerobrake does the work), moderate-heavy once derotating toward the nosewheel. */
      double brake = casKt <= brakeKt ? RolloutBrakeNorm() : 0.0;
      c.WheelBrakeLeft = brake; c.WheelBrakeRight = brake;
      return c;
    }

    case Phase::Bfm:
      return BfmCommands(state, st, dt);

    case Phase::Shutdown:
    default:
      return c;
  }
}

void FBPilot::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("phase");
  schema.Add("activeWp");
  schema.Add("distToWpM", "m");
}

void FBPilot::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push(std::string(PhaseName(CurPhase)));
  row.Push(ActiveWpCache);
  row.Push(DistToWpCache);
}

} // namespace FlightBox
