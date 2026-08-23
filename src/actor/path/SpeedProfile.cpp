#include "SpeedProfile.h"

#include <vector>

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
      within.AirDensity < 0.0) {
    error = "an envelope is a vehicle standing in a world and not a set of limits: it declares a "
            "friction coefficient, the world's gravity, a mass, a driving force, a braking force, "
            "a drag area and an air density (0 is a vacuum, less is not air) -- this one leaves a "
            "vehicle term at zero or the air below nothing";
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

  const size_t whole = (size_t)(along.LengthM() / stepM);
  const bool onGrid = (double)whole * stepM == along.LengthM();
  const size_t samples = whole + (onGrid ? 1u : 2u);
  Held_.resize(samples, topMs);
  Curvature_.resize(samples, 0.0);
  StepM_ = stepM;
  LengthM_ = along.LengthM();

  const auto NoteCrest = [&](const Placed &here, double station, double bindingMs) {
    const double crest = -here.SlopeRatePerM;
    if (!(crest > 0.0)) { return; }
    const double flying = std::sqrt(within.GravityMs2 / crest);
    if (flying > bindingMs) { return; }
    ++CrestsBound_;
    if (CrestHeld_ <= 0.0 || flying < CrestHeld_) {
      CrestHeld_ = flying;
      CrestHeldAt_ = station;
    }
  };

  const auto HeldAt = [&](const Placed &here) {
    double held = topMs;
    const double bend = std::fabs(here.CurvaturePerM);
    if (bend > 0.0) {
      const double turning = std::sqrt(lateralMs2 / bend);
      if (turning < held) { held = turning; }
    }
    if (bend > 0.0 && within.CorneringNPerRad > 0.0 && within.HoldWithinM > 0.0 &&
        within.SettleS > 0.0) {
      const double slipped = std::cbrt(4.0 * within.CorneringNPerRad * within.HoldWithinM /
                                       (within.MassKg * bend * within.SettleS));
      if (slipped < held) { held = slipped; }
    }
    const double ramp = std::fabs(here.CurvatureRatePerM);
    if (ramp > 0.0 && within.HoldWithinM > 0.0 && within.SettleS > 0.0) {
      const double followedMs =
          std::cbrt(6.0 * within.HoldWithinM /
                    (ramp * within.SettleS * within.SettleS * within.SettleS));
      if (followedMs < held) { held = followedMs; }
    }
    const double climb = here.Slope;
    if (climb > 0.0) {
      const double left = within.DriveN - within.MassKg * within.GravityMs2 * climb;
      const double resistance = 0.5 * within.AirDensity * within.DragArea;
      const double heldMs = left > 0.0 && resistance > 0.0 ? std::sqrt(left / resistance) : 0.0;
      if (heldMs < held) { held = heldMs; }
    }
    const double crest = -here.SlopeRatePerM;
    if (crest > 0.0) {
      const double flying = std::sqrt(within.GravityMs2 / crest);
      if (flying < held) { held = flying; }
    }
    return held;
  };

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
    Held_[at] = HeldAt(here);
    NoteCrest(here, station, Held_[at]);
  }

  const std::vector<double> seams = along.Seams();
  for (size_t which = 0; which + 1 < seams.size(); ++which) {
    const double from = seams[which];
    const double to = seams[which + 1];
    if (!(to > from)) { continue; }
    Placed head, middle;
    if (!along.At(from, head) || !along.At(0.5 * (from + to), middle)) { continue; }
    Placed tail = middle;
    tail.CurvaturePerM = 2.0 * middle.CurvaturePerM - head.CurvaturePerM;
    tail.CurvatureRatePerM = 2.0 * middle.CurvatureRatePerM - head.CurvatureRatePerM;
    tail.SlopeRatePerM = 2.0 * middle.SlopeRatePerM - head.SlopeRatePerM;
    const double bound =
        std::fmin(HeldAt(head), std::fmin(HeldAt(middle), HeldAt(tail)));
    const size_t first = (size_t)(from / stepM);
    const double reach = to / stepM;
    const size_t last = (size_t)reach == reach ? (size_t)reach : (size_t)reach + 1;
    for (size_t at = first; at <= last && at < samples; ++at) {
      if (bound < Held_[at]) { Held_[at] = bound; }
    }
    NoteCrest(head, from, bound);
    NoteCrest(middle, 0.5 * (from + to), bound);
    NoteCrest(tail, to, bound);
  }

  const auto gapM = [&](size_t before) {
    return before + 2 < samples ? stepM : LengthM_ - (double)(samples - 2) * stepM;
  };
  Held_[0] = entryMs < Held_[0] ? entryMs : Held_[0];
  for (size_t at = 1; at < samples; ++at) {
    const double reached =
        std::sqrt(Held_[at - 1] * Held_[at - 1] + 2.0 * accelMs2 * gapM(at - 1));
    if (reached < Held_[at]) { Held_[at] = reached; }
  }
  for (size_t at = samples - 1; at > 0; --at) {
    const double allowed = std::sqrt(Held_[at] * Held_[at] + 2.0 * brakeMs2 * gapM(at - 1));
    if (allowed < Held_[at - 1]) { Held_[at - 1] = allowed; }
  }
  return true;
}

double SpeedProfile::At(double alongM) const {
  if (Held_.empty()) { return 0.0; }
  if (!(alongM > 0.0)) { return Held_.front(); }
  if (alongM >= LengthM_) { return Held_.back(); }
  const double where = alongM / StepM_;
  size_t low = (size_t)where;
  if (low + 1 >= Held_.size()) { return Held_.back(); }
  double part = where - (double)low;
  if (low + 2 == Held_.size()) {
    const double tailM = LengthM_ - (double)low * StepM_;
    part = tailM > 0.0 ? (alongM - (double)low * StepM_) / tailM : 1.0;
  }
  return Held_[low] + part * (Held_[low + 1] - Held_[low]);
}

}
