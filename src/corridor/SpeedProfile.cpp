#include "SpeedProfile.h"

#include <cmath>

namespace outshine {

bool SpeedProfile::Over(const ReferenceLine &along, const Envelope &within, double stepM,
                        double entryMs, std::string &error) {
  Held_.clear();
  Curvature_.clear();
  StepM_ = 0.0;
  LengthM_ = 0.0;
  CrestHeld_ = 0.0;
  CrestHeldAt_ = 0.0;
  CrestsBound_ = 0;

  if (!(stepM > 0.0)) {
    error = "a speed profile is sampled at a positive step and this one asks for " +
            std::to_string(stepM);
    return false;
  }
  if (!(within.Grip > 0.0) || !(within.GravityMs2 > 0.0) || !(within.MassKg > 0.0) ||
      !(within.DriveN > 0.0) || !(within.BrakeN > 0.0) || !(within.DragArea > 0.0) ||
      !(within.AirDensity > 0.0)) {
    error = "an envelope is a vehicle standing in a world and not a set of limits: it declares a "
            "friction coefficient, the world's gravity, a mass, a driving force, a braking force, "
            "a drag area and an air density, and every acceleration is derived from those -- this "
            "one leaves at least one at zero";
    return false;
  }
  const double lateralMs2 = within.HoldingMs2();
  const double accelMs2 = within.AccelMs2();
  const double brakeMs2 = within.BrakeMs2();
  const double topMs = within.TopMs();
  if (!(along.LengthM() > 0.0)) {
    error = "a speed profile is taken over a reference line and this one has no length";
    return false;
  }

  const size_t samples = (size_t)(along.LengthM() / stepM) + 1u;
  Held_.resize(samples, topMs);
  Curvature_.resize(samples, 0.0);
  StepM_ = stepM;
  LengthM_ = along.LengthM();

  for (size_t at = 0; at < samples; ++at) {
    Placed here;
    const double station = (double)at * stepM > LengthM_ ? LengthM_ : (double)at * stepM;
    if (!along.At(station, here)) {
      error = "the reference line places nothing at " + std::to_string(station) + " m of " +
              std::to_string(LengthM_);
      Held_.clear();
      Curvature_.clear();
      return false;
    }
    Curvature_[at] = here.CurvaturePerM;
    const double bend = std::fabs(here.CurvaturePerM);
    if (bend > 0.0) {
      const double held = std::sqrt(lateralMs2 / bend);
      Held_[at] = held < topMs ? held : topMs;
    }
    if (bend > 0.0 && within.CorneringNPerRad > 0.0 && within.HoldWithinM > 0.0 &&
        within.SettleS > 0.0) {
      const double slipped = std::cbrt(4.0 * within.CorneringNPerRad * within.HoldWithinM /
                                       (within.MassKg * bend * within.SettleS));
      if (slipped < Held_[at]) { Held_[at] = slipped; }
    }
    const double ramp = std::fabs(here.CurvatureRatePerM);
    if (ramp > 0.0 && within.HoldWithinM > 0.0 && within.SettleS > 0.0) {
      const double followedMs =
          std::cbrt(6.0 * within.HoldWithinM /
                    (ramp * within.SettleS * within.SettleS * within.SettleS));
      if (followedMs < Held_[at]) { Held_[at] = followedMs; }
    }
    const double climb = here.Slope;
    if (climb > 0.0) {
      const double left = within.DriveN - within.MassKg * within.GravityMs2 * climb;
      const double resistance = 0.5 * within.AirDensity * within.DragArea;
      const double heldMs = left > 0.0 && resistance > 0.0 ? std::sqrt(left / resistance) : 0.0;
      if (heldMs < Held_[at]) { Held_[at] = heldMs; }
    }
    const double crest = -here.SlopeRatePerM;
    if (crest > 0.0) {
      const double flying = std::sqrt(within.GravityMs2 / crest);
      if (flying < Held_[at]) {
        Held_[at] = flying;
        ++CrestsBound_;
        if (CrestHeld_ <= 0.0 || flying < CrestHeld_) {
          CrestHeld_ = flying;
          CrestHeldAt_ = station;
        }
      }
    }
  }

  Held_[0] = entryMs < Held_[0] ? entryMs : Held_[0];
  for (size_t at = 1; at < samples; ++at) {
    const double reached =
        std::sqrt(Held_[at - 1] * Held_[at - 1] + 2.0 * accelMs2 * stepM);
    if (reached < Held_[at]) { Held_[at] = reached; }
  }
  for (size_t at = samples - 1; at > 0; --at) {
    const double allowed = std::sqrt(Held_[at] * Held_[at] + 2.0 * brakeMs2 * stepM);
    if (allowed < Held_[at - 1]) { Held_[at - 1] = allowed; }
  }
  return true;
}

double SpeedProfile::At(double alongM) const {
  if (Held_.empty()) { return 0.0; }
  if (!(alongM > 0.0)) { return Held_.front(); }
  if (alongM >= LengthM_) { return Held_.back(); }
  const double where = alongM / StepM_;
  const size_t low = (size_t)where;
  if (low + 1 >= Held_.size()) { return Held_.back(); }
  const double part = where - (double)low;
  return Held_[low] + part * (Held_[low + 1] - Held_[low]);
}

} // namespace outshine
