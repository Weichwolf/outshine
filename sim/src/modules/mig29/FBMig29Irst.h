/* FlightBox — FBMig29Irst: the OEPS-29 / KOLS ("Quantum Optical-Laser Station"), an override of
 * sensors/FBIrstSystem — an infrared search-and-track head COLLIMATED WITH A LASER RANGEFINDER, in the
 * transparent sphere forward and right of the canopy.
 *
 * This is the sensor that makes the MiG-29 a different opponent rather than a slower one. The F-16
 * pilot's cost function is "a lock warns the target"; this jet's is "RADIATING AT ALL warns the target,
 * and not radiating costs range AND identity" (doc/modules/mig29/radar-sensors.md §6.4, quoting both
 * manuals). Everything below is one of those three terms:
 *
 *   RANGE      — 25 km at best against 50 km for the N019, and only from behind.
 *   IDENTITY   — none. "The IFF interrogator does not operate with the IRST." The generic slot has no
 *                IFF field at all, so this is structural rather than a switch left off.
 *   STEALTH    — nothing is transmitted. "The enemy doesn't know that OESS is tracking his aircraft",
 *                and the laser is not detectable by the target's RWR either — which is why the whole
 *                launch computation can be built without ever appearing on a warning receiver.
 *
 * MODES. DCS-EA's 9-12 taxonomy puts IR (search) and IR CC (close combat, REAR hemisphere, aspect
 * limited) on the WCS-modes selector next to the radar's RAD/CC; the FC3 manual's Vertical Scan and
 * BORE keys select a ZONE with the IRST as the DEFAULT sensor behind it. The zone geometry therefore
 * lives once per sensor, and this class carries the optical half of it.
 *
 * NOT MODELLED, named rather than approximated: the Shchel-3UM helmet sight (a designation channel to
 * a missile seeker, and there is no IR missile in the tree to designate to); the IR GAIN knob and its
 * "ПП" spurious-mark thinning; the documented degradation under thermal countermeasures (5.4-1.6 nm) —
 * flares are dispensed and counted in this simulator but not published in FBUnitSignature, so there is
 * nothing to degrade against; and the SPAN angular ranging method, which is the OTHER fallback range
 * source beyond the laser's 6 km. */
#ifndef FBMIG29IRST_H
#define FBMIG29IRST_H

#include <cstring>
#include "FBIrstSystem.h"

namespace FlightBox::Modules {

/* Ordinals are the telemetry's `irst_mode` column — append, never reorder. */
enum class FBMig29IrstMode { Off, Ir, IrCc, Bore };

inline bool FBMig29IrstModeFromString(const char *s, FBMig29IrstMode &out) {
  if (!std::strcmp(s, "off"))   { out = FBMig29IrstMode::Off;  return true; }
  if (!std::strcmp(s, "ir"))    { out = FBMig29IrstMode::Ir;   return true; }
  if (!std::strcmp(s, "ir_cc")) { out = FBMig29IrstMode::IrCc; return true; }
  if (!std::strcmp(s, "bore"))  { out = FBMig29IrstMode::Bore; return true; }
  return false;
}

inline const char *FBMig29IrstModeStr(FBMig29IrstMode m) {
  switch (m) {
    case FBMig29IrstMode::Off:  return "off";
    case FBMig29IrstMode::Ir:   return "ir";
    case FBMig29IrstMode::IrCc: return "ir_cc";
    case FBMig29IrstMode::Bore: return "bore";
  }
  return "?";
}

class FBMig29Irst : public Sensors::FBIrstSystem {
public:
  /* [DOC §6.2, DCS-EA p.91] Clean-air IR detection range 13.5 … 5.4 nm (25 … 10 km). The pair is
   * read as REAR-aspect best case and HEAD-ON worst case — "IR detection is more effective from the
   * rear" (DCS-FM p.85) is the source's own reason for there being two numbers at all. 25 km is also
   * within 10 km of the independent T4 research figure (15 km), which is recorded in §6.2 as the
   * lower-confidence value; the manual pair is preferred because both ends of it are stated together. */
  static constexpr double kRearRangeM = 25000.0;
  static constexpr double kFrontRangeM = 10000.0;

  /* [DOC §6.2, T4] Laser rangefinder 6 km — the one number that bounds where the PASSIVE channel can
   * produce a TRUE range for a launch computation. Beyond it the real WCS falls back on the SPAN
   * angular method, which is why that knob exists and why this limit matters. */
  static constexpr double kLaserRangeM = 6000.0;

  /* [DOC §6.2, T4] Field of view: azimuth ±30° or ±15°, elevation ±15°. The wider azimuth is taken for
   * the SEARCH field (it is the one that has to find anything) and the narrower for close combat. */
  static constexpr double kSearchAzHalfDeg = 30.0;
  static constexpr double kSearchElHalfDeg = 15.0;
  static constexpr double kCcAzHalfDeg = 15.0;
  static constexpr double kCcElHalfDeg = 15.0;
  /* [DOC §6.2, DCS-FM p.86] "IRST dwell to search one increment: 4-6 s." [SET] 5.0 s, the middle,
   * read as the frame of the whole pattern — the raster's increment COUNT is undocumented (§8 gap 5).
   * With the generic two-look firming that makes acquisition cost 10 s, which is the slowest sensor in
   * the tree by a wide margin and is exactly the trade the doctrine pays for. */
  static constexpr double kSearchFrameS = 5.0;
  /* [SET] Close combat is the same head on a narrower field, so it comes round faster; ratio of the
   * areas, rounded — the relation is the model, as with the radar's frames. */
  static constexpr double kCcFrameS = 2.5;
  /* [DOC DCS-FM p.55] BORE: a 2.5° cone along the aircraft axis. Same aiming device as the radar's
   * BORE, other sensor behind it. */
  static constexpr double kBoreHalfDeg = 1.25;
  static constexpr double kBoreFrameS = 1.0;   /* [SET] a stare, not a sweep */

  /* [DOC DCS-EA p.92] IR CC is a REAR-HEMISPHERE mode with a target aspect limit of 3/4 — the one
   * aspect gate the sources state numerically enough to build. 3/4 aspect is read as 135°, i.e. the
   * head refuses a source whose aspect angle is beyond it (nearly head-on). */
  static constexpr double kCcAspectLimitDeg = 135.0;

  FBMig29Irst();

  void SetMode(FBMig29IrstMode m);
  FBMig29IrstMode Mode() const { return Mode_; }

protected:
  const Sensors::FBIrstFieldOfRegard &ActiveField() const override {
    if (Mode_ == FBMig29IrstMode::Off) return Fields_[(int)FBMig29IrstMode::Off];
    return Locked() ? Track_ : Fields_[(int)Mode_];
  }
  int ModeOrdinal() const override { return (int)Mode_; }
  double LaserRangeM() const override { return kLaserRangeM; }

  /* The aspect law with this head's two documented endpoints, plus IR CC's aspect gate on top. */
  double DetectRangeM(const Sensors::FBIrstFieldOfRegard &f, const Units::FBUnitPose &tgt,
                      bool afterburner, double bearingDeg) const override;

private:
  void RebuildFields();

  FBMig29IrstMode Mode_ = FBMig29IrstMode::Off;
  Sensors::FBIrstFieldOfRegard Fields_[4]{};
  Sensors::FBIrstFieldOfRegard Track_{};
};

} // namespace FlightBox::Modules
#endif
