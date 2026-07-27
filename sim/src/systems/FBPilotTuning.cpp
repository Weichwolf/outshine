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
