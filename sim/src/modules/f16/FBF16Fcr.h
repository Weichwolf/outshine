/* FlightBox — FBF16Fcr: the AN/APG-68, an override of systems/FBRadarSystem. What is F-16 about it is
 * only the MODE SET — detection, build-up, coast, anonymity and IFF all live in the generic system.
 * The whole taxonomy is one question ("which pattern is the antenna flying?"), which is exactly the one
 * overridden hook. The taxonomy is [DOC]; the ANGLES are a declared MODEL PARAMETER SET [SET] — the
 * source documents the modes as MFD screenshots and gives no geometry. No HUD symbology, deliberately:
 * the symbology reference documents no TD box. Volumes, frame times and their justification:
 * doc/flightbox/modules-f16.md §4. */
#ifndef FBF16FCR_H
#define FBF16FCR_H

#include <cstring>
#include "FBRadarSystem.h"

namespace FlightBox {

/* Ordinals are the telemetry's `fcr_mode` column, so the order is observable schema — append, never
 * reorder. */
enum class FBF16FcrMode { Off, Crm, AcmHud, AcmBore, AcmVert, AcmSlew };

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
  /* Every close-in sub-mode is a 10 nm acquisition mode: the volume is the aiming device, not reach. */
  static constexpr double kAcmRangeNm = 10.0;
  static constexpr double kSearchRangeNm = 40.0;   /* against a fighter-size target */
  static constexpr double kGimbalAzDeg = 60.0;
  static constexpr double kGimbalElDeg = 60.0;

  FBF16Fcr();

  void SetMode(FBF16FcrMode m);
  FBF16FcrMode Mode() const { return Mode_; }

  /* The commanded box centre (deg off the nose). Two axes, two setters, so no caller carries a shadow
   * copy of the one it is not changing; also CRM's antenna-elevation control. */
  void SetSlewAz(double azDeg);
  void SetSlewEl(double elDeg);

  /* Overrides EVERY mode's range gate with ONE figure (0 = back to the table): a mission that tests the
   * gate at all holds the geometry fixed and varies only the reach. */
  void SetRangeOverrideNm(double nm);

protected:
  /* THE override: the sub-mode's box while searching, the STT gimbal envelope once locked. That change
   * from "sweep a small box" to "stare at one target" IS the whole ACM->STT transition. */
  const FBRadarScanVolume &ActiveVolume() const override {
    /* Off outranks the lock: switching the set off must STOP the antenna, not leave it staring through
     * an OFF mode. Every other mode hands the lock over to STT. */
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
