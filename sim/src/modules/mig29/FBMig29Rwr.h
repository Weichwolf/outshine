/* FlightBox — FBMig29Rwr: the SPO-15LM "Beryoza", an override of sensors/FBRwrSystem.
 *
 * The ALR-56M is a detector plus a threat library plus a priority sorter. The SPO-15 is ENTIRELY
 * ANALOGUE — application-specific circuits, no processor, one azimuth channel processed at a time —
 * and doc/modules/mig29/defence-rwr-cm.md §2.7 lists eleven consequences of that as DEVICE LIMITATIONS.
 * Three of them are behaviour rather than colour, and those three are what this class is:
 *
 *   1. OWN RADAR RADIATING -> THE WHOLE FORWARD HEMISPHERE IS SWITCHED OFF (§2.7 item 11, "the single
 *      most tactically consequential fact in the file"). The device cannot filter its own HPRF. So on
 *      this jet, using the radar costs you the warning that would have told you to stop using it —
 *      and it is the same one bit on both sides of the fence: what the world hears (the published
 *      FBEmitterSignature) is what deafens the receiver in the same fuselage.
 *   2. THE PRIORITY LOGIC HARD-ASSUMES OWN ALTITUDE 26 000-55 000 ft (§2.6). The box has no altitude
 *      input at all. Its two type-priority rows and the azimuth criterion that chooses between them
 *      are therefore evaluated against a CONSTANT — see PriorityRank().
 *   3. TRACK IS A PROPERTY OF THE AZIMUTH CHANNEL, NOT OF AN EVENT (§2.5 + §2.7 item 6): search vs
 *      track is decided by how LONG a channel is illuminated, and once any track is seen in a channel
 *      the whole channel stays marked for the next 2-4 s — so a merely searching radar on the same
 *      bearing is reported as tracking.
 *
 * WHAT IS DELIBERATELY NOT TAKEN OVER, because taking it over would mean inventing: the six threat
 * letters (П/З/Х/Н/F/С). Their assignment is by PULSE WIDTH AND PRF (§2.5) and FlightBox's
 * FBEmitterSignature carries neither — it deliberately has no frequency and no power rating. The same
 * argument the F-16 file makes for the ALIC table. With it fall the CW/HPRF confusion (§2.7 item 8),
 * the 6 dB two-emitter rule (item 4), the multi-sector blooming of a low-frequency emitter (item 2)
 * and the 781 Hz PRF criterion (§2.6 priority 5).
 *
 * Also absent: the elevation lights В/Н (the block already publishes an elevation ANGLE, which is more
 * than the two lamps carry), the SAM WEZ cue (no surface emitter exists), the BIT self-test, and the
 * stock/automatic threat-programme distinction (§2.9) — there is no library to be out of date. */
#ifndef FBMIG29RWR_H
#define FBMIG29RWR_H

#include <cmath>
#include "FBGeodesy.h"
#include "FBRwrSystem.h"

namespace FlightBox::Modules {

class FBMig29Rwr : public Sensors::FBRwrSystem {
public:
  /* [DOC §2.2] 360° azimuth, ±30° elevation — a NARROWER cone than the F-16's ±45°, i.e. a bigger
   * blind zone above and below the fuselage axis, opened by one's own manoeuvring. */
  static constexpr double kElevCoverageDeg = 30.0;

  /* [DOC §2.7 item 11] The blanked sector while the own set radiates. The source says "the forward
   * hemisphere", so the half-width is 90° — read literally, not narrowed to the radar's own scan
   * width: the device is deafened by its own transmitter's SIDE LOBES as much as by its main beam,
   * which is why the blanking is a hemisphere and not a beam. */
  static constexpr double kForwardBlankHalfDeg = 90.0;

  /* [DOC §2.2] Azimuth resolution is a consequence of the ANTENNA PATTERN, not of a channel count:
   * four Luneburg-lens feeds per side forward (20° azimuth beamwidth, 20° peak separation), spiral
   * antennas aft (≥60°). The reported bearing is therefore the CENTRE of the channel that fired — the
   * eight logical channels and what they cost in resolution are tabulated in the .cpp. */

  /* [DOC §2.7 item 6] "once any track is detected, the sector is marked tracking for the next 2-4 s".
   * [SET] 3.0 s, the middle of the documented band. */
  static constexpr double kChannelTrackHoldS = 3.0;
  /* [DOC §2.2] 10 physical, 8 LOGICAL azimuth channels (the LM merges the 50° and 70° feeds). Eight is
   * also kMaxRwrThreats, which is why the detection table never has to be a second number. */
  static constexpr int kAzChannels = 8;

  /* [DOC §2.6] The hard-assumed own-altitude band. It is a CONSTANT because the device has no altitude
   * input — flying at 500 ft it still believes it is at 26-55 kft, and de-prioritises exactly the
   * short-range threats that can reach you. With today's emitter inventory (airborne fire control and
   * missile seekers, both engaging inside the band) nothing is de-prioritised by it; the moment a
   * surface emitter exists, this constant is where the documented defect bites. */
  static constexpr double kAssumedAltLoFt = 26000.0;
  static constexpr double kAssumedAltHiFt = 55000.0;
  /* [DOC §2.6] The azimuth criterion for the two type-priority rows: for types П and F — the 4th-
   * generation HPRF fighter radar among them — ABEAM is the low-priority geometry and everything else
   * is high. "Abeam" as the quadrant around the wingline. */
  static constexpr double kAbeamCenterDeg = 90.0;
  static constexpr double kAbeamHalfDeg = 45.0;
  /* [SET] The engagement ceiling the device credits an emitter class with, in the same units as the
   * assumed own-altitude band above. An airborne fire control radar engages wherever it flies, so it
   * is above the band's floor and stays in the high row; the number exists so that the first surface
   * emitter is a table entry rather than a new mechanism. */
  static constexpr double kAirborneCeilingFt = 60000.0;

  /* Per-tick wiring from the module, plus the one line that turns the blanking into an observable
   * event instead of a silent hole. */
  void Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
           double simTimeS) override;

protected:
  double ElevCoverageDeg() const override { return kElevCoverageDeg; }

  bool Blanked(double rxAzDeg) const override {
    return OwnRadiating() && std::fabs(FBWrap180(rxAzDeg)) <= kForwardBlankHalfDeg;
  }

  double ReportBearingDeg(double rxAzDeg) const override;

  FBRwrThreatMode ClassifyMode(const FBEmitterSignature &sig, FBEmitterKind kind,
                                        double rxAzDeg, double simTimeS) override;

  int PriorityRank(FBRwrThreatMode mode, FBEmitterKind kind, double bearingDeg) const override;

private:
  int ChannelOf(double rxAzDeg) const;

  /* One entry per LOGICAL azimuth channel. `SinceS` is when this channel started being illuminated
   * without a gap; `TrackUntilS` is item 6's channel-wide marking. Fixed array, no allocation. */
  struct Channel {
    double TrackUntilS = -1.0;
  };
  Channel Ch_[kAzChannels]{};
  bool BlankLogged_ = false;
};

} // namespace FlightBox::Modules
#endif
