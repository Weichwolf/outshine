#include "FBMig29Pilot.h"
#include "FBGeodesy.h"
#include "FBLog.h"
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
  GciClockS_ += dt;

  while (GciNext_ < GciCount_ && GciClockS_ >= Gci_[GciNext_].AtS) {
    /* A newer call supersedes an older one that is still being typed. */
    if (GciNext_ + 1 < GciCount_ && GciClockS_ >= Gci_[GciNext_ + 1].AtS) {
      if (GciStep_ != 0)
        FBLog::Info("gci", "CALL_SUPERSEDED", {{"atS", Gci_[GciNext_].AtS}, {"step", GciStep_}});
      GciNext_++;
      GciStep_ = 0;
      continue;
    }
    if (GciStep_ >= 3) break;
    if (GciClockS_ - GciLastEntryS_ < kGciEntrySpacingS) break;

    const GciCall &g = Gci_[GciNext_];
    bool refused = false;
    if (GciStep_ == 0) {
      FBLog::Info("gci", "BRAA", {{"brgDeg", g.BrgDeg}, {"rangeKm", g.RangeM * 0.001},
          {"altKm", g.AltM * 0.001}, {"t", GciClockS_}});
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
      refused = avionics.PostAntennaEl(el, GciClockS_).Outcome == FBCommandOutcome::Rejected;
      FBLog::Info("gci", "ENTER_ELEV", {{"relM", relM}, {"rangeM", g.RangeM},
          {"worldElDeg", worldElDeg}, {"ownPitchDeg", st.pitch}, {"elDeg", el.Deg()}});
    } else if (GciStep_ == 1) {
      FBBodyAngle az = FBBodyAngle::FromTrueBearing(g.BrgDeg, st.yaw);
      refused = avionics.PostAntennaAz(az, GciClockS_).Outcome == FBCommandOutcome::Rejected;
      FBLog::Info("gci", "ENTER_ZONE", {{"brgDeg", g.BrgDeg}, {"ownHdgDeg", st.yaw},
          {"offDeg", az.Deg()}});
    } else {
      refused = avionics.Post(FBCommandTarget::RadarEmission, (double)FBMig29Emission::Illum,
                              GciClockS_).Outcome == FBCommandOutcome::Rejected;
      /* No new field for the refusal: the bus already emits CMD_REJECT with the reason, and a retry
       * shows up as a SECOND line of this one. */
      FBLog::Info("gci", "RADAR_ILLUM", {{"t", GciClockS_}});
    }
    GciLastEntryS_ = GciClockS_;
    if (!refused) {
      GciStep_++;
      if (GciStep_ >= 3) { GciNext_++; GciStep_ = 0; }
    }
    break;   /* AT MOST ONE cockpit action per decision tick, the same rule the generic brief follows */
  }

  return Pilot::FBPilot::Run(state, avionics, airframe, st, plan, runway, dt);
}

} // namespace FlightBox::Modules
