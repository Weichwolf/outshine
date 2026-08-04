#include "FBMig29Pilot.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBMig29Director.h"
#include "FBUnits.h"
#include <cmath>

namespace FlightBox::Modules {

/* [SET] One entry per this many seconds — the interval between the pilot's hands STARTING two of them,
 * on top of whatever latency the bus charges each one. The same figure the generic brief uses for a
 * retry, and for the same reason: a typed field is a head and a hand, not a 10 Hz loop.
 *
 * WHY THE SPACING CARRIES THE HEAD-DOWN COST AND NOT THE COMMAND CLASS. FBCommandClassOf is a property
 * of the CONTROL, and RadarSlewAz/El is a cursor slew — a thumb — on the F-16, which shares the target.
 * On this jet the same two numbers are a typed range-angle entry, but re-classifying the target would
 * re-classify it for both airframes. So the aircraft-specific part of the cost lives where it belongs:
 * in how long THIS pilot takes between two entries. Only RadarEmission, which no other airframe has,
 * is classified head-down in the bus itself. */
static constexpr double kGciEntrySpacingS = 2.0;

bool FBMig29Pilot::BriefGci(double atS, double brgDeg, double rangeKm, double altKm) {
  if (GciCount_ >= kMaxGciCalls) return false;
  GciCall &g = Gci_[GciCount_++];
  g.AtS = atS;
  g.BrgDeg = brgDeg;
  g.RangeM = rangeKm * 1000.0;   /* the controller speaks kilometres [DCS-FM p.99-100] */
  g.AltM = altKm * 1000.0;
  return true;
}

/* THE GCI LOOP, exactly as documented and in the documented order: aim the antenna where he says, and
 * only then radiate. Three entries per call, one per decision tick:
 *
 *   1. ELEVATION — the range-angle method. The manual's own worked example: own aircraft at 5 km, a
 *      target reported at 80 km and 10 km, "enter the range of 80 km and relative altitude 5 km into
 *      the radar; the radar scan zone would then be correctly aimed". Here that is one arctangent of
 *      the RELATIVE altitude over the reported range, and the pilot's own altitude comes off his own
 *      instrument (the Platform block), never off the controller.
 *   2. AZIMUTH — the ZONE switch, three discrete sectors, chosen from the reported bearing relative to
 *      where the nose is now. The module snaps and reports the clamp.
 *   3. ILLUM — and this is the doctrinal act, not a housekeeping one: on this aircraft the radar is the
 *      thing that gives you away, so it comes on when somebody else has already put you in the right
 *      piece of sky.
 *
 * A call whose entries are still outstanding when the NEXT one falls due is abandoned: the controller
 * has just superseded it, and typing yesterday's numbers is worse than typing none.
 *
 * A REFUSED ENTRY IS RETRIED, and it is the same rule pilot/FBPilot's brief already follows: the bus
 * may turn a head-down input away (manoeuvre lock, channel busy), and a pilot whose hand was pushed
 * back reaches for the switch again. Advancing regardless meant the ONE entry that makes this radar
 * exist could be lost to a single g-loaded tick — measured on `duel-emcon.fbm`, where the commit call
 * fell while the jet was already beaming a threat: ILLUM rejected `sequence_precondition` at t=174.2
 * and the MiG then flew 400 s of the duel blind and never fired. The retry costs nothing when nothing
 * is rejected, so no run in which every entry was accepted moves by a tick. */
Pilot::FBPilotCommands FBMig29Pilot::Run(const FBState &state, FBCommandBus &avionics,
                                         const Systems::FBAirframeControls &airframe,
                                         const Fdm::fb_fdm_state &st, const FBFlightPlan &plan,
                                         const FBRunway *runway, double dt) {
  ClockS_ += dt;

  while (GciNext_ < GciCount_ && ClockS_ >= Gci_[GciNext_].AtS) {
    /* A newer call supersedes an older one that is still being typed. */
    if (GciNext_ + 1 < GciCount_ && ClockS_ >= Gci_[GciNext_ + 1].AtS) {
      if (GciStep_ != 0)
        FBLog::Info("gci", "CALL_SUPERSEDED", {{"atS", Gci_[GciNext_].AtS}, {"step", GciStep_}});
      GciNext_++;
      GciStep_ = 0;
      continue;
    }
    if (GciStep_ >= 3) break;
    if (ClockS_ - GciLastEntryS_ < kGciEntrySpacingS) break;

    const GciCall &g = Gci_[GciNext_];
    bool refused = false;
    if (GciStep_ == 0) {
      /* HEARD, not typed: the emission decision runs on what the controller SAID, and it stands until
       * he says something else — the switch throws that follow are what the pilot does about it. */
      GciHeard_ = GciNext_;
      FBLog::Info("gci", "BRAA", {{"brgDeg", g.BrgDeg}, {"rangeKm", g.RangeM * 0.001},
          {"altKm", g.AltM * 0.001}, {"t", ClockS_}});
      /* No Platform block, no entry: the relative altitude is a DIFFERENCE, and half of it is his own
       * instrument. A pilot who cannot read his own altimeter does not type a scan elevation. */
      if (!state.Platform.H.Readable()) break;
      double relM = g.AltM - (double)state.Platform.AltM;
      /* THE RANGE-ANGLE METHOD PRODUCES A WORLD ANGLE and the knob is BODY-referenced, so the pilot's
       * own pitch attitude is the second half of the entry — exactly as his own uncued search law has
       * always done it, and as the AZIMUTH entry below has always done it with his heading. A climbing
       * interceptor that skipped it aimed the whole of its pitch away from the raid
       * (doc/pilot.md 2.15). */
      double worldElDeg = std::atan2(relM, g.RangeM) * kRad2Deg;
      FBBodyAngle el = FBBodyAngle::FromWorldElevation(worldElDeg, st.pitch);
      refused = avionics.PostAntennaEl(el, ClockS_).Outcome == FBCommandOutcome::Rejected;
      FBLog::Info("gci", "ENTER_ELEV", {{"relM", relM}, {"rangeM", g.RangeM},
          {"worldElDeg", worldElDeg}, {"ownPitchDeg", st.pitch}, {"elDeg", el.Deg()}});
    } else if (GciStep_ == 1) {
      FBBodyAngle az = FBBodyAngle::FromTrueBearing(g.BrgDeg, st.yaw);
      refused = avionics.PostAntennaAz(az, ClockS_).Outcome == FBCommandOutcome::Rejected;
      FBLog::Info("gci", "ENTER_ZONE", {{"brgDeg", g.BrgDeg}, {"ownHdgDeg", st.yaw},
          {"offDeg", az.Deg()}});
    } else {
      refused = avionics.Post(FBCommandTarget::RadarEmission, (double)FBMig29Emission::Illum,
                              ClockS_).Outcome == FBCommandOutcome::Rejected;
      /* No new field for the refusal: the bus already emits CMD_REJECT with the reason, and a retry
       * shows up as a SECOND line of this one. */
      FBLog::Info("gci", "RADAR_ILLUM", {{"t", ClockS_}});
    }
    GciLastEntryS_ = ClockS_;
    if (!refused) {
      GciStep_++;
      if (GciStep_ >= 3) { GciNext_++; GciStep_ = 0; }
    }
    break;   /* AT MOST ONE cockpit action per decision tick, the same rule the generic brief follows */
  }

  Pilot::FBPilotCommands c = Pilot::FBPilot::Run(state, avionics, airframe, st, plan, runway, dt);
  /* AFTER the generic machine, not instead of it: the run-in leg, the flight picture and the briefed
   * items are the same on every airframe. What is this aircraft's own is what happens on that leg —
   * and in the Attack phase the generic pass does nothing at all here, because it waits for a release
   * cue (FBFireControlBlock::AgValid) this computer never publishes. */
  if (GetPhase() == Phase::Attack) DirectorPass(state, avionics, st, plan, c);
  return c;
}

/* "After dropping the bombs, use an energetic maneuver to exit the attack" [DCS-EA p.102].
 *
 * THE EXIT IS THE BRIEFED ONE where the mission declares it, and that is the whole difference from the
 * generic pass. The generic egress invents a point (track + 120 deg, 12 km, +500 m) and then hands the
 * jet back to the Route phase — which on THIS airframe is a crash: the route's active waypoint is still
 * the target the pass never overflew, so Route turns back onto it hard and the aircraft departs
 * (measured: roll 97 deg, r = -323 deg/s, ATTITUDE_CONTACT 12 s into Route). A briefed egress point
 * takes the invention out and the reversal with it. Where the mission declares none, the computed turn
 * is used and the pass simply holds it — this pilot never hands a spent strike back to the router,
 * because it has no way to tell the router that the target waypoint is spent (a stated limit, not a
 * silent one: doc/modules/mig29/weapons.md §5.4.5). */
void FBMig29Pilot::StartBreakoff(const FBState &state, const Fdm::fb_fdm_state &st,
                                 const FBFlightPlan &plan, const char *why) {
  AtkLeaving_ = true;
  AtkLeaveUntilS_ = ClockS_ + AttackEgressS();
  int next = plan.ActiveIndex() + 1;
  if (next < plan.Size()) {
    const FBWaypoint &wp = plan.At(next);
    AtkLeaveLatDeg_ = wp.LatDeg; AtkLeaveLonDeg_ = wp.LonDeg; AtkLeaveAltM_ = wp.AltM;
    AtkLeaveSpeedKt_ = wp.SpeedKt;
  } else {
    double trackDeg = state.AirData.H.Readable() ? state.AirData.TrackDeg : st.yaw;
    double hdg = (trackDeg + AttackEgressTurnDeg()) * kDeg2Rad;
    double coslat = std::cos(st.lat * kDeg2Rad);
    double rng = AttackEgressRangeM();
    AtkLeaveLatDeg_ = st.lat + rng * std::cos(hdg) / kMPerDeg;
    AtkLeaveLonDeg_ = st.lon + (coslat > 1e-6 ? rng * std::sin(hdg) / (kMPerDeg * coslat) : 0.0);
    AtkLeaveAltM_ = st.elev + AttackEgressClimbM();
    AtkLeaveSpeedKt_ = 0.0;
  }
  FBLog::Info("pilot", "ATTACK_BREAKOFF", {{"why", why}, {"briefed", next < plan.Size()},
      {"toLat", AtkLeaveLatDeg_}, {"toLon", AtkLeaveLonDeg_}, {"toAltM", AtkLeaveAltM_},
      {"altM", st.elev}, {"gsMs", st.gs}});
}

void FBMig29Pilot::DirectorPass(const FBState &state, FBCommandBus &avionics,
                                const Fdm::fb_fdm_state &st, const FBFlightPlan &plan,
                                Pilot::FBPilotCommands &c) {
  if (AtkLeaving_) {
    if (!AtkLeftLogged_ && ClockS_ >= AtkLeaveUntilS_) {
      AtkLeftLogged_ = true;
      FBLog::Info("pilot", "ATTACK_PASS_COMPLETE", {{"altM", st.elev}, {"gsMs", st.gs}});
    }
    c = Pilot::FBPilotCommands{};
    c.Guidance = Pilot::FBPilotGuidance::Direct;
    c.TargetLatDeg = AtkLeaveLatDeg_; c.TargetLonDeg = AtkLeaveLonDeg_;
    c.TargetAltM = AtkLeaveAltM_;
    c.TargetSpeedKt = AtkLeaveSpeedKt_ > 0.0 ? AtkLeaveSpeedKt_ : st.cas * kMsToKt;
    return;
  }

  const FBFireControlBlock &fc = state.FireControl;
  if (!fc.H.Readable() || !fc.DirEngaged) return;

  /* THE COUNTDOWN IS NOT THE PILOT'S BUSINESS. Once the ring is up his contract is to hold the run-in,
   * and holding it is what the generic leg above already commands — so this branch does NOTHING, and
   * that is the behaviour, not an omission. It is the exact inverse of the F-16's pass, where every
   * tick between the cue and the pickle is a decision. */
  if (fc.DirArmed) {
    if (AtkReleasedSeen_ < 0 && state.Stores.H.Readable())
      AtkReleasedSeen_ = state.Stores.ReleasedCount;
    return;
  }

  /* The store left: the SMS counter is how a pilot sees it, the same instrument the generic pass uses. */
  if (AtkReleasedSeen_ >= 0 && state.Stores.H.Readable() &&
      state.Stores.ReleasedCount > AtkReleasedSeen_) {
    StartBreakoff(state, st, plan, "released");
    return;
  }

  double gs = st.gs;
  /* THE ONE PIECE OF ARITHMETIC THE COMPUTER REFUSES TO DO. Before the trigger this jet shows a RANGE
   * and where the aiming mark sits; the conversion into a time is the pilot's own, off his own
   * groundspeed (doc/modules/mig29/weapons.md §5.4.3). */
  double markS = gs > 1.0 ? (double)fc.DirMarkErrM / gs : 1e9;

  /* Flown through it: the release point is behind and the source's procedure has no branch for that. */
  if (markS <= 0.0) { StartBreakoff(state, st, plan, "release point passed"); return; }
  if (fc.DirRefusal == (uint8_t)FBDirectorRefusal::NoReleasePoint ||
      fc.DirRefusal == (uint8_t)FBDirectorRefusal::WouldNotArm) {
    StartBreakoff(state, st, plan, FBDirectorRefusalStr((FBDirectorRefusal)fc.DirRefusal));
    return;
  }
  if (AtkConsents_ >= kMaxConsents) { StartBreakoff(state, st, plan, "attempts spent"); return; }
  if (ClockS_ < AtkActNextS_) return;

  /* LOCKON. Fired at the lead the countdown needs, and only there: a rangefinder pressed earlier gives
   * a designation that has aged past the documented 10 s by the time the trigger follows it. */
  if (!fc.DirRanged || fc.DirRangeAgeS > FBMig29Director::kConsentMaxAgeS - kConsentDelayS) {
    if (markS > kDesignateLeadS) return;
    avionics.Post(FBCommandTarget::Designate, 0.0, ClockS_);
    AtkActNextS_ = ClockS_ + kActSpacingS;
    FBLog::Info("pilot", "ATTACK_LOCKON", {{"markErrM", (double)fc.DirMarkErrM}, {"markS", markS},
        {"altM", st.elev}, {"gsMs", gs}});
    return;
  }

  /* THE CONSENT, and it is deliberately LATE: the F-16's pass advances its pickle by the actuation
   * latency, this one WAITS OUT a documented window before pressing at all. The aircraft owns
   * everything after it. */
  if (fc.DirRangeAgeS < kConsentDelayS) return;
  avionics.Post(FBCommandTarget::WeaponRelease, 1.0, ClockS_);
  AtkActNextS_ = ClockS_ + kActSpacingS;
  AtkConsents_++;
  FBLog::Info("pilot", "ATTACK_CONSENT", {{"attempt", AtkConsents_},
      {"rangeAgeS", (double)fc.DirRangeAgeS}, {"rangeM", (double)fc.DirRangeM},
      {"markErrM", (double)fc.DirMarkErrM}, {"markS", markS}, {"altM", st.elev}, {"gsMs", gs}});
}

} // namespace FlightBox::Modules
