#include "ReferenceLine.h"

#include <array>
#include <algorithm>

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <span>
#include <vector>

namespace outshine {

constexpr double kCubicRateFactor = 3.0;
constexpr double kSquareRateFactor = 2.0;

namespace {

constexpr size_t kNodes = 8;
constexpr std::array<double, kNodes> kAbscissa = {{-0.9602898564975363,
                                                   -0.7966664774136267,
                                                   -0.5255324099163290,
                                                   -0.1834346424956498,
                                                   0.1834346424956498,
                                                   0.5255324099163290,
                                                   0.7966664774136267,
                                                   0.9602898564975363}};
constexpr std::array<double, kNodes> kWeight = {{0.1012285362903763,
                                                 0.2223810344533745,
                                                 0.3137066458778873,
                                                 0.3626837833783620,
                                                 0.3626837833783620,
                                                 0.3137066458778873,
                                                 0.2223810344533745,
                                                 0.1012285362903763}};

double HeadingAlong(const Segment &along, double byM) {
  const double rate =
      along.LengthM > 0.0 ? (along.ExitCurvature - along.EntryCurvature) / along.LengthM : 0.0;
  return along.EntryCurvature * byM + 0.5 * rate * byM * byM;
}

} // namespace

bool ReferenceLine::Refuse(std::string why) {
  Error_ = std::move(why);
  Laid_.clear();
  Rise_.clear();
  Bank_.clear();
  Length_ = 0.0;
  return false;
}

bool ReferenceLine::Fasten(std::span<const Knot> through,
                           const char *what,
                           const char *unit,
                           std::vector<Knot> &into,
                           std::string &error) {
  into.clear();
  if (Laid_.empty()) {
    error = std::string("a ") + what +
            " profile is fastened to a line that is laid, and this one "
            "carries no segments to fasten it to";
    Error_ = error;
    return false;
  }
  if (through.size() > kMaxCorridorKnots) {
    error = std::string("a ") + what + " profile of " + std::to_string(through.size()) +
            " knots reaches the bound of " + std::to_string(kMaxCorridorKnots);
    Error_ = error;
    return false;
  }
  for (size_t which = 0; which < through.size(); ++which) {
    const Knot &knot = through[which];
    if (!(knot.AlongM >= 0.0) || knot.AlongM > Length_ + kTangentTolerance) {
      error = std::string("a ") + what + " knot stands at " + std::to_string(knot.AlongM) +
              " m along a line " + std::to_string(Length_) + " m long, and a profile states its " +
              what + " over the line it is fastened to";
      Error_ = error;
      return false;
    }
    if (which > 0 && !(knot.AlongM > through[which - 1].AlongM)) {
      error = std::string("a ") + what + " knot at " + std::to_string(knot.AlongM) +
              " m follows one at " + std::to_string(through[which - 1].AlongM) +
              " m, and a station a line reaches once carries one " + what + " and one " + unit;
      Error_ = error;
      return false;
    }
  }
  into.assign(through.begin(), through.end());
  return true;
}

bool ReferenceLine::Rise(std::span<const Knot> through, std::string &error) {
  return Fasten(through, "height", "slope", Rise_, error);
}

bool ReferenceLine::Bank(std::span<const Knot> through, std::string &error) {
  return Fasten(through, "bank", "rate", Bank_, error);
}

Curving ReferenceLine::Read(std::span<const Knot> through, double alongM) {
  Curving out;
  if (through.empty()) { return out; }
  if (through.size() < 2 || alongM < through.front().AlongM) {
    out.Rate = through.front().RatePerM;
    out.Value = through.front().Value + out.Rate * (alongM - through.front().AlongM);
    return out;
  }
  if (alongM > through.back().AlongM) {
    out.Rate = through.back().RatePerM;
    out.Value = through.back().Value + out.Rate * (alongM - through.back().AlongM);
    return out;
  }

  size_t low = 0;
  size_t high = through.size() - 1;
  while (low < high) {
    const size_t mid = (low + high + 1) / 2;
    if (through[mid].AlongM <= alongM) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }
  if (low + 1 >= through.size()) { low = through.size() - 2; }
  const Knot &from = through[low];
  const Knot &to = through[low + 1];
  const double span = to.AlongM - from.AlongM;
  const double t = (alongM - from.AlongM) / span;

  const double slopeFrom = span * from.RatePerM;
  const double slopeTo = span * to.RatePerM;
  const double cubed = kSquareRateFactor * (from.Value - to.Value) + slopeFrom + slopeTo;
  const double squared =
      kCubicRateFactor * (to.Value - from.Value) - kSquareRateFactor * slopeFrom - slopeTo;
  const double linear = slopeFrom;
  const double constant = from.Value;

  out.Value = ((cubed * t + squared) * t + linear) * t + constant;
  out.Rate = ((kCubicRateFactor * cubed * t + kSquareRateFactor * squared) * t + linear) / span;
  out.Bend = (kCubicRateFactor * kSquareRateFactor * cubed * t + kSquareRateFactor * squared) /
             (span * span);
  return out;
}

std::vector<double> ReferenceLine::Seams() const {
  std::vector<double> at;
  at.reserve(Laid_.size() + Rise_.size() + Bank_.size());
  for (const Held &one : Laid_) { at.push_back(one.AlongM); }
  for (const Knot &one : Rise_) { at.push_back(one.AlongM); }
  for (const Knot &one : Bank_) { at.push_back(one.AlongM); }
  at.push_back(Length_);
  std::ranges::sort(at);
  at.erase(std::ranges::unique(at).begin(), at.end());
  return at;
}

Placed ReferenceLine::Walk(const Placed &from, const Segment &along, double byM) {
  Placed out;
  const double rate =
      along.LengthM > 0.0 ? (along.ExitCurvature - along.EntryCurvature) / along.LengthM : 0.0;
  out.CurvaturePerM = along.EntryCurvature + rate * byM;
  out.CurvatureRatePerM = rate;
  out.HeadingRad = from.HeadingRad + HeadingAlong(along, byM);

  if (along.Shape == Curve::Straight) {
    out.EastM = from.EastM + byM * std::cos(from.HeadingRad);
    out.NorthM = from.NorthM + byM * std::sin(from.HeadingRad);
    return out;
  }

  if (along.Shape == Curve::Arc && along.EntryCurvature != 0.0) {
    const double radius = 1.0 / along.EntryCurvature;
    const double turned = along.EntryCurvature * byM;
    out.EastM =
        from.EastM + radius * (std::sin(from.HeadingRad + turned) - std::sin(from.HeadingRad));
    out.NorthM =
        from.NorthM - radius * (std::cos(from.HeadingRad + turned) - std::cos(from.HeadingRad));
    return out;
  }

  double east = 0.0;
  double north = 0.0;
  const double half = 0.5 * byM;
  for (size_t node = 0; node < kNodes; ++node) {
    const double at = half * (kAbscissa[node] + 1.0);
    const double heading = from.HeadingRad + HeadingAlong(along, at);
    east += kWeight[node] * std::cos(heading);
    north += kWeight[node] * std::sin(heading);
  }
  out.EastM = from.EastM + half * east;
  out.NorthM = from.NorthM + half * north;
  return out;
}

bool ReferenceLine::Lay(const Placed &from, std::span<const Segment> along, std::string &error) {
  Error_.clear();
  Laid_.clear();
  Rise_.clear();
  Bank_.clear();
  Length_ = 0.0;
  End_ = from;

  if (along.empty()) {
    error = "a reference line is laid from 1..N segments and this one declares none";
    return Refuse(error);
  }
  if (along.size() > kMaxCorridorSegments) {
    error = "a reference line of " + std::to_string(along.size()) +
            " segments reaches the bound of " + std::to_string(kMaxCorridorSegments);
    return Refuse(error);
  }

  Placed at = from;
  Laid_.reserve(along.size());
  for (size_t which = 0; which < along.size(); ++which) {
    Segment declared = along[which];
    if (!(declared.LengthM > 0.0)) {
      error = "segment " + std::to_string(which) + " is " + std::to_string(declared.LengthM) +
              " m long, and a segment of no length places nothing";
      return Refuse(error);
    }
    if (declared.Shape == Curve::Straight) {
      declared.EntryCurvature = declared.ExitCurvature = 0.0;
    }
    if (declared.Shape == Curve::Arc) { declared.ExitCurvature = declared.EntryCurvature; }

    if (which > 0) {
      const double leaving = Laid_.back().Declared.ExitCurvature;
      if (std::fabs(leaving - declared.EntryCurvature) > kTangentTolerance) {
        error =
            "segment " + std::to_string(which) + " enters at curvature " +
            std::to_string(declared.EntryCurvature) + " where segment " +
            std::to_string(which - 1) + " leaves at " + std::to_string(leaving) +
            ", and a leap in curvature is a step in the lateral force -- a spiral is what carries "
            "a transition without one";
        return Refuse(error);
      }
    }

    Held held;
    held.Declared = declared;
    held.Entry = at;
    held.AlongM = Length_;
    Laid_.push_back(held);

    at = Walk(at, declared, declared.LengthM);
    Length_ += declared.LengthM;
  }
  End_ = at;
  return true;
}

std::optional<double> ReferenceLine::Nearest(EastNorth at, Nearby about) const {
  if (Laid_.empty() || !(about.WithinM > 0.0)) { return std::nullopt; }

  double lowM = about.AboutM - about.WithinM;
  double highM = about.AboutM + about.WithinM;
  lowM = std::max(lowM, 0.0);
  highM = std::min(highM, Length_);
  if (!(highM > lowM)) { return std::nullopt; }

  const auto away = [at](const Placed &there) {
    const double east = at.EastM - there.EastM;
    const double north = at.NorthM - there.NorthM;
    return east * east + north * north;
  };

  double bestM = lowM;
  double bestAway = 0.0;
  bool have = false;
  const auto steps = static_cast<size_t>((highM - lowM) / kResectionCoarseM);
  for (size_t step = 0; step <= steps; ++step) {
    const double atM = lowM + static_cast<double>(step) * kResectionCoarseM;
    Placed there;
    if (!At(atM, there)) { continue; }
    const double is = away(there);
    if (!have || is < bestAway) {
      have = true;
      bestAway = is;
      bestM = atM;
    }
  }
  if (!have) { return std::nullopt; }

  double lowBracket = bestM - kResectionCoarseM;
  double highBracket = bestM + kResectionCoarseM;
  lowBracket = std::max(lowBracket, lowM);
  highBracket = std::min(highBracket, highM);

  const double shrink = 0.6180339887498949;
  double leftM = highBracket - shrink * (highBracket - lowBracket);
  double rightM = lowBracket + shrink * (highBracket - lowBracket);
  const auto awayAt = [this, &away](double atM, double &into) {
    Placed there;
    if (!At(atM, there)) { return false; }
    into = away(there);
    return true;
  };
  double leftAway = 0.0;
  double rightAway = 0.0;
  if (awayAt(leftM, leftAway) && awayAt(rightM, rightAway)) {
    for (int narrow = 0; narrow < kResectionRefinements; ++narrow) {
      if (leftAway < rightAway) {
        highBracket = rightM;
        rightM = leftM;
        rightAway = leftAway;
        leftM = highBracket - shrink * (highBracket - lowBracket);
        if (!awayAt(leftM, leftAway)) { break; }
      } else {
        lowBracket = leftM;
        leftM = rightM;
        leftAway = rightAway;
        rightM = lowBracket + shrink * (highBracket - lowBracket);
        if (!awayAt(rightM, rightAway)) { break; }
      }
    }
    bestM = 0.5 * (lowBracket + highBracket);
  }

  return bestM;
}

bool ReferenceLine::At(double alongM, Placed &out) const {
  if (Laid_.empty()) { return false; }
  if (!(alongM >= 0.0) || alongM > Length_) { return false; }

  size_t low = 0;
  size_t high = Laid_.size() - 1;
  while (low < high) {
    const size_t mid = (low + high + 1) / 2;
    if (Laid_[mid].AlongM <= alongM) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }
  const Held &held = Laid_[low];
  out = Walk(held.Entry, held.Declared, alongM - held.AlongM);

  const Curving rise = Read(Rise_, alongM);
  out.HeightM = rise.Value;
  out.Slope = rise.Rate;
  out.SlopeRatePerM = rise.Bend;
  const Curving bank = Read(Bank_, alongM);
  out.BankRad = bank.Value;
  out.BankRatePerM = bank.Rate;
  return true;
}

} // namespace outshine
