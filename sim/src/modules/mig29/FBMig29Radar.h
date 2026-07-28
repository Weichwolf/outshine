/* FlightBox — FBMig29Radar: the N019 "Rubin", an override of sensors/FBRadarSystem, built exactly the
 * way FBF16Fcr is — a MODE SET behind ActiveVolume(). Detection, build-up, anonymity, IFF and the
 * emission all stay in the generic system.
 *
 * TWO THINGS MAKE IT A DIFFERENT RADAR AND NOT A DIFFERENT NUMBER, and both are why this round widened
 * two generic constants into hooks:
 *   1. The Doppler notch is QUANTIFIED and RANGE-DEPENDENT (doc/modules/mig29/radar-sensors.md §3.1) —
 *      and it REJECTS, i.e. a target inside the filter is not detected at all. The APG-68 has no
 *      documented threshold, so it keeps the old behaviour (the notch is only chaff's channel there).
 *   2. Losing a target to the notch costs SIX SECONDS of inertial tracking before the file drops
 *      (§4, DCS-FM p.37-38), not the generic three antenna frames.
 *
 * WHICH QUANTITY THE THRESHOLDS ARE ON. The source says "closure/lag speed", which read literally is
 * the closure between the two aircraft — but the same source explains the effect by target ASPECT ("at
 * aspect near 90° ... small radial closure, small Doppler shift", DCS-FM p.37), and a beaming target is
 * exactly the case where the aircraft-to-aircraft closure stays large while the target's own radial
 * velocity goes to zero. The physical discriminator of a pulse-Doppler set is the target's Doppler
 * against the MAIN-LOBE CLUTTER Doppler in that direction, i.e. the target's radial velocity over the
 * ground — which is the quantity FBRadarSystem already measures for chaff. The documented numbers are
 * therefore applied to it. [DERIVED]
 *
 * NOT MODELLED, and named rather than approximated: minimum range (250 m, T4), PRF selection
 * (ППС/ЗПС/АВТ and its -25 % range penalty), TWS as a track-while-scan capacity of its own, the AOJ/
 * burn-through jamming chain, and the source's probabilistic wording ("automatic tracking MAY be
 * disrupted", "an aggressive turn in the notch breaks lock with HIGH PROBABILITY") — FlightBox has no
 * die, so a documented "not guaranteed" is read as "lost". doc/modules/mig29/module.md. */
#ifndef FBMIG29RADAR_H
#define FBMIG29RADAR_H

#include <cstring>
#include "FBRadarSystem.h"

namespace FlightBox::Modules {

/* Ordinals are the telemetry's `fcr_mode` column, so the order is observable schema — append, never
 * reorder. */
enum class FBMig29RadarMode { Off, Rad, Cc, Vs, Bore, Acm };

/* The PUR-31 emission switch, a real three-position control on this jet [DCS-EA p.63]: ILLUM = combat,
 * DUMMY = an antenna-equivalent TEST state that does NOT radiate, OFF = powered down. The generic
 * system already has the two halves (SetPowered + a non-Active volume), so DUMMY is not new mechanism —
 * it is the one state the F-16's set has no switch for. */
enum class FBMig29Emission { Illum, Dummy, Off };

/* The ZONE switch: the 130° azimuth field is divided into three OVERLAPPING sectors, and the pilot
 * picks one — the scan volume is slewed in discrete thirds, not continuously [DCS-EA p.12-13,
 * DCS-FM p.43-44]. */
enum class FBMig29Zone { Left, Center, Right };

inline bool FBMig29RadarModeFromString(const char *s, FBMig29RadarMode &out) {
  if (!std::strcmp(s, "off"))  { out = FBMig29RadarMode::Off;  return true; }
  if (!std::strcmp(s, "rad"))  { out = FBMig29RadarMode::Rad;  return true; }
  if (!std::strcmp(s, "cc"))   { out = FBMig29RadarMode::Cc;   return true; }
  if (!std::strcmp(s, "vs"))   { out = FBMig29RadarMode::Vs;   return true; }
  if (!std::strcmp(s, "bore")) { out = FBMig29RadarMode::Bore; return true; }
  if (!std::strcmp(s, "acm"))  { out = FBMig29RadarMode::Acm;  return true; }
  return false;
}

inline const char *FBMig29RadarModeStr(FBMig29RadarMode m) {
  switch (m) {
    case FBMig29RadarMode::Off:  return "off";
    case FBMig29RadarMode::Rad:  return "rad";
    case FBMig29RadarMode::Cc:   return "cc";
    case FBMig29RadarMode::Vs:   return "vs";
    case FBMig29RadarMode::Bore: return "bore";
    case FBMig29RadarMode::Acm:  return "acm";
  }
  return "?";
}

class FBMig29Radar : public Sensors::FBRadarSystem {
public:
  /* ---- Gimbal envelope [T4 §7.1]: az ±65°, el +56°/−36°. The asymmetric elevation is exactly what
   * FBRadarScanVolume's centre+half representation exists for. */
  static constexpr double kGimbalAzHalfDeg = 65.0;
  static constexpr double kGimbalElCenterDeg = 10.0;   /* (+56 + −36)/2 */
  static constexpr double kGimbalElHalfDeg = 46.0;     /* (+56 − −36)/2 */

  /* ---- Search reach. The T4 set is RCS- and altitude-qualified (encounter/HPRF, 3 m², above 3 000 m:
   * 50-70 km search) and radar-sensors.md §7.1 names it as the one to model over the manuals' 70 km
   * brochure figure. The LOWER bound is taken: FlightBox has no RCS model yet (roadmap R6), so the
   * qualifier cannot be honoured and the conservative end is the honest one. It also happens to be one
   * of the set's own HUD range scales (27 nm, DCS-EA p.89). */
  static constexpr double kSearchRangeM = 50000.0;
  /* Close-combat reach [DCS-EA p.89: CC captures from 5.4 nm down; DCS-FM p.87: CAC engagement
   * ranges ~10 km]. The two agree: 10 km = 5.4 nm. */
  static constexpr double kCloseRangeNm = 5.4;

  /* ---- RAD, the long-range search. Azimuth: one ZONE third, ±30° about its centre [DCS-FM p.43-44:
   * centre ±30°, left −60…0°, right 0…+60°]. */
  static constexpr double kZoneHalfDeg = 30.0;
  static constexpr double kZoneOffsetDeg = 30.0;   /* left/right centres, from the same three sectors */
  /* [SET] The elevation bar. No source states the scan-bar coverage of the 9-12 (radar-sensors.md §8
   * gap 1). ±6° is chosen so that the documented RANGE-ANGLE ENTRY (datalink-gci.md §2.2 — the pilot
   * types expected range and relative altitude and the set aims the bar) is the load-bearing act it is
   * described as: at 40 nm a ±6° bar spans ±7 800 ft, so a wrongly cued antenna misses a target it
   * could easily have seen, which is what both manuals warn about. */
  static constexpr double kRadElHalfDeg = 6.0;
  /* [DERIVED] "You may have to wait up to SIX SECONDS before the target is detected ... only after the
   * radar has completed several scanning cycles" [DCS-FM p.84]. The generic system firms a track after
   * kHitsToFirm (2) consecutive looks, so the documented latency IS 2 × FrameS: FrameS = 3.0 s. That
   * sits inside T4's 2.5-5 s scan-cycle band for the widest pattern. */
  static constexpr double kRadFrameS = 3.0;

  /* ---- CC, radar close combat. T4 gives the pattern as a fixed "±37°/−13°" scan and DCS-FM p.12 says
   * the antenna "rotates ONLY IN THE VERTICAL AXIS" in close combat, so the pair is read as a VERTICAL
   * zone −13°…+37° at a fixed azimuth — the reading radar-sensors.md §7.1 itself adopts, and the one
   * that matches DCS-EA §3.3's "captures a target in the vertical search zone". The azimuth width is
   * then one beam: the 3.5° beamwidth [T4], half = 1.75°. */
  static constexpr double kCcAzHalfDeg = 1.75;
  static constexpr double kCcElCenterDeg = 12.0;   /* (+37 + −13)/2 */
  static constexpr double kCcElHalfDeg = 25.0;     /* (+37 − −13)/2 */
  static constexpr double kCcFrameS = 2.5;         /* [T4] close-combat cycle time */

  /* ---- VS, the FC3 vertical-scan acquisition zone: "3° wide × −10…+50° in elevation" [DCS-FM p.54].
   * Lock "1-3 s after the target enters the zone" with the button held → 2 looks × 1.0 s = 2.0 s
   * [DERIVED, inside the band]. */
  static constexpr double kVsAzHalfDeg = 1.5;
  static constexpr double kVsElCenterDeg = 20.0;
  static constexpr double kVsElHalfDeg = 30.0;
  static constexpr double kVsFrameS = 1.0;
  /* ---- BORE: "2.5° cone along the aircraft axis" [DCS-FM p.55], and "better aiming precision and
   * slightly longer lock range than VS" — the faster frame is the acquisition half of that statement
   * [DERIVED]; the longer RANGE half is not modelled (no figure exists) and stays a gap. */
  static constexpr double kBoreHalfDeg = 1.25;
  static constexpr double kBoreFrameS = 0.5;

  /* ---- ACM: the BROAD close-combat auto-acquisition volume — the merge acquisition mode, and the
   * acquisition half of module.md gap 4h. WHY IT EXISTS BESIDE CC/VS/BORE: those three are azimuth
   * PENCILS (±1.75°/±1.5°/±1.25°), the vertical reading DCS-FM p.12 forces ("the antenna rotates ONLY IN
   * THE VERTICAL AXIS in close combat"). A pencil cannot hold a manoeuvring nose-on merge target — the
   * MiG never firms a contact and never acquires (MEASURED, duel-merge lock_s 0). The SAME T4 §7.1
   * figure that CC reads vertically ("fixed ±37°/−13° scan") reads the OTHER way as a broad forward
   * acquisition box, and the two sources genuinely conflict (radar-sensors.md §7.1 records it). This mode
   * takes the ±37° as AZIMUTH — a wide forward search in front of the nose, the role the F-16's acm_hud
   * fills. The elevation half is [SET] ±15°: a forward band tall enough to hold a target through a merge
   * pull, well inside the +56/−36 gimbal, since the vertical extent of the ±37° reading is undocumented.
   * Auto-lock (nobody operates a radar in a knife fight) and Doppler-EXEMPT like CC (a co-speed merge is
   * exactly the low-closure case, DopplerNotchMs below). Frame 2.5 s [T4]. */
  static constexpr double kAcmAzHalfDeg = 37.0;   /* [T4 §7.1] ±37°, read as azimuth */
  /* [SET] ±30°, a nose-centred vertical acquisition band ~60° tall — comparable in extent to the
   * DOCUMENTED close-combat vertical scans (VS −10…+50 = 60°, CC −13…+37 = 50°) but symmetric about the
   * nose rather than biased up, because a merge acquisition must hold a target that moves to EITHER side
   * of the nose as the jet pulls. MEASURED to be the threshold: ±25° never firms in duel-merge (the
   * target dips out between two looks during the pull), ±30° acquires at t=3.9 and locks; wider buys
   * nothing (±55° gives the identical lock). It is well inside the +56/−36 gimbal. */
  static constexpr double kAcmElHalfDeg = 30.0;
  /* [DERIVED] T4 §7.1 gives the close-combat mode a "1-2 s LOCK", distinct from the 2.5 s raster cycle
   * — the LOCK is the operative number, and the generic system firms (and auto-locks) after kHitsToFirm
   * (2) looks, so the frame is that lock time over two: 1.5 s / 2 = 0.75 s. The same construction as
   * kRadFrameS (the 6 s RAD detection over two). It is why the merge acquires at all: a 2.5 s frame needs
   * 5 s of continuous presence, longer than the window a high-closure pass leaves in the forward
   * hemisphere. */
  static constexpr double kAcmFrameS = 0.75;

  /* [SET] The single-target stare. No source gives a frame time for РНП; 0.2 s is twice the APG-68's
   * stare, which is the one relation this older analogue set can be given honestly. */
  static constexpr double kSttFrameS = 0.2;

  /* ---- THE DOPPLER ENVELOPE [DCS-EA p.87], verbatim: beyond 8 nm the closure must exceed 81 kts;
   * inside 8 nm, 27 kts; and detection is "not guaranteed" below 32.4 kts inside 5.4 nm head-on. The
   * third row is the deterministic reading of a probabilistic statement — FlightBox does not roll
   * dice, so "not guaranteed" is modelled as lost, which makes the innermost band WIDER than the one
   * outside it. That inversion is in the source, not a slip. */
  static constexpr double kNotchFarNm = 8.0;
  static constexpr double kNotchNearNm = 5.4;
  static constexpr double kNotchFarKt = 81.0;
  static constexpr double kNotchMidKt = 27.0;
  static constexpr double kNotchNearKt = 32.4;
  /* [DOC DCS-FM p.37-38] "On entering the notch the radar applies inertial tracking for up to 6
   * seconds, extrapolating the target's trajectory and pointing the antenna at the predicted exit
   * point." A DURATION, not a frame count — which is why it REPLACES the generic rule rather than
   * flooring it: the generic coast is `3 antenna frames` (9 s in RAD, 0.6 s in the stare), and both
   * would contradict a source that names one number for the filter. Applied to every loss, not only to
   * a notch loss: the set has one track filter and no source describes a second. */
  static constexpr double kInertialTrackS = 6.0;

  FBMig29Radar();

  void SetMode(FBMig29RadarMode m);
  FBMig29RadarMode Mode() const { return Mode_; }

  void SetEmission(FBMig29Emission e);
  FBMig29Emission Emission_State() const { return Emit_; }

  void SetZone(FBMig29Zone z);
  FBMig29Zone Zone() const { return Zone_; }

  /* The antenna elevation control (PUR-31, "continuous"): the centre of the RAD bar. It is the OUTPUT
   * of the documented range-angle entry — the pilot types range and relative altitude, the set aims
   * the bar — so the GCI loop reaches the radar through exactly this one number. */
  void SetAntennaElevDeg(double deg);
  double AntennaElevDeg() const { return AntennaElDeg_; }

  /* Overrides every mode's gate with ONE figure (0 = back to the table), same purpose as the F-16's:
   * a mission that measures the gate holds the geometry fixed and varies only the reach. */
  void SetRangeOverrideNm(double nm);

protected:
  const Sensors::FBRadarScanVolume &ActiveVolume() const override {
    if (Emit_ != FBMig29Emission::Illum) return Silent_;   /* DUMMY/OFF beat the mode AND the lock */
    if (Mode_ == FBMig29RadarMode::Off) return Silent_;
    return Locked() ? Stt_ : Modes_[(int)Mode_];
  }
  int ModeOrdinal() const override { return (int)Mode_; }

  /* The whole reason the base class grew this hook. CC is exempt because the source says so outright:
   * "stable automatic tracking is provided AT EQUAL SPEEDS AND AT A LAG" [DCS-EA §3.3] — dropping the
   * closure requirement is precisely what makes it the dogfight mode. */
  double DopplerNotchMs(double rangeM) const override;
  bool NotchRejectsDetection() const override { return true; }
  double CoastS(const Sensors::FBRadarScanVolume &v) const override {
    (void)v;
    return kInertialTrackS;
  }

private:
  void RebuildVolumes();

  FBMig29RadarMode Mode_ = FBMig29RadarMode::Rad;
  /* Power-up state: the emission switch starts on OFF and the pilot (or the mission) turns it to
   * ILLUM. That is this jet's doctrine made structural — "radiate as late as possible"
   * (datalink-gci.md §5.2) is only a decision if not radiating is the default. */
  FBMig29Emission Emit_ = FBMig29Emission::Off;
  FBMig29Zone Zone_ = FBMig29Zone::Center;
  double AntennaElDeg_ = 0.0;
  double RangeOverrideNm_ = 0.0;
  Sensors::FBRadarScanVolume Modes_[6]{};
  Sensors::FBRadarScanVolume Stt_{};
  Sensors::FBRadarScanVolume Silent_{};
};

} // namespace FlightBox::Modules
#endif
