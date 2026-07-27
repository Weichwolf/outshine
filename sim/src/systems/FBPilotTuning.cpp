#include "FBPilotTuning.h"

namespace FlightBox {

namespace {
/* The whole public surface of a pilot variant, in one table: mission key, parameter, and the band the
 * value has to be inside. The bands are sanity rails, not judgement — see the header. */
struct Entry {
  const char  *Key;
  FBPilotParam Param;
  double       Lo, Hi;
};
const Entry kParams[] = {
  {"pilot_speed_kt",       FBPilotParam::InterceptSpeedKt, 150.0,  900.0},
  {"pilot_lock_nm",        FBPilotParam::LockRangeNm,        1.0,   40.0},   /* the APG-68's own gate */
  {"pilot_shot_rtr",       FBPilotParam::ShotRtrFactor,      0.1,    3.0},   /* >1 = shoot beyond Rtr */
  {"pilot_shot_ata_deg",   FBPilotParam::ShotAtaDeg,         1.0,   60.0},   /* the gimbal limit */
  {"pilot_shot_spacing_s", FBPilotParam::ShotSpacingS,       0.0,  120.0},
  {"pilot_crank_deg",      FBPilotParam::CrankAtaDeg,        0.0,   60.0},
  {"pilot_abort_nm",       FBPilotParam::AbortRangeNm,       0.0,   40.0},
  {"pilot_beam_deg",       FBPilotParam::BeamOffsetDeg,      0.0,  180.0},
  {"pilot_chaff_s",        FBPilotParam::ChaffIntervalS,     0.2,   60.0},
  {"pilot_defend_hold_s",  FBPilotParam::DefendHoldS,        0.0,  120.0},
  {"pilot_react_s",        FBPilotParam::ReactionS,          0.0,   30.0},
  {"pilot_action_s",       FBPilotParam::ActionSpacingS,     0.1,   30.0},
  /* One squeeze, in seconds. Its DURATION is not the bus's spacing: a pilot may fire a tenth of a
   * second and wait, and against a tracking solution that only holds for a fraction of a second that is
   * the shot that hits. Upper rail = the gun's own longest honoured burst (core/FBGun.h's MaxBurstS). */
  {"pilot_gun_burst_s",    FBPilotParam::GunBurstS,          0.1,    1.0},
  /* The control position's range band. The rails span "inside the gun's minimum range" to "a missile
   * shot's worth of separation"; which end a mission wants is what the weapon decides. */
  {"pilot_gun_tol_frac",   FBPilotParam::GunFireTolFrac,     0.05,    1.0},
  {"pilot_bfm_ctrl_min_nm", FBPilotParam::BfmCtrlMinNm,      0.05,    5.0},
  {"pilot_bfm_ctrl_max_nm", FBPilotParam::BfmCtrlMaxNm,      0.05,   10.0},
  /* The release moment's own offset from the computed cue, in seconds. The band is deliberately WIDE
   * and SIGNED: this is the parameter a mission declares when it wants a WRONG delivery, so that the
   * resulting miss can be measured against the computed one. +/-10 s at fighter speeds is a couple of
   * kilometres either way — far past any real error and exactly the point. */
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

} // namespace FlightBox
