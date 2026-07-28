#include "FBWarningSystem.h"

namespace FlightBox::Systems {

void FBWarningSystem::Run(FBState &state, double dt) {
  (void)dt;
  uint32_t active = 0, inhibited = 0;

  /* Schwelle 0 = keine eingegeben, also nichts zu warnen — kein Inhibit. */
  if (state.Ufc.H.Readable() && state.Ufc.AlowFt > 0.0f) {
    if (!state.RadarAlt.H.Readable()) inhibited |= FBWarnAlow;
    else if (state.RadarAlt.AglFt < state.Ufc.AlowFt) active |= FBWarnAlow;
  }

  /* BINGO gegen die EFFEKTIVE Schwelle: das DED zeigt, was der Pilot tippte, gewarnt wird an der
   * Systemobergrenze (doc/modules/f16/controls-commands.md §6.8). */
  if (state.Ufc.H.Readable() && state.Ufc.BingoEffectiveLbs > 0.0f) {
    if (!state.Airframe.H.Readable()) inhibited |= FBWarnBingo;
    else if (state.Airframe.FuelLbs <= state.Ufc.BingoEffectiveLbs) active |= FBWarnBingo;
  }

  /* Die eine Bedingung aus EINEM Block — der Kontrollfall gegen die zwei Fusionen darueber. */
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

} // namespace FlightBox::Systems
