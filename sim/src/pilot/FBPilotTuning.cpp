#include "FBPilotTuning.h"

namespace FlightBox::Pilot {

namespace {
/* Die ganze oeffentliche Flaeche einer Piloten-Variante in EINER Tabelle. Die Baender sind
 * Plausibilitaetsgelaender, kein Geschmack: ein Turnier darf eine schlechte Idee versuchen, es darf
 * nicht 6.000 nm versuchen. Bedeutung je Schluessel: doc/flightbox/pilot-ai.md, Abschnitt 9. */
struct Entry {
  const char  *Key;
  FBPilotParam Param;
  double       Lo, Hi;
};
const Entry kParams[] = {
  {"pilot_speed_kt",       FBPilotParam::InterceptSpeedKt, 150.0,  900.0},
  {"pilot_lock_nm",        FBPilotParam::LockRangeNm,        1.0,   40.0},   /* das Tor des APG-68 */
  {"pilot_shot_rtr",       FBPilotParam::ShotRtrFactor,      0.1,    3.0},   /* >1 = jenseits von Rtr */
  {"pilot_shot_ata_deg",   FBPilotParam::ShotAtaDeg,         1.0,   60.0},   /* der Kardanwinkel */
  {"pilot_shot_spacing_s", FBPilotParam::ShotSpacingS,       0.0,  120.0},
  {"pilot_crank_deg",      FBPilotParam::CrankAtaDeg,        0.0,   60.0},
  {"pilot_abort_nm",       FBPilotParam::AbortRangeNm,       0.0,   40.0},
  {"pilot_beam_deg",       FBPilotParam::BeamOffsetDeg,      0.0,  180.0},
  {"pilot_chaff_s",        FBPilotParam::ChaffIntervalS,     0.2,   60.0},
  {"pilot_defend_hold_s",  FBPilotParam::DefendHoldS,        0.0,  120.0},
  {"pilot_react_s",        FBPilotParam::ReactionS,          0.0,   30.0},
  {"pilot_action_s",       FBPilotParam::ActionSpacingS,     0.1,   30.0},
  /* Obergrenze = der laengste vom Geschuetz honorierte Feuerstoss (core/FBGun.h, MaxBurstS). */
  {"pilot_gun_burst_s",    FBPilotParam::GunBurstS,          0.1,    1.0},
  {"pilot_gun_tol_frac",   FBPilotParam::GunFireTolFrac,     0.05,    1.0},
  {"pilot_bfm_ctrl_min_nm", FBPilotParam::BfmCtrlMinNm,      0.05,    5.0},
  {"pilot_bfm_ctrl_max_nm", FBPilotParam::BfmCtrlMaxNm,      0.05,   10.0},
  /* Bewusst WEIT und VORZEICHENBEHAFTET: der Parameter fuer einen ABSICHTLICH falschen Abwurf. */
  {"pilot_attack_bias_s",  FBPilotParam::AttackBiasS,      -10.0,   10.0},
  {"pilot_attack_ccip_m",  FBPilotParam::AttackCcipTolM,     1.0, 2000.0},
};
} // namespace

bool FBPilotTuning::Set(const std::string &key, double value) {
  for (const Entry &e : kParams) {
    if (key != e.Key) continue;
    if (value < e.Lo || value > e.Hi) return false;
    Have_[(int)e.Param] = true;
    Value_[(int)e.Param] = value;
    return true;
  }
  return false;
}

} // namespace FlightBox::Pilot
