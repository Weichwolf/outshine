/* FlightBox — FBF16Hud: the F-16's own HUD symbology (doc/f16/hud-symbology.md, DCS F-16C Viper Guide
 * Part 16 p.706 as the reference frame; positions cross-checked against the GPL-2.0 FlightGear F-16 mod
 * https://github.com/NikolaiVChr/f16, Nasal/HUD/HUD_main.nas + hud_math.nas — FACTS verified, no code
 * copied). Overrides FBDisplaySystem::BuildHud, the module's Displays override point: FPM, conformal
 * pitch ladder, heading/CAS/altitude tapes, bank-angle scale, G-load, the ARM/Mach/Peak-G/NAV/bullseye
 * block, the R/AL/B/TTG/dist>STPT block, and the steerpoint diamond+tadpole. Pure symbology — reads
 * FBState (written by FBAirDataSystem/FBRadarAltimeter/FBNavSystem/FBF16FireControl/FBF16Ufc/FBF16Sms),
 * writes nothing. */
#ifndef FBF16HUD_H
#define FBF16HUD_H

#include "FBDisplaySystem.h"

namespace FlightBox {

class FBF16Hud : public FBDisplaySystem {
public:
  void BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const override;
};

} // namespace FlightBox
#endif
