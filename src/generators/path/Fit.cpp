#include "Fit.h"
#include "Alignment.h"
#include "Angle.h"

#include <cmath>
#include <numbers>

namespace outshine {

namespace {

double AwayFromChordM(std::span<const double> points, size_t point, size_t from, size_t to) {
  const double fromE = points[2 * from], fromN = points[2 * from + 1];
  const double toE = points[2 * to], toN = points[2 * to + 1];
  const double atE = points[2 * point], atN = points[2 * point + 1];
  const double runE = toE - fromE, runN = toN - fromN;
  const double runSquared = runE * runE + runN * runN;
  if (runSquared <= 0.0) {
    const double e = atE - fromE, n = atN - fromN;
    return std::sqrt(e * e + n * n);
  }
  double part = ((atE - fromE) * runE + (atN - fromN) * runN) / runSquared;
  if (part < 0.0) { part = 0.0; }
  if (part > 1.0) { part = 1.0; }
  const double e = atE - (fromE + part * runE), n = atN - (fromN + part * runN);
  return std::sqrt(e * e + n * n);
}

void KeepBetween(std::span<const double> points,
                 size_t wholeFrom,
                 size_t wholeTo,
                 double withinM,
                 std::vector<bool> &keep) {
  std::vector<std::pair<size_t, size_t>> spans;
  spans.reserve(64);
  spans.emplace_back(wholeFrom, wholeTo);
  while (!spans.empty()) {
    const auto [from, to] = spans.back();
    spans.pop_back();
    if (to <= from + 1) { continue; }
    size_t worst = from;
    double worstM = 0.0;
    for (size_t point = from + 1; point < to; ++point) {
      const double awayM = AwayFromChordM(points, point, from, to);
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
  KeepBetween(eastNorthM, 0, points - 1, withinM, keep);

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

Fitted
Fit(std::span<const double> eastNorthM, double withinM, double tightestM, ReferenceLine &into) {
  return Fit(eastNorthM, withinM, tightestM, std::span<const double>(), into);
}

Fitted Fit(std::span<const double> eastNorthM,
           double withinM,
           double tightestM,
           std::span<const double> classTightestM,
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

  std::vector<double> legM(points - 1, 0.0);
  std::vector<double> headingRad(points - 1, 0.0);
  for (size_t leg = 0; leg + 1 < points; ++leg) {
    const double east = eastNorthM[2 * (leg + 1)] - eastNorthM[2 * leg];
    const double north = eastNorthM[2 * (leg + 1) + 1] - eastNorthM[2 * leg + 1];
    legM[leg] = std::sqrt(east * east + north * north);
    headingRad[leg] = std::atan2(north, east);
  }
  std::vector<double> turnRad(points, 0.0);
  for (size_t vertex = 1; vertex + 1 < points; ++vertex) {
    const double turn = Wrapped(headingRad[vertex] - headingRad[vertex - 1]);
    turnRad[vertex] = turn;
    const double swing = std::fabs(turn);
    if (swing > out.SharpestTurnRad) {
      out.SharpestTurnRad = swing;
      out.SharpestTurnAtM = static_cast<double>(vertex);
    }
    if (swing > 0.5 * std::numbers::pi) { ++out.TurnsPastRightAngle; }
    if (swing > 0.75 * std::numbers::pi) { ++out.TurnsPastHalfCircle; }
  }

  if (points < 3) {
    Placed from;
    from.EastM = eastNorthM[0];
    from.NorthM = eastNorthM[1];
    from.HeadingRad = headingRad[0];
    const Segment only{Curve::Straight, legM[0], 0.0, 0.0};
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
  out.LongestRunVertices = aligned->LongestRunVertices;
  out.SplitByAccuracy = aligned->SplitByAccuracy;
  out.TightestRadiusM = aligned->TightestRadiusM;
  out.TightestDemandedM = aligned->TightestRadiusM;
  out.LengthM = into.LengthM();
  out.Straights = laid->Straights;

  for (const Bend &bend : aligned->Bends) {
    const size_t held = bend.LastVertex - bend.FirstVertex + 1u;
    if (held > 2) { out.SheltredVertices += held - 2; }
    if (bend.RadiusM <= out.TightestRadiusM) { out.TightestAtVertex = bend.FirstVertex; }
    for (size_t vertex = bend.FirstVertex; vertex <= bend.LastVertex; ++vertex) {
      const double classM = vertex < classTightestM.size() ? classTightestM[vertex] : 0.0;
      if (!(classM > 0.0) || bend.RadiusM >= classM) { continue; }
      ++out.UnderClass;
      const double shortfall = bend.RadiusM / classM;
      if (out.UnderClass == 1 || shortfall < out.UnderClassRadiusM / out.UnderClassMinimumM) {
        out.UnderClassAtVertex = vertex;
        out.UnderClassRadiusM = bend.RadiusM;
        out.UnderClassMinimumM = classM;
      }
    }
  }

  for (size_t vertex = 0; vertex < points; ++vertex) {
    const double eastM = eastNorthM[2 * vertex];
    const double northM = eastNorthM[2 * vertex + 1];
    double alongM = 0.0;
    if (!into.Nearest(eastM, northM, 0.5 * out.LengthM, out.LengthM, alongM)) { continue; }
    Placed on;
    if (!into.At(alongM, on)) { continue; }
    const double east = eastM - on.EastM, north = northM - on.NorthM;
    const double awayM = std::sqrt(east * east + north * north);
    if (awayM <= out.WorstOffsetM) { continue; }
    out.WorstOffsetM = awayM;
    out.WorstOffsetAtM = alongM;
    out.WorstVertex = static_cast<double>(vertex);
    out.WorstLegInM = vertex > 0 ? legM[vertex - 1] : 0.0;
    out.WorstLegOutM = vertex + 1 < points ? legM[vertex] : 0.0;
    out.WorstTurnRad = turnRad[vertex];
    out.WorstStationM = alongM;
  }
  out.DriftM = out.WorstOffsetM > withinM ? out.WorstOffsetM - withinM : 0.0;
  out.DriftPerCornerM = out.Corners > 0 ? out.DriftM / static_cast<double>(out.Corners) : 0.0;
  out.Laid = true;
  return out;
}

} // namespace outshine
