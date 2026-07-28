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
 * has just superseded it, and typing yesterday's numbers is worse than typing none. */
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
    if (GciStep_ == 0) {
      FBLog::Info("gci", "BRAA", {{"brgDeg", g.BrgDeg}, {"rangeKm", g.RangeM * 0.001},
          {"altKm", g.AltM * 0.001}, {"t", GciClockS_}});
      /* No Platform block, no entry: the relative altitude is a DIFFERENCE, and half of it is his own
       * instrument. A pilot who cannot read his own altimeter does not type a scan elevation. */
      if (!state.Platform.H.Readable()) break;
      double relM = g.AltM - (double)state.Platform.AltM;
      double elDeg = std::atan2(relM, g.RangeM) * kRad2Deg;
      avionics.Post(FBCommandTarget::RadarSlewEl, elDeg, GciClockS_);
      FBLog::Info("gci", "ENTER_ELEV", {{"relM", relM}, {"rangeM", g.RangeM}, {"elDeg", elDeg}});
    } else if (GciStep_ == 1) {
      double offDeg = FBWrap180(g.BrgDeg - st.yaw);
      avionics.Post(FBCommandTarget::RadarSlewAz, offDeg, GciClockS_);
      FBLog::Info("gci", "ENTER_ZONE", {{"brgDeg", g.BrgDeg}, {"ownHdgDeg", st.yaw},
          {"offDeg", offDeg}});
    } else {
      avionics.Post(FBCommandTarget::RadarEmission, (double)FBMig29Emission::Illum, GciClockS_);
      FBLog::Info("gci", "RADAR_ILLUM", {{"t", GciClockS_}});
    }
    GciStep_++;
    GciLastEntryS_ = GciClockS_;
    if (GciStep_ >= 3) { GciNext_++; GciStep_ = 0; }
    break;   /* AT MOST ONE cockpit action per decision tick, the same rule the generic brief follows */
  }

  return Pilot::FBPilot::Run(state, avionics, airframe, st, plan, runway, dt);
}

} // namespace FlightBox::Modules
