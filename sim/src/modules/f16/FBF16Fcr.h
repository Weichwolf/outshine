/* FlightBox — FBF16Fcr: the F-16's AN/APG-68 Fire Control Radar, an override of the generic active set
 * (systems/FBRadarSystem — read its banner first; everything about HOW detection, build-up, coast and
 * IFF work lives there, this file carries only what is F-16 about it).
 *
 * WHAT IS F-16 ABOUT IT: the MODE SET. From doc/f16/radar-sensors.md (DCS F-16C Viper Guide, Part 10):
 *   - top level is BVR search / ACM (air combat maneuvering, close-in) / STT (single target track);
 *   - CRM (Combined Radar Mode) is the mode the set powers up in — a large-volume, all-aspect RWS search
 *     that does NOT lock by itself;
 *   - the four ACM sub-modes — HUD Scan, Vertical Scan, Boresight, Slewable — "auto-lock the first
 *     target in a close-range volume tied to HUD geometry". That auto-acquisition is the whole point:
 *     nobody operates a radar in a turning fight, so the sub-mode IS the aiming device;
 *   - a lock is STT: all radar power on one target, and the antenna then follows it wherever it goes
 *     inside its gimbal limits instead of continuing to sweep the little ACM box.
 * All of that is one question — "which pattern is the antenna flying right now?" — which is exactly
 * FBRadarSystem::ActiveVolume(), the one hook overridden below.
 *
 * THE NUMBERS, honestly sourced. doc/f16/radar-sensors.md documents the TAXONOMY and gives no angles:
 * the guide presents the modes as MFD screenshots. The geometry below is therefore a MODEL PARAMETER
 * SET, chosen to match the publicly quoted DCS F-16C ACM patterns (30x20 HUD scan, a ~10 deg boresight
 * acquisition cone, a narrow-and-tall vertical scan reaching well above the boresight, a 20x20 slewable
 * box, all at the ~10 nm ACM range gate), the APG-68's ~40 nm fighter-target search range and its
 * mechanically-scanned 4-bar elevation coverage. It is declared as such — the same status
 * FBDatalinkSystem::kGenericRangeNm carries — rather than dressed up as a citation the doc cannot back.
 *
 * NO HUD SYMBOLOGY, deliberately. doc/f16/hud-symbology.md documents no target designator box and no
 * locked-target symbol: its radar-adjacent entry is the HMC (HUD Mark Cue), a markpoint designation
 * circle, which is a different function. Drawing a TD box would mean inventing symbology, so the lock
 * stays in FBState/telemetry/events until the symbology reference actually covers it. */
#ifndef FBF16FCR_H
#define FBF16FCR_H

#include <cstring>
#include "FBRadarSystem.h"

namespace FlightBox {

/* The FCR modes a mission may select (`set fcr_mode`). Ordinals are the telemetry's `fcr_mode` column,
 * so the order is part of the observable schema — append, never reorder. */
enum class FBF16FcrMode { Off, Crm, AcmHud, AcmBore, AcmVert, AcmSlew };

/* The `set fcr_mode` spellings, strict like every other .fbm value (same shape as
 * FBF16ContactFilterFromString / core/FBTeam.h's FBUnitTeamFromString). */
inline bool FBF16FcrModeFromString(const char *s, FBF16FcrMode &out) {
  if (!std::strcmp(s, "off"))       { out = FBF16FcrMode::Off;     return true; }
  if (!std::strcmp(s, "crm"))       { out = FBF16FcrMode::Crm;     return true; }
  if (!std::strcmp(s, "acm_hud"))   { out = FBF16FcrMode::AcmHud;  return true; }
  if (!std::strcmp(s, "acm_bore"))  { out = FBF16FcrMode::AcmBore; return true; }
  if (!std::strcmp(s, "acm_vert"))  { out = FBF16FcrMode::AcmVert; return true; }
  if (!std::strcmp(s, "acm_slew"))  { out = FBF16FcrMode::AcmSlew; return true; }
  return false;
}

inline const char *FBF16FcrModeStr(FBF16FcrMode m) {
  switch (m) {
    case FBF16FcrMode::Off:     return "off";
    case FBF16FcrMode::Crm:     return "crm";
    case FBF16FcrMode::AcmHud:  return "acm_hud";
    case FBF16FcrMode::AcmBore: return "acm_bore";
    case FBF16FcrMode::AcmVert: return "acm_vert";
    case FBF16FcrMode::AcmSlew: return "acm_slew";
  }
  return "?";
}

class FBF16Fcr : public FBRadarSystem {
public:
  /* The ACM range gate: every close-in sub-mode is a 10 nm acquisition mode — the volume is the aiming
   * device, not the reach. */
  static constexpr double kAcmRangeNm = 10.0;
  /* The search/track reach for a fighter-size target, and the STT gimbal envelope the antenna can follow
   * a locked target through once it has one. */
  static constexpr double kSearchRangeNm = 40.0;
  static constexpr double kGimbalAzDeg = 60.0;
  static constexpr double kGimbalElDeg = 60.0;

  FBF16Fcr();

  void SetMode(FBF16FcrMode m);
  FBF16FcrMode Mode() const { return Mode_; }

  /* The Slewable sub-mode's commanded box centre (deg off the nose / off the boresight plane) — the
   * cursor position the pilot would have slewed it to. Two axes, two setters, so no caller has to carry
   * a shadow copy of the one it is not changing. Ignored by every other mode. */
  void SetSlewAz(double azDeg);
  void SetSlewEl(double elDeg);

  /* Overrides EVERY mode's range gate with one figure (`set fcr_range_nm`; 0 = back to the table below)
   * — a mission that wants to show the range limit bite sets it small, exactly as `set
   * datalink_range_nm` does for the terminal. One number rather than one per mode: a mission that cares
   * about the gate at all is holding the geometry fixed and varying only the reach. */
  void SetRangeOverrideNm(double nm);

protected:
  /* THE override (class banner): the ACM sub-mode's box while searching, the STT gimbal envelope once
   * something is locked. The mode change from "sweep a small box" to "stare at one target" is the whole
   * ACM->STT transition, expressed as the only thing the base class ever asks. */
  const FBRadarScanVolume &ActiveVolume() const override {
    /* Off outranks the lock: switching the set off with a target locked must stop the antenna, not leave
     * it staring through an OFF mode (the base class then drops every track, exactly as a power-down
     * does). Every other mode hands the lock over to STT. */
    if (Mode_ == FBF16FcrMode::Off) return Modes_[(int)FBF16FcrMode::Off];
    return Locked() ? Stt_ : Modes_[(int)Mode_];
  }
  int ModeOrdinal() const override { return (int)Mode_; }

private:
  void RebuildVolumes();

  FBF16FcrMode Mode_ = FBF16FcrMode::Crm;   /* CRM is the power-up mode (doc/f16/radar-sensors.md) */
  double SlewAzDeg_ = 0.0, SlewElDeg_ = 0.0;
  double RangeOverrideNm_ = 0.0;   /* 0 = use the mode table's own gate */
  FBRadarScanVolume Modes_[6]{};
  FBRadarScanVolume Stt_{};
};

} // namespace FlightBox
#endif
