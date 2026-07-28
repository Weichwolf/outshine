#include "FBAirPilot.h"

#include <cmath>

#include "FBGeodesy.h"
#include "FBLog.h"

namespace FlightBox::Modules {

Pilot::FBPilotCommands FBAirPilot::Run(const FBState &state, FBCommandBus &avionics,
                                       const Systems::FBAirframeControls &airframe,
                                       const Fdm::fb_fdm_state &st, const FBFlightPlan &plan,
                                       const FBRunway *runway, double dt) {
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
  if (GetPhase() == Phase::Orbit || GetPhase() == Phase::Drag) return Pilot::FBPilotCommands{};

  return Pilot::FBPilot::Run(state, avionics, airframe, st, plan, runway, dt);
}

} // namespace FlightBox::Modules
