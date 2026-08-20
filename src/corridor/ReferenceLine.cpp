#include "ReferenceLine.h"

#include <cmath>

namespace outshine {

namespace {

constexpr size_t kNodes = 8;
constexpr double kAbscissa[kNodes] = {-0.9602898564975363, -0.7966664774136267,
                                      -0.5255324099163290, -0.1834346424956498,
                                      0.1834346424956498,  0.5255324099163290,
                                      0.7966664774136267,  0.9602898564975363};
constexpr double kWeight[kNodes] = {0.1012285362903763, 0.2223810344533745, 0.3137066458778873,
                                    0.3626837833783620, 0.3626837833783620, 0.3137066458778873,
                                    0.2223810344533745, 0.1012285362903763};

double HeadingAlong(const Segment &along, double byM) {
  const double rate = along.LengthM > 0.0
                          ? (along.ExitCurvature - along.EntryCurvature) / along.LengthM
                          : 0.0;
  return along.EntryCurvature * byM + 0.5 * rate * byM * byM;
}

} // namespace

bool ReferenceLine::Refuse(const std::string &why) {
  Error_ = why;
  Laid_.clear();
  Rise_.clear();
  Bank_.clear();
  Length_ = 0.0;
  return false;
}

bool ReferenceLine::Fasten(const std::vector<Knot> &through, const char *what, const char *unit,
                           std::vector<Knot> &into, std::string &error) {
  into.clear();
  if (Laid_.empty()) {
    error = std::string("a ") + what + " profile is fastened to a line that is laid, and this one "
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
              " m along a line " + std::to_string(Length_) +
              " m long, and a profile states its " + what + " over the line it is fastened to";
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
  into = through;
  return true;
}

bool ReferenceLine::Rise(const std::vector<Knot> &through, std::string &error) {
  return Fasten(through, "height", "slope", Rise_, error);
}

bool ReferenceLine::Bank(const std::vector<Knot> &through, std::string &error) {
  return Fasten(through, "bank", "rate", Bank_, error);
}

void ReferenceLine::Read(const std::vector<Knot> &through, double alongM, double &value,
                         double &rate, double &bend) {
  value = 0.0;
  rate = 0.0;
  bend = 0.0;
  if (through.empty()) { return; }
  if (alongM <= through.front().AlongM) {
    rate = through.front().RatePerM;
    value = through.front().Value + rate * (alongM - through.front().AlongM);
    return;
  }
  if (alongM >= through.back().AlongM) {
    rate = through.back().RatePerM;
    value = through.back().Value + rate * (alongM - through.back().AlongM);
    return;
  }

  size_t low = 0, high = through.size() - 1;
  while (low < high) {
    const size_t mid = (low + high + 1) / 2;
    if (through[mid].AlongM <= alongM) {
      low = mid;
    } else {
      high = mid - 1;
    }
  }
  const Knot &from = through[low];
  const Knot &to = through[low + 1];
  const double span = to.AlongM - from.AlongM;
  const double t = (alongM - from.AlongM) / span;
  const double tt = t * t;
  const double ttt = tt * t;

  value = (2.0 * ttt - 3.0 * tt + 1.0) * from.Value + (ttt - 2.0 * tt + t) * span * from.RatePerM +
          (-2.0 * ttt + 3.0 * tt) * to.Value + (ttt - tt) * span * to.RatePerM;
  rate = (6.0 * tt - 6.0 * t) / span * from.Value + (3.0 * tt - 4.0 * t + 1.0) * from.RatePerM +
         (-6.0 * tt + 6.0 * t) / span * to.Value + (3.0 * tt - 2.0 * t) * to.RatePerM;
  bend = (12.0 * t - 6.0) / (span * span) * from.Value + (6.0 * t - 4.0) / span * from.RatePerM +
         (-12.0 * t + 6.0) / (span * span) * to.Value + (6.0 * t - 2.0) / span * to.RatePerM;
}

Placed ReferenceLine::Walk(const Placed &from, const Segment &along, double byM) {
  Placed out;
  const double rate =
      along.LengthM > 0.0 ? (along.ExitCurvature - along.EntryCurvature) / along.LengthM : 0.0;
  out.CurvaturePerM = along.EntryCurvature + rate * byM;
  out.HeadingRad = from.HeadingRad + HeadingAlong(along, byM);

  if (along.Shape == Curve::Straight) {
    out.EastM = from.EastM + byM * std::cos(from.HeadingRad);
    out.NorthM = from.NorthM + byM * std::sin(from.HeadingRad);
    return out;
  }

  if (along.Shape == Curve::Arc && along.EntryCurvature != 0.0) {
    const double radius = 1.0 / along.EntryCurvature;
    const double turned = along.EntryCurvature * byM;
    out.EastM = from.EastM + radius * (std::sin(from.HeadingRad + turned) - std::sin(from.HeadingRad));
    out.NorthM = from.NorthM - radius * (std::cos(from.HeadingRad + turned) - std::cos(from.HeadingRad));
    return out;
  }

  double east = 0.0, north = 0.0;
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

bool ReferenceLine::Lay(const Placed &from, const std::vector<Segment> &along, std::string &error) {
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
    if (declared.Shape == Curve::Straight) { declared.EntryCurvature = declared.ExitCurvature = 0.0; }
    if (declared.Shape == Curve::Arc) { declared.ExitCurvature = declared.EntryCurvature; }

    if (which > 0) {
      const double leaving = Laid_.back().Declared.ExitCurvature;
      if (std::fabs(leaving - declared.EntryCurvature) > kTangentTolerance) {
        error = "segment " + std::to_string(which) + " enters at curvature " +
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

bool ReferenceLine::Nearest(double eastM, double northM, double nearM, double windowM,
                            double &alongM) const {
  if (Laid_.empty() || !(windowM > 0.0)) { return false; }

  double lowM = nearM - windowM;
  double highM = nearM + windowM;
  if (lowM < 0.0) { lowM = 0.0; }
  if (highM > Length_) { highM = Length_; }
  if (!(highM > lowM)) { return false; }

  const auto away = [eastM, northM](const Placed &there) {
    const double east = eastM - there.EastM;
    const double north = northM - there.NorthM;
    return east * east + north * north;
  };

  double bestM = lowM;
  double bestAway = 0.0;
  bool have = false;
  for (double atM = lowM; atM <= highM; atM += kResectionCoarseM) {
    Placed there;
    if (!At(atM, there)) { continue; }
    const double is = away(there);
    if (!have || is < bestAway) {
      have = true;
      bestAway = is;
      bestM = atM;
    }
  }
  if (!have) { return false; }

  double spanM = kResectionCoarseM;
  for (int narrow = 0; narrow < kResectionRefinements; ++narrow) {
    spanM *= 0.1;
    for (int side = -1; side <= 1; side += 2) {
      const double tryM = bestM + (double)side * spanM;
      if (tryM < lowM || tryM > highM) { continue; }
      Placed there;
      if (!At(tryM, there)) { continue; }
      const double is = away(there);
      if (is < bestAway) {
        bestAway = is;
        bestM = tryM;
      }
    }
  }
  alongM = bestM;
  return true;
}

bool ReferenceLine::At(double alongM, Placed &out) const {
  if (Laid_.empty()) { return false; }
  if (!(alongM >= 0.0) || alongM > Length_) { return false; }

  size_t low = 0, high = Laid_.size() - 1;
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

  double bend = 0.0;
  Read(Rise_, alongM, out.HeightM, out.Slope, out.SlopeRatePerM);
  Read(Bank_, alongM, out.BankRad, out.BankRatePerM, bend);
  return true;
}

} // namespace outshine
