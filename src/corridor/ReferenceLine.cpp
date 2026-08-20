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
  Length_ = 0.0;
  return false;
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
  return true;
}

} // namespace outshine
