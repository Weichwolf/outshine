#include "FBPilot.h"
#include "FBGeodesy.h"
#include "FBLog.h"
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
/* A ROLL RATE THE PILOT CAN STOP. The lift-vector command above is an ANGLE error and the F-16's lateral
 * stick is a rate command, so a large error means full stick for as long as it takes — and the largest
 * error this law can ever produce is 180 deg (it always takes the short way round). Held at full stick
 * that is a snap roll: measured, a re-acquisition at 3.7 nm demanded 230 deg of roll, the jet answered
 * with 150 deg/s for three seconds, and the flight monitor called it what it looks like from outside —
 * a departure. So the pilot governs itself. The ORDER of the number comes from the law it serves: the
 * roll exists to establish a turn whose own time constant is kBfmTurnTimeS, so even the worst case has
 * to be flown in about that time — tens of degrees per second, not hundreds. The VALUE is measured
 * across sixteen gun engagements against two defenders: ungoverned and at 90 deg/s the fight departed in
 * six of them, at 60 deg/s in none, and the gunnery was best there too. The governor only ever REDUCES a
 * command, so it can never add stick. */
const double kBfmRollRateMaxDegS = 120.0 / kBfmTurnTimeS;
const double kBfmGKp = 0.25, kBfmGKi = 0.5, kBfmGIMax = 0.6;
const double kBfmPushMax = 0.3;           /* the push half of the stick (see above) */
const double kBfmSearchRangeM = 5556.0;   /* 3 nm: where a cold search aims, purely to have a point */
/* THE GUN TRACKING LOOP's rate term (see BfmCommands' section 3c). The estimate is a difference of one
 * published float across two 10-Hz decision ticks, so it is filtered — the same first-order filter and
 * for the same reason the track's own velocity is (systems/FBBfmTrack::kVelAlpha): fast enough to follow
 * a reversal, slow enough that a single noisy look cannot swing the nose. */
const double kBfmLeadRateAlpha = 0.4;
/* ...and its integral. Feed-forward alone leaves whatever the RATE ESTIMATE itself got wrong: a filter
 * has lag, and the required bore's motion depends weakly on own velocity, so the estimate is a little
 * short and the loop settles a little behind — measured, a steady 1.45 deg with the solution's own
 * tolerance near 0.4 deg. A constant offset in a loop that already has proportional and rate terms is
 * exactly what an integrator is for, and here it converges on a specific number: at equilibrium the
 * error is zero and the integrator holds precisely the feed-forward's own shortfall.
 * ITS GAIN IS NOT A TASTE. With the law commanding a turn RATE of (e + I)/T against an angle, the loop
 * closes as s^2 + s/T + Ki/T = 0, i.e. a damping ratio of 1/(2*sqrt(Ki*T)) — so Ki = 1/(2T) is exactly
 * 0.707, the textbook "settles without ringing" root, and nothing else has to be chosen. Twice that was
 * tried first (zeta 0.5) and it showed: against a defender in a break turn it was a clear win, against
 * one flying straight the loop rang and the funnel time collapsed. The clamp bounds wind-up while the
 * geometry is being captured; the funnel breaking resets it outright. */
const double kBfmTrackKi = 0.5 / kBfmTurnTimeS, kBfmTrackIMaxDeg = 10.0;
/* A SEARCH IS A SCAN, NOT A TURN. The weave's own heading rate is 2*pi*A/T, and the pilot's declared
 * amplitude/period (BfmScanAmplitudeDeg/BfmScanPeriodS) fix it — so when the uncertainty demands a WIDER
 * sweep the period grows with it and the rate stays what the airframe's own hooks say it should be.
 * Wider than this and the weave stops being a search pattern and becomes a reversal, at which point what
 * the pilot is looking at is not the datum any more. */
const double kBfmScanMaxAmpDeg = 45.0;
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

/* INTERCEPT (the Intercept phase). The four numbers below are properties of the PILOT and of the
 * geometry, not of the airframe, which is why they are constants here rather than virtual hooks: an
 * F-16 and a MiG-29 have different gimbal limits and different launch zones (those ARE hooks), but a
 * human's reaction time and the width of an antenna-elevation deadband are the same in both cockpits.
 *
 * kInterceptReactionS is the one number in this file that models a PERSON: perceive the warning,
 * recognise what it means, decide, move. Published figures for a trained pilot answering an
 * unambiguous, expected cue cluster around 0.5-1.5 s; 1.0 s is the middle of that, and it sits ON TOP
 * of the command bus's own 0.5 s HOTAS latency (core/FBCommandBus), so the earliest a defensive command
 * can take effect is a second and a half after the receiver lit up. That is the whole point of having
 * it: an AI that answered in the tick the block changed would be winning on reflexes it does not have.
 *
 * kInterceptActionS is the same idea applied to the HANDS: a pilot works one control at a time, and the
 * avionics itself uses 0.5 s to tell two commands on one switch apart (core/FBCommandBus::
 * kHotasLatencyS). So this phase posts at most one command per 0.5 s, in priority order, however many
 * things it would like to have done at once.
 *
 * Both are DEFAULTS rather than laws: a mission may override either through the pilot variant
 * (systems/FBPilotTuning, `set pilot_react_s` / `set pilot_action_s`), which is how a tournament asks
 * what a slower or a quicker pilot is worth. Nothing else about them changes — the override still sits
 * on top of the command bus's own latency, so no variant can answer faster than the jet can. */
const double kInterceptReactionS = 1.0;
const double kInterceptActionS = 0.5;
/* How far the antenna has to be off the wanted elevation before it is worth another switch action. A
 * search pattern is +/-10.5 deg tall (modules/f16/FBF16Fcr), so a 2-deg deadband is well inside the
 * beam and keeps the pilot from spending the whole engagement re-pointing it. */
const double kInterceptElDeadDeg = 2.0;
/* How stale a contact may get before this phase calls it lost and goes back to searching. Two CRM
 * frames plus a margin: one missed sweep is a missed sweep, three is a target that is no longer there. */
const double kInterceptLostS = 10.0;
/* The Direct guidance target is a POINT, and a heading demand has to become one. Far enough that the
 * bearing to it is the heading asked for to within a fraction of a degree over a whole tick. */
const double kInterceptAimM = 60.0 * kNmToM;
/* The longest a shot is supported when the fire control never predicted an activation time (see the
 * Support state). Longer than any time of flight this simulator's weapon catalogue produces. */
const double kInterceptSupportMaxS = 60.0;
/* HOW LONG A FRESH LOCK IS LET SETTLE BEFORE THE TRIGGER. A single-target track is not a firing
 * solution the instant the antenna stops sweeping: the round is programmed with the target's estimated
 * MOTION (core/FBWeaponUplink), and that estimate is differenced from successive looks and filtered
 * (systems/FBBfmTrack::kVelAlpha, a ~0.4 s time constant on a 0.1 s STT frame). Shooting inside a
 * second of designating means launching a round programmed with a velocity of nearly zero — measured:
 * the aspect the shot was taken at could not even be computed. Two seconds is several filter time
 * constants and is also what a real firing sequence costs in switchology. */
const double kInterceptTrackSettleS = 2.0;

double Clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }

/* The bearing/altitude pair a Direct-guidance aim point needs, from a heading demand. */
void AimAlongHeading(const fb_fdm_state &st, double hdgDeg, double &latDeg, double &lonDeg) {
  double h = hdgDeg * kDeg2Rad;
  double coslat = std::cos(st.lat * kDeg2Rad);
  latDeg = st.lat + kInterceptAimM * std::cos(h) / kMPerDeg;
  lonDeg = st.lon + kInterceptAimM * std::sin(h) / (kMPerDeg * (std::fabs(coslat) > 1e-6 ? coslat : 1e-6));
}
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
    case Phase::Intercept: return "Intercept";
    case Phase::Attack: return "Attack";
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

/* The airframe's own corner numbers, read as a turn rate (header). n and V are this pilot's hooks, so a
 * module that flies a different jet gets a different assumption for free. */
double FBPilot::CornerTurnRateDegS() const {
  double v = std::fmax(BfmCornerSpeedKt() * kKtToMs, 1.0);
  double n = std::fmax(BfmCornerG(), 1.001);
  return kBfmG0 * std::sqrt(n * n - 1.0) / v * kRad2Deg;
}

/* THE WEAVE. Two patterns, and which one is flown depends on whether the pilot is looking for SOMEWHERE
 * or for anywhere at all.
 *
 * WITH NO DATUM there is nothing to centre a pattern on — nothing has ever been seen — so this stays the
 * pattern it always was: the airframe's declared amplitude and period, phased on the mission clock, a
 * lazy S across whatever heading the cold search anchored itself on.
 *
 * WITH A DATUM the pattern acquires a centre, and both of its numbers then follow from that centre:
 *   ITS PHASE IS ANCHORED TO THE MOMENT THE SEARCH STARTS, so the sweep BEGINS on the datum bearing —
 *   the most likely bearing there is — and walks outward symmetrically from it. Phased on the mission
 *   clock instead, the pilot enters every search at whatever point of the sine the clock happens to be
 *   at, and the same merge two seconds earlier is a different search: measured across sixteen merges of
 *   one geometry, reacquisition ranged from 34 s to never again, on nothing but that phase.
 *   ITS WIDTH IS WHAT THE PILOT DOES NOT KNOW. A fixed amplitude sweeps the same few degrees whether he
 *   was lost a second ago or a minute ago; the datum's half-width (systems/FBBfmTrack's FBTrackDatum) is
 *   exactly the angle the uncertainty subtends from here, so that is the sweep. The PERIOD grows with it
 *   so the weave's own heading rate stays the gentle number the airframe's hooks declare — a wider
 *   search takes longer, it does not turn harder. */
double FBPilot::SearchWeaveDeg(const FBTrackDatum &datum, bool searching) {
  double base = std::fmax(BfmScanAmplitudeDeg(), 1e-3);
  if (!searching || !datum.Valid) {
    ScanRunning_ = false;
    return searching ? base * std::sin(2.0 * kPi * TimeS_ / BfmScanPeriodS()) : 0.0;
  }
  if (!ScanRunning_) { ScanSinceS_ = TimeS_; ScanRunning_ = true; }
  double amp = Clamp(datum.HalfWidthDeg, base, kBfmScanMaxAmpDeg);
  double period = std::fmax(BfmScanPeriodS() * amp / base, 1e-3);
  return amp * std::sin(2.0 * kPi * (TimeS_ - ScanSinceS_) / period);
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
FBPilotCommands FBPilot::BfmCommands(const FBState &state, FBCommandBus &avionics,
                                    const fb_fdm_state &st, double dt) {
  Bfm_.Update(state, st, TimeS_);
  const FBBfmBlock &g = Bfm_.Block();
  /* The head answers the two questions this whole law branches on: is there anything at all (Readable)
   * and is it young enough to lead on (IsValid); Held = the frozen last measured datum, which is worth
   * turning back toward but not worth pulling lead on. */
  const bool haveTrack = g.H.Readable(), validTrack = g.H.IsValid();
  const double trackAgeS = haveTrack ? TimeS_ - g.H.StampS : 0.0;
  /* WHAT THE PILOT STILL KNOWS once the picture goes stale: where the target could be by now and how
   * wide "could" has become (systems/FBBfmTrack's FBTrackDatum). Asked for every tick, used only while
   * searching — computing it unconditionally keeps the search's reference the same object the whole
   * time rather than something that appears when the pursuit gives up. */
  const FBTrackDatum datum = Bfm_.Datum(st, TimeS_, CornerTurnRateDegS());
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
  /* THE CONTROL POSITION this fight is being flown to. Read through the variant table (systems/
   * FBPilotTuning) rather than straight off the airframe's hooks, because it is the one BFM number a
   * mission genuinely has to be able to change: the range band that is right for holding a firing
   * position with a missile is outside the gun's own funnel (doc/f16/weapons.md §2.5 puts that at
   * 600-3,000 ft), so a guns brief IS a different control position and nothing else about the fight
   * changes with it. Unset = this pilot's own numbers, so every existing mission is untouched. */
  const double ctrlMinNm = Tuned(FBPilotParam::BfmCtrlMinNm, BfmControlMinNm());
  const double ctrlMaxNm = Tuned(FBPilotParam::BfmCtrlMaxNm, BfmControlMaxNm());
  double ctrlMidNm = 0.5 * (ctrlMinNm + ctrlMaxNm);
  double schedKt = Clamp((rngNm - ctrlMidNm) * BfmClosureGainKtPerNm(), -BfmMaxClosureKt(),
                         BfmMaxClosureKt());
  bool overtaking = validTrack && closKt > schedKt + kBfmClosureDeadKt;

  FBBfmPursuit mode;
  if (!validTrack) mode = FBBfmPursuit::Search;
  else if (rngNm < ctrlMinNm || overtaking) mode = FBBfmPursuit::Lag;
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
    /* SEARCH is flown as a DIRECTION and an ALTITUDE, never as a point, and it is flown at the DATUM
     * rather than at the frozen last-seen position. Those are two different places and the difference is
     * this round's finding: the block reverts to where he was actually MEASURED, and a defender in a
     * break turn has flown a quarter of his circle away from there by the time the search starts.
     * systems/FBBfmTrack's FBTrackDatum propagates the last known VECTOR for as long as that prediction
     * is worth more than the last look (2/w, derived) and reports how wide the region has grown since,
     * so what is left is "he could be over there, at about that height, within about so much": turn back
     * onto the bearing, hold the height, hold corner speed, and let the weave below cover the width.
     * Aiming at the stale POINT instead had the jet diving or zooming at a place the target left long ago
     * (measured: a 1500 m dive to a datum whose owner had flown a quarter of a turn circle away from it),
     * and leaving the altitude unreferenced would let the search spiral into the ground — this law deliberately does not hold altitude in a bank (see the file's BFM banner), so
     * during a long search the aim point is the only thing that can.
     * With nothing ever seen there is no bearing either, so heading AND altitude are ANCHORED at the
     * moment the cold search begins and never re-read from the jet afterwards: aiming at "wherever my
     * nose points right now" is a control loop with no reference at all, and it does exactly what such a
     * loop does — the weave starts a roll, the roll turns the jet, the aim follows the turn, and the
     * search settles into a steady 80-deg-banked orbit that searches nothing (measured, before this). */
    double brgDeg;
    /* ...while it is still a PLACE. Once this jet is INSIDE the region the datum describes, the bearing
     * to its centre is not information any more — he is as likely to be behind as ahead — and steering
     * at it does what steering at a point one is sitting on always does: the bearing swings through 180
     * degrees, the law answers with a maximum-rate reversal, and the search turns into an orbit. Inside
     * it, the honest search is the cold one: hold a heading and a height and let the scan work. */
    if (datum.Valid && datum.RangeM > datum.RadiusM) {
      brgDeg = datum.BearingDeg;
      aimU = datum.UpM;
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
  double w = SearchWeaveDeg(datum, !validTrack || trackAgeS > BfmScanAfterS()) * kDeg2Rad;
  double cw = std::cos(w), sw = std::sin(w);
  double weaveE = aimE * cw + aimN * sw, weaveN = -aimE * sw + aimN * cw;
  aimE = weaveE; aimN = weaveN;

  /* ENERGY, expressed in the aim point rather than as a mode of its own: below the minimum manoeuvring
   * speed the fight does not go uphill any more — the aim point is dropped to the horizon so height stops
   * being bought with speed the jet no longer has. Deliberately a CLAMP and not a nose-down bias: a
   * negative elevation demand rolls the lift vector INVERTED (this law points the LIFT axis at the aim
   * point, and "below" means 180 deg of roll), which is a split-S, not an unload — measured, and it
   * dumped the jet 2,900 m while the pilot was only trying to regain 50 kt. */
  if (lowEnergy && aimU > 0.0) aimU = 0.0;

  double azErr = 0.0, elErr = 0.0;
  FBEnuToBodyLos(st.roll, st.pitch, st.yaw, aimE, aimN, aimU, azErr, elErr);

  /* ---- 3b. GUN TRACKING: inside the funnel, fly the funnel ----
   * Pursuit steering aims the nose at a POSITION — where he is, where he will be in a couple of
   * seconds, or a point behind him. None of those is where a gun has to point: a 20 mm round takes a
   * third of a second to cross 300 m and the target moves a wingspan's worth of angle in that time, so
   * pure pursuit inside gun range is a guaranteed miss (measured: with the default law the aim error at
   * the control position runs to ~9 deg against a funnel tolerance of ~1 deg, and the trigger never
   * comes down).
   *
   * So once the target is inside the funnel's own range window — the guide's own in-range test, which
   * is that it no longer appears smaller than the funnel's wide end (doc/f16/weapons.md §2.5) — the
   * steering error becomes the LEAD error the fire control publishes: the angle between where the gun
   * has to point and where the nose is. Flying that to zero is exactly what a pilot does with the
   * funnel, and it is the same solution the trigger gate reads, so aiming and shooting cannot disagree.
   * The pilot computes nothing here: it reads one number off the bus, like every other instrument. */
  const FBFireControlBlock &fc = state.FireControl;
  /* THREE conditions, and the last two are what keep this from being a worse pursuit law than the one
   * above. In range (the guide's own test) is not enough: a gun solution exists for a target ANYWHERE,
   * including one that has just gone past the wing, and its required bore is then 170 degrees off the
   * nose. Handing that to the lift-vector law as a steering error produces a violent, energy-destroying
   * reversal — measured, it flew the jet into the ground in 158 s. So the funnel is flown only when the
   * target is in front (inside the same angle-off the control position is defined by) AND the lead the
   * solution asks for is already a TRACKING correction rather than a turn: past that, the pursuit law
   * above is what gets the nose there, which is its job. */
  bool gunTrack = state.Gun.H.Readable() && state.Gun.Ready && fc.H.Readable() && fc.GunValid &&
                  fc.GunSpanMr >= fc.GunFunnelBottomMr &&
                  std::fabs(g.AzDeg) <= BfmControlAtaDeg() &&
                  fc.GunAimErrorDeg <= BfmGunTrackMaxErrDeg();
  if (gunTrack) {
    /* ---- 3c. THE TRACKING LOOP: the solution's own MOTION is a control term of its own ----
     * The law below regulates a steering ERROR: it commands a turn rate of err/kBfmTurnTimeS. Against a
     * target that is turning, the required bore is not a point but a RAMP — it sweeps through the funnel
     * — and a loop that answers a ramp with proportional gain alone settles at a constant lag of exactly
     * (ramp rate) x (its own time constant). Measured against a defender in a max-rate break: the
     * solution moves ~1 deg/s, the time constant is 2 s, and the aiming error never got below 4.6 deg
     * against a funnel tolerance near 1 deg — two squeezes, seventy rounds, no hits. That is not a
     * gunnery problem, it is a control-type problem, and the fix is to give the loop the rate.
     *
     * WHAT RATE, exactly — and this is where the obvious implementation is the wrong one. The published
     * lead is BODY-referenced, so differencing it measures the jet's own pull as much as the target's
     * motion, and in steady state (which is precisely the lag being removed) that derivative is nearly
     * zero while the solution is sweeping through the sky as fast as ever. Subtracting own rate gyros
     * back out is arithmetic on top of a quantity that already mixed them. So the rate is taken where
     * own attitude does not appear at all: the required bore is rotated into the WORLD, differenced
     * there, and the result is where the bore will point kBfmTurnTimeS from now, rotated back into the
     * body. Nothing about own roll, pitch or yaw survives that round trip, which is the point.
     *
     * Feeding THAT as the steering error hands the existing law a turn command of err/T + w: the
     * proportional term still takes the error out, and the rate term is what holds the nose on a
     * solution that will not stand still. Bounded by what the jet can actually turn
     * (CornerTurnRateDegS): a solution sweeping faster than that is not a tracking problem any more, and
     * asking for it would only throw the lift vector at a point the airframe cannot reach. */
    double az = fc.GunLeadAzDeg, el = fc.GunLeadElDeg;
    double be, bn, bu;
    FBBodyLosToEnu(st.roll, st.pitch, st.yaw, az, el, be, bn, bu);
    if (GunHaveLead_ && TimeS_ > GunLeadPrevS_) {
      double dtl = TimeS_ - GunLeadPrevS_;
      GunLeadRateE_ += kBfmLeadRateAlpha * ((be - GunLeadPrevE_) / dtl - GunLeadRateE_);
      GunLeadRateN_ += kBfmLeadRateAlpha * ((bn - GunLeadPrevN_) / dtl - GunLeadRateN_);
      GunLeadRateU_ += kBfmLeadRateAlpha * ((bu - GunLeadPrevU_) / dtl - GunLeadRateU_);
    }
    GunLeadPrevE_ = be; GunLeadPrevN_ = bn; GunLeadPrevU_ = bu;
    GunLeadPrevS_ = TimeS_; GunHaveLead_ = true;

    /* The lead's own angular step over one time constant, capped at the airframe's turn rate: a unit
     * direction's rate has magnitude equal to its angular rate, so the cap is one scalar. */
    double rate = std::sqrt(GunLeadRateE_ * GunLeadRateE_ + GunLeadRateN_ * GunLeadRateN_ +
                            GunLeadRateU_ * GunLeadRateU_);
    double rateMax = CornerTurnRateDegS() * kDeg2Rad;
    double k = kBfmTurnTimeS * (rate > rateMax ? rateMax / std::fmax(rate, 1e-9) : 1.0);
    FBEnuToBodyLos(st.roll, st.pitch, st.yaw, be + k * GunLeadRateE_, bn + k * GunLeadRateN_,
                   bu + k * GunLeadRateU_, azErr, elErr);
    GunTrackIAz_ = Clamp(GunTrackIAz_ + kBfmTrackKi * az * dt, -kBfmTrackIMaxDeg, kBfmTrackIMaxDeg);
    GunTrackIEl_ = Clamp(GunTrackIEl_ + kBfmTrackKi * el * dt, -kBfmTrackIMaxDeg, kBfmTrackIMaxDeg);
    azErr += GunTrackIAz_;
    elErr += GunTrackIEl_;
    /* AND IT MAY NEVER ASK FOR MORE THAN THE GATE THAT LET IT IN. The three terms above can add up to a
     * demand far larger than the aiming error itself — rate and integral are both angles here — and this
     * law is only entitled to the TRACKING problem: BfmGunTrackMaxErrDeg is the boundary past which the
     * pursuit law owns the geometry, so it bounds what the funnel law may demand as well. Without it the
     * combined demand reached sixty degrees at point-blank range and the jet answered it the way it
     * answers any sixty-degree demand — full stick, both axes — and departed (measured: an LOC K.O. at
     * 150 deg/s of roll rate). */
    double mag = std::sqrt(azErr * azErr + elErr * elErr);
    double lim = BfmGunTrackMaxErrDeg();
    if (mag > lim) { azErr *= lim / mag; elErr *= lim / mag; }
    mode = FBBfmPursuit::Lead;   /* it IS lead pursuit, and the scoreboard should say so */
  } else {
    GunHaveLead_ = false;
    GunLeadRateE_ = GunLeadRateN_ = GunLeadRateU_ = 0.0;
    GunTrackIAz_ = GunTrackIEl_ = 0.0;
  }
  if (gunTrack != GunTracking_) {
    GunTracking_ = gunTrack;
    FBLog::Info("pilot", gunTrack ? "GUN_TRACK" : "GUN_BREAK",
                {{"rangeM", fc.GunRangeM}, {"tofS", fc.GunTofS}, {"aimErrDeg", fc.GunAimErrorDeg},
                 {"leadAzDeg", fc.GunLeadAzDeg}, {"leadElDeg", fc.GunLeadElDeg},
                 {"spanMr", fc.GunSpanMr}, {"bottomMr", fc.GunFunnelBottomMr},
                 {"rounds", state.Gun.RoundsRemaining}});
  }

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
  double rollCmd = Clamp(phiCmd / kBfmRollFullDeg, -1.0, 1.0);
  if (std::fabs(st.p) > kBfmRollRateMaxDegS && st.p * rollCmd > 0.0 && BfmRollCmdPrev_ != 0.0) {
    /* Scale the command THAT PRODUCED the measured rate, not the raw one: on a rate-command stick the
     * two are proportional, so cmd_prev * cap/rate is the command that would have produced the cap, and
     * the fixed point of that recursion is the cap itself. Scaling the raw (saturated) command instead
     * settles on the geometric mean of the cap and the airframe's full-stick rate — measured, a 45 deg/s
     * governor held 85 deg/s and the jet rolled right through 360 degrees. */
    double lim = std::fabs(BfmRollCmdPrev_) * kBfmRollRateMaxDegS / std::fabs(st.p);
    if (std::fabs(rollCmd) > lim) rollCmd = rollCmd > 0.0 ? lim : -lim;
  }
  BfmRollCmdPrev_ = rollCmd;
  c.ManualRoll = rollCmd;

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

  bool inControl = validTrack && g.Locked && rngNm >= ctrlMinNm && rngNm <= ctrlMaxNm &&
                   g.AspectDeg <= BfmControlAspectDeg() && std::fabs(g.AzDeg) <= BfmControlAtaDeg();
  Bfm_.Report(mode, inControl, gCmd, dt);
  BfmGunfire(state, avionics);
  return c;
}

/* THE SHOT. Everything a gun engagement needs to decide has already been decided by the box that owns
 * the geometry (modules/f16/FBF16FireControl's EEGS solution): the target is inside the funnel's range
 * window and the nose is inside the lead solution's own tolerance. So this is not a second aiming
 * computation — it is the finger, and it does exactly three things a finger does: check the gun is
 * armed and loaded (the jet's own answer, off the bus), squeeze for a fixed burst, and not squeeze again
 * before that burst is over.
 *
 * IT NEVER CHECKS WHO IT IS SHOOTING AT, and it cannot: the pilot sees a radar track, not a roster
 * (FBPilot::Run's banner). A fight it was sent into is a fight it shoots in — the mission declares the
 * cast, and the trigger answers the funnel. */
void FBPilot::BfmGunfire(const FBState &state, FBCommandBus &avionics) {
  if (!state.Gun.H.Readable() || !state.Gun.Ready) return;
  const FBFireControlBlock &fc = state.FireControl;
  if (!fc.H.Readable() || !fc.GunValid) { GunHaveErr_ = false; return; }

  /* THE TRIGGER TAKES A FINGER'S TIME. Every HOTAS action on this bus arrives one press-duration later
   * (core/FBCommandBus::LatencyS — a finger's own actuation time for the trigger), and even a tenth of
   * a second matters in a gun engagement: at a fighter's tracking rates the aiming error moves about two
   * degrees a second, which at 300 m is ten metres of miss per second of delay (measured — bursts
   * commanded on a 0.35 deg solution arrived on a 1.7 deg one and passed 8 m clear). So the pilot
   * squeezes for where the pipper WILL be: the error's own rate of change, differenced across two
   * decision ticks, extrapolated over exactly the latency the bus will impose plus the round's own time
   * of flight, which the fire control publishes. Leading the pipper is not a trick — it is what makes a
   * burst arrive on a moving solution. */
  double predErrDeg = fc.GunAimErrorDeg;
  if (GunHaveErr_ && TimeS_ > GunPrevS_) {
    double rate = (fc.GunAimErrorDeg - GunPrevErrDeg_) / (TimeS_ - GunPrevS_);
    predErrDeg = fc.GunAimErrorDeg +
                 rate * (FBCommandBus::LatencyS(FBCommandTarget::GunTrigger) + fc.GunTofS);
    if (predErrDeg < 0.0) predErrDeg = 0.0;
  }
  GunPrevErrDeg_ = fc.GunAimErrorDeg;
  GunPrevS_ = TimeS_;
  GunHaveErr_ = true;
  if (!fc.GunInRange) return;
  /* THE FUNNEL IS THE SIGHT'S TOLERANCE, NOT THE PILOT'S. Its walls are the target's WINGSPAN, so a
   * solution just inside it puts the pattern half a span from his centre — on his wingtip if the aim is
   * perfect and past it if the tracking is drifting (measured: bursts fired at the funnel's own edge
   * passed 8 m clear). A pilot who is trying to kill rather than to qualify holds the pipper tighter
   * than that, and this is the number that says how much tighter. */
  if (predErrDeg > Tuned(FBPilotParam::GunFireTolFrac, BfmGunFireTolFrac()) * fc.GunTolDeg) return;
  if (TimeS_ < GunNextS_) return;
  double burstS = Tuned(FBPilotParam::GunBurstS, BfmGunBurstS());
  if (avionics.Post(FBCommandTarget::GunTrigger, burstS, TimeS_).Outcome == FBCommandOutcome::Rejected)
    return;
  GunNextS_ = TimeS_ + burstS;
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

/* ONE cockpit action, at most, per decision tick (see kInterceptActionS), in the order a pilot would
 * take them: the aircraft being shot at throws chaff before it worries about a radar mode. Everything
 * goes through the same bus a hand would drive; nothing here reaches a box directly. */
bool FBPilot::InterceptCockpit(const FBState &state, FBCommandBus &avionics, int designateTrack,
                               bool wantShot, bool wantChaff, double wantElDeg) {
  if (TimeS_ - IntLastActionS_ < Tuned(FBPilotParam::ActionSpacingS, kInterceptActionS)) return false;
  auto post = [&](FBCommandTarget t, double v) {
    IntLastActionS_ = TimeS_;
    avionics.Post(t, v, TimeS_);
  };
  if (wantChaff) {
    post(FBCommandTarget::CmDispense, 0.0);   /* CMS forward: the program the PRGM knob selects */
    IntLastChaffS_ = TimeS_;
    return true;
  }
  if (wantShot) {
    post(FBCommandTarget::WeaponRelease, 1.0);
    IntLastShotS_ = TimeS_;
    return true;
  }
  /* TMS forward on the contact the pilot picked, or TMS aft (0) to let one go. Only when the set is not
   * already holding the lock that was asked for — a designation is a decision, not a repeated demand. */
  bool locked = state.Radar.H.Readable() && state.Radar.LockIndex >= 0;
  if (designateTrack > 0 && !locked) { post(FBCommandTarget::Designate, designateTrack); return true; }
  if (designateTrack < 0 && locked) { post(FBCommandTarget::Designate, 0.0); return true; }
  /* The antenna elevation control: the search pattern is pointed at the altitude band the target is
   * expected in. Deadbanded, because a knob that is nudged every tenth of a second is not being flown. */
  if (std::fabs(wantElDeg - IntCmdElDeg_) > kInterceptElDeadDeg) {
    IntCmdElDeg_ = wantElDeg;
    post(FBCommandTarget::RadarSlewEl, wantElDeg);
    return true;
  }
  /* ...and, once, the search mode itself: this phase is flown in a mode that finds everything and locks
   * nothing. A module without such a mode (ordinal < 0) has its set left exactly as the mission set it. */
  int searchMode = SearchRadarModeOrdinal();
  if (!IntCmdMode_ && searchMode >= 0 && state.Radar.H.Readable() &&
      state.Radar.ModeOrdinal != searchMode) {
    IntCmdMode_ = true;
    post(FBCommandTarget::RadarMode, searchMode);
    return true;
  }
  return false;
}

/* THE HONEST HALF OF THE RE-ATTACK DECISION (header). Three instruments, three reasons to go home, and
 * every one of them read off the bus rather than known: a jet with empty racks cannot kill him, a jet at
 * bingo cannot get home afterwards, and a jet whose set is not radiating cannot even find him. The
 * warning block is what carries the fuel judgement, because BINGO is a number the PILOT committed to
 * (systems/FBWarningSystem against the briefed threshold), not a fraction this class gets to invent. */
bool FBPilot::CanPressOn(const FBState &state) const {
  bool weapons = state.Stores.H.Readable() && state.Stores.LoadedCount > 0;
  bool bingo = state.Warnings.H.Readable() && (state.Warnings.Active & FBWarnBingo) != 0;
  bool sensor = state.Radar.H.Readable() && state.Radar.Radiating;
  return weapons && !bingo && sensor;
}

/* ONE decision tick of an intercept. The order below is the order a pilot's attention runs in: what can
 * I see, who can see me, what state does that put me in, where do I point the jet, and only then which
 * switch do I touch.
 *
 * WHAT IT MAY SEE (CLAUDE.md "Kein Cheaten"): the radar block (anonymous contacts + the lock), the RWR
 * block (bearings and what class of emission they are), the fire-control block (the launch zone), the
 * stores block (what is left on the racks and what has left them) — all of it written by simulated
 * boxes, none of it truth. The engaged target's position is derived from the radar's own reported
 * bearing/elevation/range and nothing else, exactly as the BFM tracker's is. */
FBPilotCommands FBPilot::InterceptCommands(const FBState &state, FBCommandBus &avionics,
                                           const fb_fdm_state &st, const FBFlightPlan &plan, double dt) {
  /* The locked contact's own fusion, for the quantities a single echo cannot give: the target's
   * velocity, and from it the aspect the shot decision is taken on. Unlocked it simply coasts, which is
   * the correct answer to "where is he going" when nobody is tracking him. */
  Bfm_.Update(state, st, TimeS_);
  const FBBfmBlock &fused = Bfm_.Block();
  /* ...and what is left of it once nobody is tracking him any more: the place to go back to and how big
   * the doubt has grown (systems/FBBfmTrack's FBTrackDatum). Invalid until something has actually been
   * locked, which is what keeps every intercept that never made contact on its briefed vector. */
  const FBTrackDatum datum = Bfm_.Datum(st, TimeS_, CornerTurnRateDegS());

  /* ---- 1. the picture: which return is being worked ---- */
  const FBRadarBlock &fcr = state.Radar;
  bool radarUp = fcr.H.Readable();
  bool locked = radarUp && fcr.LockIndex >= 0 && fcr.LockIndex < fcr.ContactCount;
  const FBRadarContact *tgt = nullptr;
  if (locked) {
    tgt = &fcr.Contacts[fcr.LockIndex];
  } else if (radarUp) {
    /* Nearest return that has not identified itself as a friend. A valid Mode-4 reply PROVES friendly
     * and is the one thing that takes a contact off the list; silence proves nothing and stays a
     * candidate (core/FBRadarContact.h) — which is exactly the identification problem, not a shortcut
     * around it. */
    for (int i = 0; i < fcr.ContactCount; i++) {
      const FBRadarContact &c = fcr.Contacts[i];
      if (c.Iff == FBIffReply::Friendly) continue;
      if (!tgt || c.RangeM < tgt->RangeM) tgt = &c;
    }
  }
  if (locked) { if (IntLockSinceS_ < 0.0) IntLockSinceS_ = TimeS_; } else { IntLockSinceS_ = -1.0; }
  bool haveTgt = tgt != nullptr && tgt->LookAgeS < kInterceptLostS;
  if (haveTgt) {
    if (IntTrack_ != tgt->TrackNum) IntTrack_ = tgt->TrackNum;
    Eng_.NoteContact(TimeS_);
    if (locked) Eng_.NoteLock(TimeS_);
  } else {
    IntTrack_ = 0;
  }

  double tgtRangeM = haveTgt ? tgt->RangeM : 0.0;
  double tgtAzDeg = haveTgt ? tgt->AzDeg : 0.0;
  double tgtElDeg = haveTgt ? tgt->ElDeg : 0.0;
  double tgtBrgDeg = haveTgt ? tgt->BearingDeg : st.yaw;
  double tgtAltM = haveTgt ? st.elev + tgtRangeM * std::sin(tgt->ElevAngleDeg * kDeg2Rad) : st.elev;
  double tgtClosMs = haveTgt ? tgt->ClosureMs : 0.0;
  double aspectDeg = (locked && fused.H.IsValid()) ? fused.AspectDeg : -1.0;

  /* ---- 2. who can see me: the warning receiver, and what it demands ---- */
  const FBRwrBlock &rwr = state.Rwr;
  bool threatTrack = false, threatMissile = rwr.H.Readable() && rwr.Powered && rwr.MissileLaunch;
  double threatBrgDeg = 0.0;
  if (rwr.H.Readable() && rwr.Powered) {
    for (int i = 0; i < rwr.ThreatCount; i++) {
      const FBRwrThreat &t = rwr.Threats[i];
      if (t.Mode == FBRwrThreatMode::Search) continue;
      /* The MISSILE symbol outranks a TRACK symbol whatever the scope's own priority says: one of them
       * is a radar that might shoot, the other is a seeker that already has. */
      bool better = !threatTrack || t.Mode == FBRwrThreatMode::Missile;
      if (!better) continue;
      threatTrack = true;
      threatBrgDeg = t.BearingDeg;
      if (t.Mode == FBRwrThreatMode::Missile) threatMissile = true;
    }
  }
  if (threatTrack) Eng_.NoteThreat(TimeS_);

  /* WHEN A WARNING IS WORTH TURNING FOR. A seeker on the aircraft is never negotiable. A radar merely
   * tracking it is: turning away from a lock spike before taking your own shot loses the engagement,
   * and the whole reason a fighter accepts being tracked is that it is about to shoot back. So a track
   * spike demands an answer only once this jet's own attack has nothing left to gain — the shot is away
   * and no longer needs supporting, or there was never one to take. */
  bool weapons = state.Stores.H.Readable() && state.Stores.LoadedCount > 0;
  bool shotSelfSufficient = Eng_.HaveShot() && (Eng_.Pitbull() || !locked);
  bool mustDefend = threatMissile || (threatTrack && (!weapons || shotSelfSufficient));
  if (mustDefend) {
    if (IntDefendCueS_ < 0.0) IntDefendCueS_ = TimeS_;
    IntThreatLastS_ = TimeS_;
  } else {
    IntDefendCueS_ = -1.0;
  }
  bool defendDue = mustDefend &&
                   TimeS_ - IntDefendCueS_ >= Tuned(FBPilotParam::ReactionS, kInterceptReactionS);

  /* ---- 3. the brief: the vector flown while there is nothing on the scope ----
   * The mission's active waypoint IS the vector — a controller's "threat bearing 090, thirty miles,
   * angels 25" is a point and an altitude, and that is exactly what a waypoint carries. With none
   * declared the pilot holds what it was spawned on, anchored once so the search does not chase its own
   * turn (the same failure the BFM cold search had). */
  if (const FBWaypoint *wp = plan.ActiveWaypoint()) {
    IntBriefHdgDeg_ = FBBearingDeg(st.lat, st.lon, wp->LatDeg, wp->LonDeg);
    IntBriefAltM_ = wp->AltM;
    IntAnchored_ = true;
  } else if (!IntAnchored_) {
    IntBriefHdgDeg_ = st.yaw;
    IntBriefAltM_ = st.elev;
    IntAnchored_ = true;
  }

  /* ---- 4. the shot: has one left the jet, and is one available ---- */
  const FBFireControlBlock &fc = state.FireControl;
  bool zone = fc.H.Readable() && fc.DlzValid;
  double shotRangeM = zone ? fc.TargetRangeM : 0.0;
  bool inParams = zone && fc.InZone && shotRangeM <= fc.RtrM * Tuned(FBPilotParam::ShotRtrFactor, InterceptShotRtrFactor()) &&
                  std::fabs(tgtAzDeg) <= Tuned(FBPilotParam::ShotAtaDeg, InterceptShotAtaDeg());
  int released = state.Stores.H.Readable() ? state.Stores.ReleasedCount : IntSeenReleases_;
  if (released > IntSeenReleases_) {
    IntSeenReleases_ = released;
    Eng_.NoteShot(TimeS_, shotRangeM, tgtAzDeg, aspectDeg, fc.RaeroM, fc.RtrM, fc.RminM, fc.TimeToActiveS,
                  fc.TimeToImpactS);
    /* WHEN A SECOND ROUND IS EVEN A QUESTION. Not before the first one has had its chance: the fire
     * control predicted a time of flight, and re-attacking a target that a round is still on its way to
     * is spending a missile on an answer nobody has yet. Since the damage model exists
     * (core/FBDamageModel) the answer usually arrives before the wait is over, and a re-attack is what
     * happens when it did not — which is exactly the decision this rule was written for. The pilot
     * still learns nothing about the hit except through its own sensors: a destroyed jet stops being a
     * radar contact because it falls, not because anything told this pilot it was dead. */
    IntNextShotS_ = TimeS_ + std::fmax(Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS()),
                                       fc.TimeToImpactS > 0.0f ? (double)fc.TimeToImpactS : 0.0);
    IntHaveCrankSign_ = false;
    EngState_ = FBEngageState::Support;
  }
  int chaffOut = state.Cmds.H.Readable() ? state.Cmds.ChaffDispensed : IntSeenChaff_;
  if (chaffOut > IntSeenChaff_) { Eng_.NoteChaff(chaffOut - IntSeenChaff_); IntSeenChaff_ = chaffOut; }

  /* ---- 5. the state machine (systems/FBEngagement's banner has the states) ---- */
  if (defendDue) {
    EngState_ = FBEngageState::Defend;
  } else if (EngState_ == FBEngageState::Defend && TimeS_ - IntThreatLastS_ >= Tuned(FBPilotParam::DefendHoldS, InterceptDefendHoldS())) {
    /* THE RE-ATTACK DECISION. The threat has stopped demanding an answer; whether that was the notch
     * working or the shooter going away is not something this aircraft can know. What it can answer is
     * whether it still has anything to come back WITH — a weapon, the fuel to get home after using it,
     * and a set that can find him again (CanPressOn). Search is then not "fly the brief" any more but
     * "go back to where he was", because the datum below is what the search steers at. */
    EngState_ = CanPressOn(state) ? FBEngageState::Search : FBEngageState::Abort;
  } else if (EngState_ == FBEngageState::Support) {
    /* HOW LONG THE CRANK IS HELD. Not "until the seeker takes over": that is when the round stops
     * needing the UPLINK, not when the shot is over. Between the seeker going active and the round
     * arriving there is nothing this jet can do for it and every reason not to be flying at the target
     * meanwhile, so the crank is held until the fire control's own predicted time of flight has run —
     * shoot, crank, and only then decide again. The cap catches a launch zone that never produced a
     * countdown at all (TimeToImpactS < 0: the round dies before it gets there).
     *
     * The other ending is the failure one: the lock is gone and the seeker has NOT taken over, so the
     * round is flying an estimate nobody is refreshing. That is worth going back for — re-designating
     * resumes the uplink (systems/FBStoresSystem radiates while the fire control has a target), which
     * is the only thing that can still save the shot. */
    double sinceShotS = TimeS_ - Eng_.ShotS();
    double holdS = std::fmin(kInterceptSupportMaxS,
                             std::fmax(Eng_.ShotTtiS(), std::fmax(Eng_.ShotTtaS(), 0.0)));
    if (sinceShotS >= holdS || (!locked && !Eng_.Pitbull()))
      EngState_ = weapons && haveTgt ? FBEngageState::Attack : FBEngageState::Abort;
  } else if (EngState_ != FBEngageState::Abort) {
    /* NOTHING ON THE SCOPE OUTRANKS EVERYTHING ELSE: an aircraft with nothing to shoot at flies its
     * brief and searches, whatever is or is not left on its racks — a jet out of missiles still has a
     * job, and an empty-rack abort taken before anything was ever seen would just be a jet leaving. */
    if (!haveTgt) EngState_ = FBEngageState::Search;
    else if (!weapons) EngState_ = FBEngageState::Abort;
    else if (tgtRangeM * kMToNm < Tuned(FBPilotParam::AbortRangeNm, InterceptAbortRangeNm()) && !Eng_.HaveShot())
      EngState_ = FBEngageState::Abort;   /* inside visual range with nothing fired: this is not an
                                           * intercept any more, and pressing on is a merge */
    else if (tgtRangeM * kMToNm <= Tuned(FBPilotParam::LockRangeNm, InterceptLockRangeNm())) EngState_ = FBEngageState::Attack;
    else EngState_ = FBEngageState::Closing;
  }

  /* ---- 6. where the jet is pointed ---- */
  /* The weave runs only while this phase is actually searching, and its phase zero is the moment the
   * search started (SearchWeaveDeg) — asked for here rather than inside the case so that leaving Search
   * for any reason ends the pattern instead of leaving it half-swept for the next one. */
  bool searching = EngState_ == FBEngageState::Search || EngState_ == FBEngageState::Idle;
  double searchWeaveDeg = SearchWeaveDeg(datum, searching);

  FBPilotCommands c{};
  c.Guidance = FBPilotGuidance::Direct;
  c.TargetSpeedKt = Tuned(FBPilotParam::InterceptSpeedKt, InterceptSpeedKt());
  c.TargetAltM = IntBriefAltM_;
  double aimHdgDeg = IntBriefHdgDeg_;
  double wantElDeg = 0.0;
  int designate = 0;
  bool wantShot = false, wantChaff = false;

  switch (EngState_) {
    case FBEngageState::Idle:
    case FBEngageState::Search: {
      /* Point the antenna at the altitude band the brief put this aircraft in — a controller vectors a
       * fighter to the height the trade is expected at, so co-altitude is the search's own assumption
       * until a return says otherwise. Own PITCH is what makes this a command rather than a constant:
       * the pattern is bolted to the nose, so a climbing jet's radar looks up and out of the band it is
       * supposed to be sweeping unless the antenna is pushed back down by exactly that angle. */
      double bandAltM = IntBriefAltM_;
      double distM = std::fmax(kInterceptAimM * 0.25, 1000.0);
      /* ...UNLESS THIS AIRCRAFT HAS ALREADY SEEN HIM. Then the brief is out of date and the datum is
       * not: a fighter that broke off to defend and survived does not resume a controller's vector as
       * if nothing had happened, it goes back to the last place it knew he was. The heading, the
       * altitude band and the antenna all come off the same datum, and the weave widens to whatever the
       * uncertainty has grown to (SearchWeaveDeg). With nothing ever seen the datum is invalid and every
       * number below is the briefed one, byte for byte — which is why an intercept that never made
       * contact is unaffected by any of this. */
      if (datum.Valid) {
        bandAltM = datum.AltM;
        distM = std::fmax(std::sqrt(datum.EastM * datum.EastM + datum.NorthM * datum.NorthM), 1000.0);
        aimHdgDeg = datum.BearingDeg + searchWeaveDeg;
        c.TargetAltM = datum.AltM;
      }
      wantElDeg = std::atan2(bandAltM - st.elev, distM) * kRad2Deg - st.pitch;
      break;
    }
    case FBEngageState::Closing:
    case FBEngageState::Attack: {
      /* Pure pursuit at the contact, co-altitude with it. Deliberately NOT a lead course: before the
       * lock this aircraft has an echo and no velocity estimate to lead on, and after it the round is
       * the thing that needs the lead, not the launcher. Flying at him also keeps him in the middle of a
       * search pattern that is only 21 degrees tall. */
      aimHdgDeg = tgtBrgDeg;
      c.TargetAltM = tgtAltM;
      /* Centre the pattern on the return: both the reported contact elevation and the volume's own
       * centre are body-referenced (systems/FBRadarSystem's FBRadarScanVolume), so the wanted antenna
       * position IS the angle the contact came back at. Adding it to the current command instead would
       * walk the beam off the target one look at a time — measured, and it lost a head-on contact
       * twenty seconds after acquiring it. */
      wantElDeg = tgtElDeg;
      if (EngState_ == FBEngageState::Attack) {
        if (!locked) designate = IntTrack_;
        wantShot = locked && inParams && TimeS_ - IntLockSinceS_ >= kInterceptTrackSettleS &&
                   TimeS_ - IntLastShotS_ >= Tuned(FBPilotParam::ShotSpacingS, InterceptShotSpacingS()) && TimeS_ >= IntNextShotS_;
      }
      break;
    }
    case FBEngageState::Support: {
      /* THE CRANK. Turn away until the target sits at the edge of what the antenna can still follow —
       * every degree of that is separation bought without paying for it in guidance, because the uplink
       * needs the LOCK and not the nose. The side is fixed once per shot: a crank that re-picks its
       * direction every tick is a jet flying an S while a round it launched goes unsupported. */
      if (!IntHaveCrankSign_) { IntCrankSign_ = tgtAzDeg >= 0.0 ? 1.0 : -1.0; IntHaveCrankSign_ = true; }
      if (haveTgt) {
        double wantAz = IntCrankSign_ * Tuned(FBPilotParam::CrankAtaDeg, InterceptCrankAtaDeg());
        aimHdgDeg = st.yaw + FBWrap180(tgtAzDeg - wantAz);
        c.TargetAltM = tgtAltM;
        wantElDeg = tgtElDeg;
      } else {
        /* Nothing to hold the crank angle against any more: keep the heading the crank had reached
         * rather than snapping back to a briefed vector that points at where the fight was. */
        aimHdgDeg = st.yaw;
        c.TargetAltM = st.elev;
      }
      break;
    }
    case FBEngageState::Defend: {
      /* THE BEAM. Put the emitter on the 3/9 line: that is where own velocity has no component along
       * his line of sight, which is precisely the clutter filter a pulse-Doppler set rejects, and it is
       * the only geometry in which chaff is worth anything at all (systems/FBRadarSystem's notch). The
       * shorter of the two ways round, because the turn itself is time spent in his radar's best case. */
      double left = FBWrap180(threatBrgDeg + Tuned(FBPilotParam::BeamOffsetDeg, InterceptBeamOffsetDeg()));
      double right = FBWrap180(threatBrgDeg - Tuned(FBPilotParam::BeamOffsetDeg, InterceptBeamOffsetDeg()));
      double turn = std::fabs(right) <= std::fabs(left) ? right : left;
      aimHdgDeg = st.yaw + turn;
      wantChaff = TimeS_ - IntLastChaffS_ >= Tuned(FBPilotParam::ChaffIntervalS, InterceptChaffIntervalS());
      break;
    }
    case FBEngageState::Abort: {
      /* Cold, and stay cold: 180 degrees off the last thing that was pointed at this aircraft. */
      double fromDeg = threatTrack ? st.yaw + threatBrgDeg : (haveTgt ? tgtBrgDeg : IntBriefHdgDeg_);
      aimHdgDeg = fromDeg + 180.0;
      break;
    }
  }

  AimAlongHeading(st, aimHdgDeg, c.TargetLatDeg, c.TargetLonDeg);

  /* ---- 7. the hands ---- */
  bool acted = InterceptCockpit(state, avionics, designate, wantShot, wantChaff, wantElDeg);
  if (acted && (wantChaff || EngState_ == FBEngageState::Defend))
    Eng_.NoteDefensiveAction(TimeS_, IntDefendCueS_ >= 0.0 ? IntDefendCueS_ : IntThreatLastS_);

  Eng_.NoteSupport(locked, TimeS_, dt);
  double esFt = (st.elev + st.speed * st.speed / (2.0 * kBfmG0)) * kMToFt;
  Eng_.Report(EngState_, haveTgt, locked, tgtRangeM, tgtAzDeg, aspectDeg, tgtClosMs, esFt, dt);
  return c;
}

/* ---- THE AIR-TO-GROUND PASS (the Attack phase, see the class banner) ----
 * Three parts and one decision. The RUN-IN is FBAutopilot::Direct to the active waypoint at its own
 * declared altitude and speed — a level laydown, chosen because it is the profile this simulator's
 * guidance flies exactly (see the banner) and because it makes the release moment the only variable
 * under test. The RELEASE is one pickle over the same command bus every other cockpit action goes
 * through, taken on the fire control's published cue and on nothing else: this class solves no
 * ballistics and holds no target position, it reads an instrument (FBFireControlBlock's air-to-ground
 * fields) exactly as it reads the radar altimeter before a flare. The EGRESS is a check turn away with
 * a climb, held for a fixed time, after which the jet is back on its route.
 *
 * WHAT GATES THE PICKLE, in the order a pilot would check it:
 *   a published, valid solution      — no computer, no delivery;
 *   the arming margin is positive    — a release from here would arrive as a dud (the guide's PUAC);
 *   the mode's own cue has passed    — CCRP: time-to-release <= 0. CCIP: the same instant, PLUS the
 *                                      predicted impact point laterally on the target, which is the
 *                                      judgement a pilot makes with the pipper in view and a countdown
 *                                      cannot make at all.
 * The bias shifts only the last of them, by a declared number of seconds (AttackReleaseBiasS). */
FBPilotCommands FBPilot::AttackCommands(const FBState &state, FBCommandBus &avionics,
                                        const fb_fdm_state &st, const FBFlightPlan &plan) {
  FBPilotCommands c{};

  /* ---- the way out, once the store is gone ---- */
  if (AtkReleased_) {
    if (TimeS_ >= AtkEgressUntilS_) { Transition(Phase::Route); return c; }
    c.Guidance = FBPilotGuidance::Direct;
    c.TargetLatDeg = AtkEgressLatDeg_; c.TargetLonDeg = AtkEgressLonDeg_;
    c.TargetAltM = AtkEgressAltM_;
    c.TargetSpeedKt = st.cas * kMsToKt;   /* hold what the run-in left: the turn is the manoeuvre */
    return c;
  }

  const FBWaypoint *wp = plan.ActiveWaypoint();
  if (!wp) { Transition(Phase::Route); return c; }   /* nothing briefed to attack */

  /* ---- the run-in ---- */
  c.Guidance = FBPilotGuidance::Direct;
  c.TargetLatDeg = wp->LatDeg; c.TargetLonDeg = wp->LonDeg;
  c.TargetAltM = wp->AltM;
  c.TargetSpeedKt = wp->SpeedKt;

  const FBFireControlBlock &fc = state.FireControl;
  if (!fc.H.Readable() || !fc.AgValid) return c;
  if (fc.AgArmMarginS <= 0.0) return c;   /* too low to arm — the pass is not made from here */

  /* IN RANGE FIRST, THEN THE CUE. A release cue counts DOWN through zero, so acting on "<= 0" alone
   * would also fire on a solution that has never been positive — a computer that has not yet been given
   * a groundspeed, a target already behind the aircraft. Latching the in-range bit is what a pilot does
   * anyway: you watch the cue come down the steering line before you press anything. */
  if (fc.AgInRange) AtkInRangeSeen_ = true;
  if (!AtkInRangeSeen_) return c;

  /* THE PICKLE IS LED BY ITS OWN ACTUATION DELAY. A command reaches the box that answers it one class
   * latency after the hand moves (core/FBCommandBus), so pressing exactly ON the cue would let the store
   * off the rack half a second late — at 230 m/s, 115 m long, which is more than the whole computation is
   * worth. The real jet has the same problem and solves it the same way round: in CCRP the pilot HOLDS
   * the button and the AIRCRAFT releases when the solution cue passes (doc/f16/weapons.md §2.5), i.e.
   * the intent is issued early and the moment is the computer's. Leading by exactly the channel's own
   * latency is that, expressed in a bus where a command is a discrete event. It is the pilot's knowledge
   * of its own hands, not a peek at anything. */
  double bias = Tuned(FBPilotParam::AttackBiasS, AttackReleaseBiasS());
  double leadS = FBCommandBus::LatencyS(FBCommandTarget::WeaponRelease);
  bool cue = fc.AgTimeToReleaseS <= leadS - bias;
  /* CCIP's extra condition is the LATERAL half of the aiming error and only that. The along-track half
   * is what the cue above is FOR — it is deliberately non-zero at the moment the button is pressed (the
   * round still has to be thrown that far), so testing the combined miss would refuse every correct
   * release and then accept a late one. What "the pipper is on the target" adds over a countdown is the
   * across-track judgement, which is exactly the one a countdown cannot make. */
  if (AtkMode_ == FBDeliveryMode::Ccip)
    cue = cue && std::fabs(fc.AgCrossErrM) <= Tuned(FBPilotParam::AttackCcipTolM, AttackCcipTolM());
  if (!cue) return c;

  /* THE PICKLE. Refused (master arm, an empty rack, a shot-away SMS) is final and the pass is over:
   * a pilot who was told no by the jet does not keep mashing the button — the same rule every other
   * weapon action in this class follows. */
  FBCommandAck r = avionics.Post(FBCommandTarget::WeaponRelease, 1.0, TimeS_);
  FBLog::Info("pilot", "ATTACK_RELEASE", {{"mode", FBDeliveryModeStr(AtkMode_)},
      {"accepted", r.Outcome != FBCommandOutcome::Rejected},
      {"ttrS", (double)fc.AgTimeToReleaseS}, {"leadS", leadS}, {"biasS", bias},
      {"alongErrM", (double)fc.AgAlongErrM}, {"crossErrM", (double)fc.AgCrossErrM},
      {"missM", (double)fc.AgMissM}, {"bombRangeM", (double)fc.AgRangeM},
      {"tofS", (double)fc.AgTofS}, {"armMarginS", (double)fc.AgArmMarginS},
      {"altM", st.elev}, {"gsMs", st.gs}});
  AtkReleased_ = true;
  AtkEgressUntilS_ = TimeS_ + AttackEgressS();

  /* The break turn's aim point, computed once so the leg is a fixed place in the world rather than a
   * heading the guidance would have to re-derive every tick: AttackEgressTurnDeg off the CURRENT ground
   * track, AttackEgressRangeM ahead, AttackEgressClimbM above. Always to the right — a check turn has to
   * go one way, and picking the side from the geometry would be a decision nothing here can source. */
  double trackDeg = state.AirData.H.Readable() ? state.AirData.TrackDeg : st.yaw;
  double hdg = (trackDeg + AttackEgressTurnDeg()) * kDeg2Rad;
  double coslat = std::cos(st.lat * kDeg2Rad);
  double rng = AttackEgressRangeM();
  AtkEgressLatDeg_ = st.lat + rng * std::cos(hdg) / kMPerDeg;
  AtkEgressLonDeg_ = st.lon + (coslat > 1e-6 ? rng * std::sin(hdg) / (kMPerDeg * coslat) : 0.0);
  AtkEgressAltM_ = st.elev + AttackEgressClimbM();
  return c;
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
      return BfmCommands(state, avionics, st, dt);

    case Phase::Intercept:
      return InterceptCommands(state, avionics, st, plan, dt);

    case Phase::Attack:
      return AttackCommands(state, avionics, st, plan);

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
