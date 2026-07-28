/* FlightBox — FBVisualSystem: the EYE, and the fourth question the other three sensor slots were built
 * around avoiding.
 *
 *   FBRadarSystem  asks "what is out there" and transmits to find out.
 *   FBRwrSystem    asks "who is looking at me" and can only hear.
 *   FBIrstSystem   asks the radar's question and pays nothing.
 *   FBVisualSystem asks "what does it LOOK like" — and pays in every currency there is: the shortest
 *                  reach of the five, no identity and no way to interrogate for one, no range
 *                  measurement EVER, damping by haze AND cloud AND the sun, and it is the only sensor
 *                  in the tree that stops working when the sun goes down.
 *
 * THE PERCEPTION BOUNDARY GROWS BY ONE FILE, TO SIX. The .cpp reads units/FBUnitRegistry, which makes
 * it the sixth file in the tree allowed to and puts it in tools/verify_layers.py's RESTRICTED table by
 * name. That widening is the price of the five above, argued in doc/sensors.md §9.1/§9.2 before a line
 * of this was written: an eye that a PILOT derives from the registry is the cheat the architecture
 * exists to prevent, and an eye bolted onto the radar block would hand the radar an identity.
 *
 * WHAT IT MEASURES — one inequality, three inputs (doc/sensors.md §9.3):
 *
 *     detected  <=>  theta_obs >= theta_detect * g(C_eff)   and the target is inside the cone
 *
 * theta_obs is the presented extent over range (§9.4), theta_detect the 12-arcmin NTSB threshold, and
 * g the contrast penalty out of a chain of documented laws: a two-value inherent contrast (against sky
 * / against ground clutter), Koschmieder haze, a MARCH through core/FBCloudDensity for a real
 * transmittance, the shared daylight factor, and a Stiles-Holladay glare term. Every one of them is
 * closed-form and deterministic — no die anywhere, per doc/conventions.md.
 *
 * NOTHING TO OVERRIDE, DELIBERATELY. Every other slot carries one virtual hook for the airframe whose
 * behaviour genuinely differs; an eye is not a box the aeroplane carries. The canopy that WOULD differ
 * between airframes is named as not modelled (§9.8, there is no cockpit geometry in the tree), and the
 * field of regard is a setting rather than an override. */
#ifndef FBVISUALSYSTEM_H
#define FBVISUALSYSTEM_H

#include "FBCloudDensity.h"
#include "FBState.h"
#include "FBTelemetry.h"
#include "FBUnits.h"
#include "FBVisualContact.h"
#include "FBFdm.h"
#include "FBUnit.h"

namespace FlightBox::Units { class FBUnitRegistry; }

namespace FlightBox::Sensors {

/* Deliberately NOT FBRadarScanVolume: that type carries a RangeM gate, an AutoAcquire and a
 * SingleTarget flag, and an eye has none of the three. Its reach is not a setting at all — it falls out
 * of the geometry — and there is nothing to slew and nothing to lock. */
struct FBVisualFieldOfRegard {
  double AzHalfDeg = 60.0;
  double ElHalfDeg = 40.0;
  double FrameS = 1.0;
};

class FBVisualSystem : public FBTelemetrySource {
public:
  /* ---- THE THRESHOLD [T4]. NTSB (1987, via the ATSB cockpit-visibility study): an aircraft's largest
   * dimension must subtend more than 12 arcmin = 0.2 deg to be acquired. The same study reports 24-36
   * arcmin as more realistic under sub-optimal conditions; FlightBox takes the ONE number and lets the
   * contrast term below carry the degradation, rather than carrying two competing thresholds. */
  static constexpr double kDetectThresholdDeg = 0.2;

  /* ---- RECOGNITION AND IDENTIFICATION [DERIVED, Johnson 1958]. N50 cycles across the critical
   * dimension: detection 1.0, orientation 1.4, recognition 4.0, identification 6.4. The multiples are
   * applied to the CONTRAST-MODULATED threshold, not to the bare one, so the whole ladder degrades
   * together in haze — at reference contrast that is exactly §9.7's worked pair (a beam-on F-16
   * detected at ~4.2 km is recognised at ~1.05 km and identified at ~650 m). */
  static constexpr double kRecogniseCycles = 4.0;
  static constexpr double kIdentifyCycles = 6.4;

  /* ---- CONTRAST. C = |L_tgt - L_bg| / L_bg, and FlightBox has no luminance field: the inherent
   * contrast is a two-value BINARY by background class, decided by the sign of the contact's elevation
   * angle against the horizon dip. Crude, and the right crudeness — it reproduces the one thing every
   * source agrees on (a target against ground clutter is far harder) with one comparison and two
   * settings, and it does not pretend to a terrain-albedo model that does not exist. */
  static constexpr double kContrastLookUp = 1.0;     /* [SET] against sky — and the REFERENCE the
                                                      * threshold above is stated at */
  static constexpr double kContrastLookDown = 0.35;  /* [SET] against terrain clutter */
  /* [SET] Where the linear stand-in stops. The true relation is the contrast sensitivity function (the
   * Spatial Standard Observer's CSF); this inverse is its stand-in, and a tenfold angular penalty is
   * where a stand-in has left the shape it stands in for. It does double duty on purpose: the clamp
   * never bites, because BELOW C_ref/kContrastPenaltyMax the channel reports nothing at all. One
   * constant, two consequences, and no region where the model reports a capability it cannot defend. */
  static constexpr double kContrastPenaltyMax = 10.0;

  /* ---- HAZE [DERIVED, Koschmieder + ISA]. sigma0 = ln(1/0.02)/visibility is the contrast-threshold
   * definition of meteorological visibility; the ISA density scale height thins it with altitude. Both
   * numbers are already in the tree (the cloud stage's aerial perspective) and are not re-derived. */
  static constexpr double kKoschmieder = 3.912;
  static constexpr double kHazeScaleHM = 8000.0;

  /* ---- GLARE [T3, Stiles-Holladay]. L_veil = 10*E/theta^2: intraocular scatter raises the BACKGROUND,
   * and contrast is measured against the background. Neither E nor the ambient luminance exists in
   * absolute units here, so their ratio collapses into ONE setting with a readable meaning —
   *
   *     C_eff = C / (1 + k/theta_sun^2),   k = kGlareHalfAngleDeg^2
   *
   * so kGlareHalfAngleDeg IS the off-sun angle at which contrast is halved. 45 deg -> 0.95, 30 -> 0.90,
   * 10 -> 0.50, 5 -> 0.20, 2 -> 0.04. */
  static constexpr double kGlareHalfAngleDeg = 10.0;
  /* [SET] ...plus a HARD CUT inside a cone about the sun's centre, four solar diameters wide (the disc
   * is 0.53 deg). There the eye is not degraded, it is out of action, and a smooth function through
   * that region would report a faint capability that does not exist. It meets the smooth term where
   * that term is already at 0.04, so the cut removes nothing the model was claiming. */
  static constexpr double kSunBlindConeDeg = 2.0;

  /* ---- THE CLOUD MARCH. The price the IRST declined (doc/sensors.md §6.5) and this channel pays: a
   * transmittance along the line of sight instead of a lid, so a 40 % deck is mostly hole. Bounded by
   * the eye's own reach — at most three shell segments, a few km of path, <= 8 contacts at 1 Hz. */
  static constexpr int kCloudMarchSteps = 12;   /* [SET] samples per deck segment */

  /* Two consecutive looks, the radar's and the IRST's figure and reason: acquisition costs measurable
   * TIME, and a shape that flashes once through the field is never reported. */
  static constexpr int kHitsToFirm = 2;
  static constexpr double kCoastFrames = 3.0;
  static constexpr double kMinCoastS = 1.0;

  ~FBVisualSystem() override = default;

  /* NO TEAM, and that is structural rather than tidy: FBRwrSystem stores a SelfTeam_ it deliberately
   * never reads, which doc/sensors.md logs as dead state with cheat potential. An eye has no use for
   * one, so it is not given one and cannot grow the habit. */
  void SetIdentity(int selfId) { SelfId_ = selfId; }

  void SetPowered(bool on) { Powered_ = on; }
  bool Powered() const { return Powered_; }

  /* Measuring rigs only (doc/missions/sensors.md): the cone is a setting, never an override. */
  void SetCone(double azHalfDeg, double elHalfDeg);
  const FBVisualFieldOfRegard &Field() const { return Field_; }

  /* The weather the OWNER resolved, handed down exactly like the ground elevation and on the same
   * cadence — the sensor never queries the world. Never set = a clear sky. */
  void SetSky(const FBCloudSky &sky) { Sky_ = sky; }

  /* `state` is read as well as written, once and one way: FBEnvironmentBlock's sun angles. An Invalid
   * environment block (a mission with no `time` line) means NO CLOCK, and the channel then runs as a
   * clear day with no glare — every mission written before the clock existed was read as a day
   * mission, and silently blinding them would change 84 measurements to say something they never said. */
  void Run(FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry *net,
           double simTimeS);

  const char *TelemetryName() const override { return "vis"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

private:
  struct Track {
    int    UnitId = 0;
    int    TrackNum = 0;
    double BearingDeg = 0.0, ElevAngleDeg = 0.0, AzDeg = 0.0, ElDeg = 0.0;
    double SizeRad = 0.0;
    double LastLookS = 0.0;
    int    Hits = 0;
    bool   Firm = false;
    bool   Masked = false;               /* the cloud transmittance is what is rejecting it */
    FBVisualState State = FBVisualState::Detected;
    char   TypeName[kVisualTypeNameLen] = {};
  };

  /* One look at the whole field. Registry IN ORDER, so a file number hangs on the mission declaration
   * and never on who was noticed first — the determinism rule all six channels share. */
  void ScanFrame(const FBState &state, const Fdm::fb_fdm_state &st, const Units::FBUnitRegistry &net,
                 double simTimeS);
  void DropAllTracks(const char *reason);

  /* The transmittance of every cloud deck standing between the two points: exp(-sum sigma*rho*ds) over
   * the segment each deck's altitude band actually cuts out of the line of sight. */
  double CloudTransmittance(const Fdm::fb_fdm_state &own, const Units::FBUnitPose &tgt) const;
  /* The Stiles-Holladay factor for a contact on this world bearing/elevation, 1 = no glare, 0 = inside
   * the blind cone. `haveSun` false (no mission clock) = 1. */
  double GlareFactor(const FBState &state, double bearingDeg, double elevAngleDeg) const;

  Track Tracks_[kMaxVisualContacts]{};
  int TrackCount_ = 0;
  /* WHOM THE CLOUD IS CURRENTLY HIDING — the counterfactual set, kept apart from the track array
   * because these are precisely the units this sensor CANNOT see. It exists so `vis MASKED` can be
   * edge-triggered instead of repeated every look; a target that was never detected in the first place
   * is exactly the case worth logging, and it has no track file to hang the flag on. */
  int MaskedIds_[kMaxVisualContacts]{};
  int MaskedIdCount_ = 0;
  int NextTrackNum_ = 1;
  int SelfId_ = 0;
  bool Powered_ = true;   /* a pilot has eyes: unlike every box in the jet this one boots ON */
  FBVisualFieldOfRegard Field_{};
  FBCloudSky Sky_{};
  double NextScanS_ = 0.0;

  int ContactCount_ = 0, MaskedCount_ = 0;
  float BestMrad_ = 0.0f, BestAzDeg_ = 0.0f, BestElDeg_ = 0.0f, GlareMin_ = 1.0f;
  int BestState_ = 0;
  int BlockStatus_ = 0;
};

} // namespace FlightBox::Sensors
#endif
