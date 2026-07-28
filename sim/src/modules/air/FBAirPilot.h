/* FlightBox — FBAirPilot: a catalogue aircraft's pilot, and the whole class is two states plus a row's
 * own numbers.
 *
 * WHY IT IS THIS SMALL. pilot/FBPilot is already generic and every airframe number in it is a VIRTUAL
 * HOOK (doc/pilot.md: *airframe numbers are hooks, pilot numbers are not*). So a TIER is not a class —
 * it is (a) which `set task` values the module accepts, (b) which sensor slots it powers and (c) which
 * of its own MEASURED hooks it can fill. Four of the five tiers are FBPilot unchanged.
 *
 * WHAT IT ADDS: `Orbit` (T0 — hold the station the mission declared and react to nothing) and `Drag`
 * (T1 — on this aircraft's OWN warning receiver reporting an airborne fire control, turn to the
 * reciprocal of the REPORTED bearing at max speed for `drag_threat_s`, then resume). One trigger, one
 * measured signal, one timer. The reflex is a SENSOR and not a clock: a silent attacker produces no
 * report and therefore no drag, which is the measurement doc/modules/air/module.md §Spec 11 criterion 8
 * asks for.
 *
 * WHAT NO TIER GETS, deliberately: a reaction to a surface-to-air launch. That is
 * doc/modules/ground/module.md G11, it is open for the F-16 and the MiG-29 too, and giving it to a
 * catalogue cell first would be building the behaviour where it is least measurable. */
#ifndef FBAIRPILOT_H
#define FBAIRPILOT_H

#include "FBAircraft.h"
#include "FBPilot.h"

namespace FlightBox::Modules {

class FBAirPilot : public Pilot::FBPilot {
public:
  void Configure(const FBAircraftSpec &spec) { Spec_ = &spec; }

  /* `0` = never, and it is the HONEST way to declare an asset that was ordered to hold its orbit —
   * how long a high-value asset runs before it turns back is a commander's rule, not a property of the
   * airframe, which is why it is a mission key. */
  void SetDragThreatS(double s) { DragThreatS_ = s; }
  double DragThreatS() const { return DragThreatS_; }

  /* What the module's mover has to fly this tick when the reflex is running. -1 = not dragging. */
  double DragHeadingDeg() const { return Dragging_ ? DragHdgDeg_ : -1.0; }
  bool Dragging() const { return Dragging_; }
  double DragElapsedS() const { return Dragging_ ? DragElapsedS_ : 0.0; }

  Pilot::FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                             const Systems::FBAirframeControls &airframe, const Fdm::fb_fdm_state &st,
                             const FBFlightPlan &plan, const FBRunway *runway, double dt) override;

protected:
  /* ---- THE ROW'S OWN NUMBERS, and nothing else is overridden. Every one of them is a MEASUREMENT on
   * this row's generated deck (recipe step 7) or a published limit; a row that has not been through
   * step 7 leaves the roll plant at zero, and its module then refuses `set task bfm` at boot rather
   * than flying the close-combat law with another aircraft's plant. ---- */
  double BfmCornerSpeedKt() const override { return Own(Spec_ ? Spec_->Perf.CornerKt : 0.0, FBPilot::BfmCornerSpeedKt()); }
  double BfmCornerG() const override { return Own(Spec_ ? Spec_->Perf.CornerG : 0.0, FBPilot::BfmCornerG()); }
  double BfmMaxG() const override { return Own(Spec_ ? Spec_->Perf.MaxG : 0.0, FBPilot::BfmMaxG()); }
  double BfmMinSpeedKt() const override { return Own(Spec_ ? Spec_->Perf.MinSpeedKt : 0.0, FBPilot::BfmMinSpeedKt()); }
  double BfmRollPlantA() const override { return Own(Spec_ ? Spec_->Perf.RollPlantA : 0.0, FBPilot::BfmRollPlantA()); }
  double BfmRollPlantKDegS() const override {
    return Own(Spec_ ? Spec_->Perf.RollPlantKDegS : 0.0, FBPilot::BfmRollPlantKDegS());
  }
  double ClimbSpeedKt() const override { return Own(Spec_ ? Spec_->Perf.ClimbSpeedKt : 0.0, FBPilot::ClimbSpeedKt()); }
  double ApproachSpeedKt() const override { return Own(Spec_ ? Spec_->Perf.ApproachKt : 0.0, FBPilot::ApproachSpeedKt()); }
  double InterceptSpeedKt() const override { return Own(Spec_ ? Spec_->Perf.ClimbSpeedKt : 0.0, FBPilot::InterceptSpeedKt()); }

private:
  /* An UNMEASURED hook is zero and falls back to the base's generic placeholder — which is honest
   * (nothing about this row is claimed) and is exactly the state the tier gate reads. */
  static double Own(double own, double fallback) { return own > 0.0 ? own : fallback; }

  const FBAircraftSpec *Spec_ = nullptr;
  double DragThreatS_ = 0.0;
  double DragHdgDeg_ = 0.0;
  double DragElapsedS_ = 0.0;
  bool   Dragging_ = false;
  Pilot::FBPilot::Phase Resume_ = Pilot::FBPilot::Phase::Route;
};

} // namespace FlightBox::Modules
#endif
