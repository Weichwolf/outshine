#include "FBMig29Rwr.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include <cmath>

namespace FlightBox::Modules {

/* THE EIGHT LOGICAL AZIMUTH CHANNELS. Six forward ones 20° wide (the four Luneburg-lens feeds per side
 * have a 20° azimuth half-power beamwidth and 20° peak separation, §2.2) covering ±60°, and one spiral
 * antenna per side for everything behind that (rear beamwidth "≥60°"). That is the documented "10
 * physical, 8 logical" count with the LM's merged 50°/70° feeds already in it.
 *
 * HONESTLY SHORT OF THE SOURCE: §2.2 states a resolution of 10° inside the forward sector, which the
 * real device reaches through the 10° DESIGNED OVERLAP — an in-between signal lights TWO channels and
 * the midpoint of the pair is the finer reading. FlightBox reports one channel, so the achieved
 * resolution here is the channel width (20° forward, ±60° aft). The double detection is a gap, named
 * in doc/modules/mig29/module.md, not silently smoothed away. */
namespace {
constexpr double kFwdEdgeDeg = 60.0;
constexpr double kFwdWidthDeg = 20.0;
constexpr double kChannelCenterDeg[8] = {-120.0, -50.0, -30.0, -10.0, 10.0, 30.0, 50.0, 120.0};
}

int FBMig29Rwr::ChannelOf(double rxAzDeg) const {
  double a = FBWrap180(rxAzDeg);
  if (a < -kFwdEdgeDeg) return 0;
  if (a > kFwdEdgeDeg) return 7;
  int i = 1 + (int)((a + kFwdEdgeDeg) / kFwdWidthDeg);
  return i < 1 ? 1 : (i > 6 ? 6 : i);
}

/* The CENTRE of the channel that fired — an analogue receiver reports the channel, not an angle it
 * never measured. This is the one place a MiG-29 threat bearing differs from an F-16 one by
 * construction rather than by geometry. */
double FBMig29Rwr::ReportBearingDeg(double rxAzDeg) const {
  return kChannelCenterDeg[ChannelOf(rxAzDeg)];
}

/* ITEM 6 OF THE LIMITATION LIST: "track is a property of the whole azimuth channel, not of an event —
 * once any track is detected, the sector is marked tracking for the next 2-4 s", so a SECOND, merely
 * searching radar on the same bearing is reported as tracking. That is the behaviour built here, and
 * it is the difference to the F-16's receiver: this box answers per CHANNEL, not per emitter.
 *
 * WHAT IS NOT BUILT, and why it would be wrong to: §2.5's actual criterion is the LENGTH of the
 * illumination event (>125-250 ms = track). FlightBox's emission model publishes a searching radar's
 * whole scan volume as one continuously illuminated window — doc/sensors.md's own gap 18: "a target
 * inside the volume is illuminated CONTINUOUSLY instead of pulsed". Timing that would make every
 * search emitter cross the 200 ms threshold within two ticks, i.e. every threat in the sky would read
 * as TRACK and the search filter would mean nothing. Measured exactly that on the first run of this
 * class (mig29-pair, t=0.3: the F-16's CRM sweep reported as track). So the EVENT half of §2.5 waits
 * for a pulsed emission model, and the CHANNEL half — the part that is a device defect rather than a
 * measurement — is what this override contributes. */
FBRwrThreatMode FBMig29Rwr::ClassifyMode(const FBEmitterSignature &sig, FBEmitterKind kind,
                                         double rxAzDeg, double simTimeS) {
  FBRwrThreatMode base = Sensors::FBRwrSystem::ClassifyMode(sig, kind, rxAzDeg, simTimeS);
  int ch = ChannelOf(rxAzDeg);
  Channel &c = Ch_[ch];
  if (base != FBRwrThreatMode::Search) {
    c.TrackUntilS = simTimeS + kChannelTrackHoldS;
    return base;
  }
  /* The channel is still marked from somebody else's beam: this emitter inherits the marking. Reported
   * as Track and NOT as Missile — the marking says "something in this sector was tracking", which is
   * exactly as much as the device knows. */
  return simTimeS <= c.TrackUntilS ? FBRwrThreatMode::Track : FBRwrThreatMode::Search;
}

/* §2.6, as much of it as the emitter model can carry. The two documented type-priority rows are
 * selected by the azimuth criterion; the ALTITUDE half of the criterion is evaluated against the
 * hard-assumed band (which is the defect), and with only airborne emitters in the tree it never
 * changes a row today — the constants are named so the day it does is not a rewrite.
 * Lower = further forward. Rank 0/1 is the row position of the ONE type this simulator can recognise
 * (F = 4th-generation HPRF fighter radar); everything unclassified sorts behind both. */
int FBMig29Rwr::PriorityRank(FBRwrThreatMode mode, FBEmitterKind kind,
                             double bearingDeg) const {
  (void)mode;
  if (kind != FBEmitterKind::AirborneFireControl) return 2;
  /* The engagement ceiling of an airborne fire control radar lies inside the assumed band, so the
   * device keeps it in the HIGH row on altitude grounds — the constant is read here rather than
   * implied, because that read IS the documented defect. */
  const bool reachesAssumedBand = kAirborneCeilingFt >= kAssumedAltLoFt;
  double m = std::fabs(FBWrap180(bearingDeg));
  bool abeam = std::fabs(m - kAbeamCenterDeg) <= kAbeamHalfDeg;
  return (reachesAssumedBand && !abeam) ? 0 : 1;
}

/* The generic Run does all the work; this exists for the one transition worth a line. The generic
 * THREAT_BLIND event says a warning went inaudible, but not that the pilot's own hand caused it. */
void FBMig29Rwr::Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
                     double simTimeS) {
  if (OwnRadiating() != BlankLogged_) {
    BlankLogged_ = OwnRadiating();
    FBLog::Info("spo15", "FORWARD_BLANK", {{"on", BlankLogged_},
        {"halfDeg", kForwardBlankHalfDeg}, {"t", simTimeS}});
  }
  Sensors::FBRwrSystem::Run(state, st, net, simTimeS);
}

} // namespace FlightBox::Modules
