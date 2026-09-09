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

namespace Says {
constexpr auto CurvatureJump =
    "adjacent segments require continuous curvature; use a spiral transition";
constexpr auto InvalidPose = "reference-line starting pose must be finite";
constexpr auto InvalidSegment =
    "segment requires a known curve type, finite curvatures and positive finite length";
constexpr auto InvalidEndpoint =
    "segment endpoint and increasing accumulated station must be representable";
constexpr auto NonfiniteProfile = "profile stations, values and rates must be finite";
}

namespace {

bool Finite(const Knot &knot) {
  return std::isfinite(knot.AlongM) && std::isfinite(knot.Value) && std::isfinite(knot.RatePerM);
}

bool Finite(const Placed &pose) {
  const std::array values{pose.EastM,
                          pose.NorthM,
                          pose.HeightM,
                          pose.HeadingRad,
                          pose.CurvaturePerM,
                          pose.CurvatureRatePerM,
                          pose.Slope,
                          pose.SlopeRatePerM,
                          pose.BankRad,
                          pose.BankRatePerM};
  return std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
}

bool Valid(const Segment &segment) {
  const std::array shapes{Curve::Straight, Curve::Arc, Curve::Spiral};
  return std::ranges::find(shapes, segment.Shape) != shapes.end() &&
         std::isfinite(segment.LengthM) && segment.LengthM > 0.0 &&
         std::isfinite(segment.EntryCurvature) && std::isfinite(segment.ExitCurvature);
}

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

}

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
  if (!std::ranges::all_of(through, [](const Knot &knot) { return Finite(knot); })) {
    error = Says::NonfiniteProfile;
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
  Error_.clear();
  error.clear();
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

std::optional<double> ReferenceLine::MaxAbsCurvaturePerM(double fromM, double toM) const noexcept {
  if (Laid_.empty() || !std::isfinite(Length_) || !std::isfinite(fromM) || !std::isfinite(toM) ||
      fromM < 0.0 || toM < fromM || toM > Length_) {
    return std::nullopt;
  }
  auto first = std::ranges::upper_bound(Laid_, fromM, {}, &Held::AlongM);
  if (first != Laid_.begin()) { --first; }
  double maximum = 0.0;
  for (auto at = first; at != Laid_.end() && at->AlongM <= toM; ++at) {
    const auto &segment = at->Declared;
    const double endM = at->AlongM + segment.LengthM;
    if (!std::isfinite(endM) || !(endM > at->AlongM) || !std::isfinite(segment.EntryCurvature) ||
        !std::isfinite(segment.ExitCurvature)) {
      return std::nullopt;
    }
    const double from = (std::max(fromM, at->AlongM) - at->AlongM) / segment.LengthM;
    const double to = (std::min(toM, endM) - at->AlongM) / segment.LengthM;
    const double entry = std::lerp(segment.EntryCurvature, segment.ExitCurvature, from);
    const double exit = std::lerp(segment.EntryCurvature, segment.ExitCurvature, to);
    maximum = std::max({maximum, std::abs(entry), std::abs(exit)});
  }
  return maximum;
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
  ReferenceLine candidate;
  if (!candidate.Build(from, along, error)) {
    Error_ = error;
    return false;
  }
  *this = std::move(candidate);
  error.clear();
  return true;
}

bool ReferenceLine::Build(const Placed &from, std::span<const Segment> along, std::string &error) {
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

  if (!Finite(from)) {
    error = Says::InvalidPose;
    return Refuse(error);
  }
  Placed at = from;
  Laid_.reserve(along.size());
  for (const Segment &declared : along) {
    if (!Append(at, declared, error)) { return false; }
  }
  End_ = at;
  return true;
}

bool ReferenceLine::Append(Placed &at, Segment declared, std::string &error) {
  if (!Valid(declared)) {
    error = Says::InvalidSegment;
    return Refuse(error);
  }
  if (declared.Shape == Curve::Straight) { declared.EntryCurvature = declared.ExitCurvature = 0.0; }
  if (declared.Shape == Curve::Arc) { declared.ExitCurvature = declared.EntryCurvature; }
  if (!Laid_.empty()) {
    const double leaving = Laid_.back().Declared.ExitCurvature;
    if (std::fabs(leaving - declared.EntryCurvature) > kTangentTolerance) {
      error = Says::CurvatureJump;
      return Refuse(error);
    }
  }
  const double endM = Length_ + declared.LengthM;
  const Placed end = Walk(at, declared, declared.LengthM);
  if (!std::isfinite(endM) || !(endM > Length_) || !Finite(end)) {
    error = Says::InvalidEndpoint;
    return Refuse(error);
  }
  Laid_.push_back({.Declared = declared, .Entry = at, .AlongM = Length_});
  at = end;
  Length_ = endM;
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
  const double strideM = (highM - lowM) / static_cast<double>(kResectionCoarseSteps);
  for (int step = 0; step <= kResectionCoarseSteps; ++step) {
    const double atM = lowM + static_cast<double>(step) * strideM;
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

  double lowBracket = bestM - strideM;
  double highBracket = bestM + strideM;
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
  if (!std::isfinite(alongM) || !(alongM >= 0.0) || alongM > Length_) { return false; }

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
  Placed candidate = Walk(held.Entry, held.Declared, alongM - held.AlongM);

  const Curving rise = Read(Rise_, alongM);
  candidate.HeightM = rise.Value;
  candidate.Slope = rise.Rate;
  candidate.SlopeRatePerM = rise.Bend;
  const Curving bank = Read(Bank_, alongM);
  candidate.BankRad = bank.Value;
  candidate.BankRatePerM = bank.Rate;
  if (!Finite(candidate)) { return false; }
  out = candidate;
  return true;
}

}
