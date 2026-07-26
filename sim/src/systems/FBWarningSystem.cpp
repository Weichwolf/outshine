#include "FBWarningSystem.h"

namespace FlightBox {

void FBWarningSystem::Run(FBState &state, double dt) {
  (void)dt;
  uint32_t active = 0, inhibited = 0;

  /* ALOW: the documented effect-side precondition (class banner). A threshold of 0 means none entered,
   * which is not an inhibit — there is simply nothing to warn about. */
  if (state.Ufc.H.Readable() && state.Ufc.AlowFt > 0.0f) {
    if (!state.RadarAlt.H.Readable()) inhibited |= FBWarnAlow;
    else if (state.RadarAlt.AglFt < state.Ufc.AlowFt) active |= FBWarnAlow;
  }

  /* BINGO against the EFFECTIVE threshold, not the entered one: the ceiling is the jet's business and
   * the UFC block publishes both numbers for exactly this reason (core/FBAvionicsBlocks.h). */
  if (state.Ufc.H.Readable() && state.Ufc.BingoEffectiveLbs > 0.0f) {
    if (!state.Airframe.H.Readable()) inhibited |= FBWarnBingo;
    else if (state.Airframe.FuelLbs <= state.Ufc.BingoEffectiveLbs) active |= FBWarnBingo;
  }

  /* Gear: on the wheels without the gear down-and-locked. Cheap, and the one condition whose inputs
   * come from a single block, so it is the control case for the two fusions above. */
  if (!state.Airframe.H.Readable()) inhibited |= FBWarnGearUnsafe;
  else if (state.Airframe.WeightOnWheels && state.Airframe.GearPosition < 0.99f)
    active |= FBWarnGearUnsafe;

  state.Warnings.Active = active;
  state.Warnings.Inhibited = inhibited;
  state.Warnings.H.Publish(state.NowS);
  Active_ = active;
  Inhibited_ = inhibited;
}

void FBWarningSystem::DeclareTelemetry(FBTelemetrySchema &schema) const {
  schema.Add("warn_active");
  schema.Add("warn_inhibited");
}

void FBWarningSystem::SampleTelemetry(FBTelemetryRow &row) const {
  row.Push((int)Active_);
  row.Push((int)Inhibited_);
}

} // namespace FlightBox
