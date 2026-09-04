#include <optional>
#include "Fit.h"
#include "Alignment.h"
#include "Angle.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>
#include <utility>
#include <string>

namespace outshine {

namespace {

struct Chord {
  size_t From = 0;
  size_t To = 0;
};

double AwayFromChordM(std::span<const double> points, size_t point, Chord of) {
  const double fromE = points[2 * of.From];
  const double fromN = points[2 * of.From + 1];
  const double toE = points[2 * of.To];
  const double toN = points[2 * of.To + 1];
  const double atE = points[2 * point];
  const double atN = points[2 * point + 1];
  const double runE = toE - fromE;
  const double runN = toN - fromN;
  const double runSquared = runE * runE + runN * runN;
  if (runSquared <= 0.0) {
    const double e = atE - fromE;
    const double n = atN - fromN;
    return std::sqrt(e * e + n * n);
  }
  double part = ((atE - fromE) * runE + (atN - fromN) * runN) / runSquared;
  part = std::max(part, 0.0);
  part = std::min(part, 1.0);
  const double e = atE - (fromE + part * runE);
  const double n = atN - (fromN + part * runN);
  return std::sqrt(e * e + n * n);
}

void KeepBetween(std::span<const double> points,
                 Chord whole,
                 double withinM,
                 std::vector<bool> &keep) {
  std::vector<std::pair<size_t, size_t>> spans;
  spans.reserve(64);
  spans.emplace_back(whole.From, whole.To);
  while (!spans.empty()) {
    const auto [from, to] = spans.back();
    spans.pop_back();
    if (to <= from + 1) { continue; }
    size_t worst = from;
    double worstM = 0.0;
    for (size_t point = from + 1; point < to; ++point) {
      const double awayM = AwayFromChordM(points, point, {.From = from, .To = to});
      if (awayM > worstM) {
        worstM = awayM;
        worst = point;
      }
    }
    if (worstM <= withinM) { continue; }
    keep[worst] = true;
    spans.emplace_back(from, worst);
    spans.emplace_back(worst, to);
  }
}

} // namespace

std::vector<double> Simplify(std::span<const double> eastNorthM, double withinM) {
  std::vector<size_t> kept;
  return Simplify(eastNorthM, withinM, kept);
}

std::vector<double>
Simplify(std::span<const double> eastNorthM, double withinM, std::vector<size_t> &kept) {
  const size_t points = eastNorthM.size() / 2;
  kept.clear();
  kept.reserve(points);
  if (points < 3 || !(withinM > 0.0)) {
    for (size_t point = 0; point < points; ++point) { kept.push_back(point); }
    return {eastNorthM.begin(), eastNorthM.end()};
  }
  std::vector<bool> keep(points, false);
  keep.front() = true;
  keep.back() = true;
  KeepBetween(eastNorthM, {.From = 0, .To = points - 1}, withinM, keep);

  std::vector<double> out;
  out.reserve(eastNorthM.size());
  for (size_t point = 0; point < points; ++point) {
    if (!keep[point]) { continue; }
    kept.push_back(point);
    out.push_back(eastNorthM[2 * point]);
    out.push_back(eastNorthM[2 * point + 1]);
  }
  return out;
}

Fitted Fit(std::span<const double> eastNorthM,
           double withinM,
           double tightestM,
           ReferenceLine &into,
           std::span<const double> withinAtM) {
  Fitted out;
  const size_t points = eastNorthM.size() / 2;
  out.Vertices = points;

  if (points < 2) {
    out.Error =
        "a corridor is fitted through 2..N vertices and this one carries " + std::to_string(points);
    return out;
  }
  if (!(withinM > 0.0)) {
    out.Error = "a fit is bounded by how far it may leave the vertices, and this one declares " +
                std::to_string(withinM) + " m";
    return out;
  }

  if (points < 3) {
    const double east = eastNorthM[2] - eastNorthM[0];
    const double north = eastNorthM[3] - eastNorthM[1];
    Placed from;
    from.EastM = eastNorthM[0];
    from.NorthM = eastNorthM[1];
    from.HeadingRad = std::atan2(north, east);
    const Segment only{.Shape = Curve::Straight,
                       .LengthM = std::sqrt(east * east + north * north),
                       .EntryCurvature = 0.0,
                       .ExitCurvature = 0.0};
    if (!into.Lay(from, std::span<const Segment>(&only, 1), out.Error)) { return out; }
    out.Straights = 1;
    out.LengthM = into.LengthM();
    out.Passes = 1;
    out.Laid = true;
    return out;
  }

  const auto aligned = Align(eastNorthM, withinM, tightestM, withinAtM);
  if (!aligned) {
    out.Error = aligned.error().Said;
    out.TightestDemandedM = aligned.error().DemandedM;
    out.TightestDemandedAtVertex = aligned.error().AtVertex;
    out.Undrivable = aligned.error().Undrivable;
    out.UndrivableAtM = static_cast<double>(aligned.error().AtVertex);
    return out;
  }
  const auto laid = LayAligned(eastNorthM, *aligned, into);
  if (!laid) {
    out.Error = laid.error().Said;
    out.TightestDemandedM = laid.error().DemandedM;
    out.TightestDemandedAtVertex = laid.error().AtVertex;
    out.Undrivable = laid.error().Undrivable;
    return out;
  }

  out.Passes = 1;
  out.Corners = aligned->Bends.size();
  out.Runs = aligned->Runs;
  out.TightestRadiusM = aligned->TightestRadiusM;
  out.TightestDemandedM = aligned->TightestRadiusM;
  out.LengthM = into.LengthM();
  out.Straights = laid->Straights;

  out.Laid = true;
  return out;
}

} // namespace outshine
