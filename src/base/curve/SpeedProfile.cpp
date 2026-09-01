#include "SpeedProfile.h"

#include <cstddef>
#include <string>
#include <vector>

#include <cmath>

namespace outshine {

const char *SpeedProfile::NameOf(Held term) noexcept {
  static constexpr const char *const kNames[static_cast<size_t>(Held::kCount)] = {
      "free", "curvature", "slip", "ramp", "climb", "crest", "entry", "traction", "brake"};
  static_assert(kNames[static_cast<size_t>(Held::kCount) - 1] != nullptr,
                "every term that can bind the plan carries a name");
  return static_cast<size_t>(term) < static_cast<size_t>(Held::kCount)
             ? kNames[static_cast<size_t>(term)]
             : "free";
}

bool SpeedProfile::Over(const ReferenceLine &along,
                        const Envelope &within,
                        double stepM,
                        double entryMs,
                        std::string &error) {
  Held_.clear();
  Curvature_.clear();
  StepM_ = 0.0;
  LengthM_ = 0.0;
  Slowest_ = Bound{};
  SlowestBound_ = Bound{};
  Fastest_ = Bound{};
  Why_.clear();
  for (size_t at = 0; at < static_cast<size_t>(Held::kCount); ++at) { Bound_[at] = 0; }
  Bin_.fill(0);
  BinMs_ = 0.0;

  if (!(stepM > 0.0)) {
    error = "a speed profile is sampled at a positive step and this one asks for " +
            std::to_string(stepM);
    return false;
  }
  if (!(within.Grip > 0.0) || !(within.GravityMs2 > 0.0) || !(within.MassKg > 0.0) ||
      !(within.DriveN > 0.0) || !(within.BrakeN > 0.0) || !(within.DragArea > 0.0) ||
      within.AirDensity < 0.0) {
    error = "an envelope is a body standing in a world and not a set of limits: it declares a "
            "friction coefficient, the world's gravity, a mass, a driving force, a braking force, "
            "a drag area and an air density (0 is a vacuum, less is not air) -- this one leaves a "
            "body term at zero or the air below nothing";
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

  const auto whole = static_cast<size_t>(along.LengthM() / stepM);
  const bool onGrid = static_cast<double>(whole) * stepM == along.LengthM();
  const size_t samples = whole + (onGrid ? 1u : 2u);
  Held_.resize(samples, topMs);
  Why_.assign(samples, Held::Free);
  Curvature_.resize(samples, 0.0);
  StepM_ = stepM;
  LengthM_ = along.LengthM();

  const auto HeldAt = [&](const Placed &here, Held &by) {
    double held = topMs;
    Held holds = Held::Free;
    const double bend = std::fabs(here.CurvaturePerM);
    if (bend > 0.0) {
      const double turning = std::sqrt(lateralMs2 / bend);
      if (turning < held) {
        held = turning;
        holds = Held::Curvature;
      }
    }
    if (bend > 0.0 && within.CorneringNPerRad > 0.0 && within.HoldWithinM > 0.0 &&
        within.SettleS > 0.0) {
      const double slipped = std::cbrt(4.0 * within.CorneringNPerRad * within.HoldWithinM /
                                       (within.MassKg * bend * within.SettleS));
      if (slipped < held) {
        held = slipped;
        holds = Held::Slip;
      }
    }
    const double ramp = std::fabs(here.CurvatureRatePerM);
    if (ramp > 0.0 && within.HoldWithinM > 0.0 && within.SettleS > 0.0) {
      const double followedMs = std::cbrt(
          6.0 * within.HoldWithinM / (ramp * within.SettleS * within.SettleS * within.SettleS));
      if (followedMs < held) {
        held = followedMs;
        holds = Held::Ramp;
      }
    }
    const double climb = here.Slope;
    if (climb > 0.0) {
      const double left = within.DriveN - within.MassKg * within.GravityMs2 * climb;
      const double resistance = 0.5 * within.AirDensity * within.DragArea;
      const double heldMs = left > 0.0 && resistance > 0.0 ? std::sqrt(left / resistance) : 0.0;
      if (heldMs < held) {
        held = heldMs;
        holds = Held::Climb;
      }
    }
    const double crest = -here.SlopeRatePerM;
    if (crest > 0.0) {
      const double flying = std::sqrt(within.GravityMs2 / crest);
      if (flying < held) {
        held = flying;
        holds = Held::Crest;
      }
    }
    by = holds;
    return held;
  };

  for (size_t at = 0; at < samples; ++at) {
    Placed here;
    const double station =
        static_cast<double>(at) * stepM > LengthM_ ? LengthM_ : static_cast<double>(at) * stepM;
    if (!along.At(station, here)) {
      error = "the reference line places nothing at " + std::to_string(station) + " m of " +
              std::to_string(LengthM_);
      Held_.clear();
      Curvature_.clear();
      return false;
    }
    Curvature_[at] = here.CurvaturePerM;
    Held holds = Held::Free;
    Held_[at] = HeldAt(here, holds);
    Why_[at] = holds;
  }

  const auto ClampAround = [&](double where, double heldMs, Held by) {
    if (!(heldMs < topMs) || !(where >= 0.0)) { return; }
    const double reach = where / stepM;
    const auto below = static_cast<size_t>(reach);
    const size_t above = static_cast<double>(below) == reach ? below : below + 1;
    for (size_t at = below; at <= above && at < samples; ++at) {
      if (heldMs < Held_[at]) {
        Held_[at] = heldMs;
        Why_[at] = by;
      }
    }
  };

  const std::vector<double> seams = along.Seams();
  for (size_t which = 0; which + 1 < seams.size(); ++which) {
    const double from = seams[which];
    const double to = seams[which + 1];
    if (!(to > from)) { continue; }
    const double centre = 0.5 * (from + to);
    Placed head;
    Placed middle;
    if (!along.At(from, head) || !along.At(centre, middle)) { continue; }
    Placed tail = middle;
    tail.CurvaturePerM = 2.0 * middle.CurvaturePerM - head.CurvaturePerM;
    tail.CurvatureRatePerM = 2.0 * middle.CurvatureRatePerM - head.CurvatureRatePerM;
    tail.SlopeRatePerM = 2.0 * middle.SlopeRatePerM - head.SlopeRatePerM;

    Held byHead = Held::Free;
    Held byMiddle = Held::Free;
    Held byTail = Held::Free;
    const double atHead = HeldAt(head, byHead);
    const double atMiddle = HeldAt(middle, byMiddle);
    const double atTail = HeldAt(tail, byTail);
    ClampAround(from, atHead, byHead);
    ClampAround(centre, atMiddle, byMiddle);
    ClampAround(to, atTail, byTail);
  }

  const auto gapM = [&](size_t before) {
    return before + 2 < samples ? stepM : LengthM_ - static_cast<double>(samples - 2) * stepM;
  };
  if (entryMs < Held_[0]) {
    Held_[0] = entryMs;
    Why_[0] = Held::Entry;
  }
  for (size_t at = 1; at < samples; ++at) {
    const double reached = std::sqrt(Held_[at - 1] * Held_[at - 1] + 2.0 * accelMs2 * gapM(at - 1));
    if (reached < Held_[at]) {
      Held_[at] = reached;
      Why_[at] = Held::Traction;
    }
  }
  for (size_t at = samples - 1; at > 0; --at) {
    const double allowed = std::sqrt(Held_[at] * Held_[at] + 2.0 * brakeMs2 * gapM(at - 1));
    if (allowed < Held_[at - 1]) {
      Held_[at - 1] = allowed;
      Why_[at - 1] = Held::Brake;
    }
  }

  for (size_t at = 0; at < samples; ++at) {
    if (at == 0 || Held_[at] > Fastest_.Ms) {
      Fastest_.Ms = Held_[at];
      Fastest_.AtM =
          static_cast<double>(at) * stepM > LengthM_ ? LengthM_ : static_cast<double>(at) * stepM;
      Fastest_.By = Why_[at];
    }
  }
  BinMs_ = Fastest_.Ms / static_cast<double>(kSpeedBins);
  for (size_t at = 0; at < samples; ++at) {
    ++Bound_[static_cast<size_t>(Why_[at])];
    if (BinMs_ > 0.0) {
      const auto bin = static_cast<size_t>(Held_[at] / BinMs_);
      ++Bin_[bin < kSpeedBins ? bin : kSpeedBins - 1];
    }
    const double station =
        static_cast<double>(at) * stepM > LengthM_ ? LengthM_ : static_cast<double>(at) * stepM;
    if (at == 0 || Held_[at] < Slowest_.Ms) {
      Slowest_.Ms = Held_[at];
      Slowest_.AtM = station;
      Slowest_.By = Why_[at];
    }
    const bool geometric = IsGeometry(Why_[at]);
    if (geometric && (SlowestBound_.By == Held::Free || Held_[at] < SlowestBound_.Ms)) {
      SlowestBound_.Ms = Held_[at];
      SlowestBound_.AtM = station;
      SlowestBound_.By = Why_[at];
    }
  }
  return true;
}

double SpeedProfile::At(double alongM) const {
  if (Held_.empty()) { return 0.0; }
  if (!(alongM > 0.0)) { return Held_.front(); }
  if (alongM >= LengthM_) { return Held_.back(); }
  const double where = alongM / StepM_;
  const auto low = static_cast<size_t>(where);
  if (low + 1 >= Held_.size()) { return Held_.back(); }
  double part = where - static_cast<double>(low);
  if (low + 2 == Held_.size()) {
    const double tailM = LengthM_ - static_cast<double>(low) * StepM_;
    part = tailM > 0.0 ? (alongM - static_cast<double>(low) * StepM_) / tailM : 1.0;
  }
  return Held_[low] + part * (Held_[low + 1] - Held_[low]);
}

double SpeedProfile::Quantile(double share) const noexcept {
  if (Held_.empty() || BinMs_ <= 0.0) { return 0.0; }
  const auto want = static_cast<size_t>(share * static_cast<double>(Held_.size()));
  size_t seen = 0;
  for (size_t bin = 0; bin < kSpeedBins; ++bin) {
    seen += Bin_[bin];
    if (seen > want) { return (static_cast<double>(bin) + 0.5) * BinMs_; }
  }
  return static_cast<double>(kSpeedBins) * BinMs_;
}

size_t SpeedProfile::StationsUnder(double ms) const noexcept {
  if (Held_.empty() || BinMs_ <= 0.0) { return 0; }
  const auto upTo = static_cast<size_t>(ms / BinMs_);
  size_t under = 0;
  for (size_t bin = 0; bin < kSpeedBins && bin < upTo; ++bin) { under += Bin_[bin]; }
  return under;
}

} // namespace outshine
