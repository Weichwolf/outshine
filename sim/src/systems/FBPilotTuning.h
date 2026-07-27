/* FlightBox — FBPilotTuning: a pilot VARIANT as mission data. The decision numbers of an intercept —
 * where the lock is spent, how deep inside the launch zone the trigger is pulled, how far the shot is
 * cranked away from, when the fight is abandoned, how fast a human answers a warning — are properties
 * of the PILOT, not of the airframe, and systems/FBPilot reads each of them through one of its own
 * hooks (FBF16Pilot supplies the F-16's). This class is the layer between the two: a sparse table of
 * OVERRIDES that a `.fbm` file fills through `set pilot_* <value>` (doc/mission-format.md).
 *
 * WHY IT IS DATA AND NOT A SUBCLASS. A variant is then a LINE in a mission file rather than a class,
 * which is what makes an evolutionary tournament possible at all: the runner writes mission files and
 * nothing is recompiled between two candidates. It also keeps the airframe's own numbers where they
 * belong — an unset entry is not a zero, it is "this pilot's own number", so a mission that overrides
 * nothing flies byte-identically to one that never heard of this class.
 *
 * The table is a fixed array of {have, value} indexed by the parameter ordinal — no allocation, no map
 * lookup in the decision path, and a Set() that refuses an unknown key or an out-of-range value so a
 * misspelt tournament parameter is a mission FAIL (FBModule::ApplySetup's contract) rather than a
 * silently unchanged pilot. */
#ifndef FBPILOTTUNING_H
#define FBPILOTTUNING_H

#include <string>

namespace FlightBox {

/* Append only — the `.fbm` key strings are the public names (FBPilotTuning::Set). */
enum class FBPilotParam {
  InterceptSpeedKt,        /* pilot_speed_kt        — the speed the intercept is flown at */
  LockRangeNm,             /* pilot_lock_nm         — where the lock (and its warning) is spent */
  ShotRtrFactor,           /* pilot_shot_rtr        — trigger at this multiple of Rtr */
  ShotAtaDeg,              /* pilot_shot_ata_deg    — how far off the nose a shot is still taken */
  ShotSpacingS,            /* pilot_shot_spacing_s  — between two shots at one target */
  CrankAtaDeg,             /* pilot_crank_deg       — how far the supported shot is cranked away */
  AbortRangeNm,            /* pilot_abort_nm        — below this the intercept is over */
  BeamOffsetDeg,           /* pilot_beam_deg        — the defensive turn off the threat bearing */
  ChaffIntervalS,          /* pilot_chaff_s         — how often chaff goes out while defending */
  DefendHoldS,             /* pilot_defend_hold_s   — how long the defence is held after the warning */
  ReactionS,               /* pilot_react_s         — the human delay before a defensive command */
  ActionSpacingS,          /* pilot_action_s        — the hands: one cockpit action per this long */
  GunBurstS,               /* pilot_gun_burst_s     — how long one squeeze of the trigger lasts */
  GunFireTolFrac,          /* pilot_gun_tol_frac    — how tightly the pipper is held before firing */
  BfmCtrlMinNm,            /* pilot_bfm_ctrl_min_nm — the control position's near edge */
  BfmCtrlMaxNm,            /* pilot_bfm_ctrl_max_nm — ...and its far one */
  Count
};

class FBPilotTuning {
public:
  /* `key` is the FULL mission key including the `pilot_` prefix, `value` its already-parsed number.
   * Returns false for an unknown key or a value outside the parameter's stated band — the caller turns
   * that into a rejected `set` line. The bands are deliberately WIDE (they exist to catch a typo or a
   * unit mix-up, not to encode taste): a tournament is allowed to try a bad idea, it is not allowed to
   * try 6,000 nm. */
  bool Set(const std::string &key, double value);

  bool Has(FBPilotParam p) const { return Have_[(int)p]; }
  /* The tuned value, or the caller's own number when this variant said nothing about it. */
  double Or(FBPilotParam p, double own) const { return Have_[(int)p] ? Value_[(int)p] : own; }

private:
  bool   Have_[(int)FBPilotParam::Count]{};
  double Value_[(int)FBPilotParam::Count]{};
};

} // namespace FlightBox
#endif
