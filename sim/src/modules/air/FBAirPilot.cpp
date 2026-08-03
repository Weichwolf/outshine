#include "FBAirPilot.h"

#include <cmath>

#include "FBGeodesy.h"
#include "FBLog.h"

namespace FlightBox::Modules {

/* Latched at the SHOT and off the station that was selected the tick BEFORE it — FBStoresSystem
 * deselects an emptied rail inside Release(), so the round that just left is no longer the round the
 * block reports. What binds a shooter is what is FLYING, so the flag is set once per launch and stands
 * until the next one; the intercept machine only ever reads it while it is supporting its own shot. */
void FBAirPilot::LatchBinding(const FBState &state) {
  const FBStoresBlock &sms = state.Stores;
  if (!sms.H.Readable()) return;
  if (SeenReleases_ < 0) SeenReleases_ = sms.ReleasedCount;
  if (sms.ReleasedCount > SeenReleases_) {
    SeenReleases_ = sms.ReleasedCount;
    const FBStoreSpec *fired = FBStoreSpecOf(RailKind_);
    Bound_ = fired && fired->Guided && FBSeekerHandoverS(fired->Seeker, 0.0) < 0.0;
  }
  RailKind_ = sms.SelectedStation >= 1 && sms.SelectedStation <= kMaxStoreStations
                  ? (FBStoreKind)sms.Station[sms.SelectedStation - 1]
                  : FBStoreKind::None;
}

Pilot::FBPilotCommands FBAirPilot::Run(const FBState &state, FBCommandBus &avionics,
                                       const Systems::FBAirframeControls &airframe,
                                       const Fdm::fb_fdm_state &st, const FBFlightPlan &plan,
                                       const FBRunway *runway, double dt) {
  LatchBinding(state);

  /* ---- THE ONE REFLEX, and it is a SENSOR READ and not a clock. The trigger is this aircraft's own
   * warning receiver reporting an AIRBORNE FIRE CONTROL that is doing more than sweeping; the direction
   * is the REPORTED relative bearing, which is all an RWR ever has (no range, ever); the duration is
   * the commander's `drag_threat_s`. A silent attacker produces no report, therefore no drag — which is
   * the experiment, not a demonstration. */
  if (DragThreatS_ > 0.0) {
    if (Dragging_) {
      DragElapsedS_ += dt;
      if (DragElapsedS_ >= DragThreatS_) {
        Dragging_ = false;
        SetPhase(Resume_);
        FBLog::Info("air", "RESUME", {{"afterS", DragElapsedS_}});
      }
    } else if (state.Rwr.H.Readable() && state.Rwr.Powered) {
      for (int i = 0; i < state.Rwr.ThreatCount; i++) {
        const FBRwrThreat &t = state.Rwr.Threats[i];
        if (t.Kind != FBEmitterKind::AirborneFireControl && t.Kind != FBEmitterKind::MissileSeeker)
          continue;
        if (t.Mode == FBRwrThreatMode::Search) continue;
        Dragging_ = true;
        DragElapsedS_ = 0.0;
        /* THE RECIPROCAL OF THE REPORTED BEARING. Relative bearing plus own heading is the world
         * bearing to the threat; the drag heading is 180 deg from it. Nothing about the threat's
         * position, speed or identity enters — the receiver measured a direction and a power. */
        DragHdgDeg_ = std::fmod(st.yaw + t.BearingDeg + 180.0 + 720.0, 360.0);
        Resume_ = GetPhase() == Phase::Drag ? Phase::Route : GetPhase();
        SetPhase(Phase::Drag);
        FBLog::Info("air", "DRAG", {{"bearingDeg", (double)t.BearingDeg},
                                    {"kind", std::string(FBEmitterKindStr(t.Kind))},
                                    {"headingDeg", DragHdgDeg_}, {"forS", DragThreatS_}});
        break;
      }
    }
  }

  /* The two added states have no control law of their own: a mover has no controls, and the module
   * hands the heading straight to modules/air/FBAirMover. Returning the neutral command is therefore
   * the correct statement — the pilot is not touching anything. */
  if (GetPhase() == Phase::Orbit || GetPhase() == Phase::Drag) {
    /* THE COMMANDER IS STILL ANSWERED. This branch skips the phase machine, so it would also skip the
     * order inbox the base services — and an order that vanished without a line is the one refusal a
     * commander cannot see (doc/player-layer.md §9.6). */
    ConsumeOrders(state, avionics, st, plan);
    return Pilot::FBPilotCommands{};
  }

  return Pilot::FBPilot::Run(state, avionics, airframe, st, plan, runway, dt);
}

} // namespace FlightBox::Modules
