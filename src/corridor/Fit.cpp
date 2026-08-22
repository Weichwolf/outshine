#include "Fit.h"

#include <numbers>
#include <cmath>

namespace outshine {

namespace {

constexpr double kTurn = 6.283185307179586;

double Wrapped(double angleRad) {
  while (angleRad > 0.5 * kTurn) { angleRad -= kTurn; }
  while (angleRad < -0.5 * kTurn) { angleRad += kTurn; }
  return angleRad;
}

} // namespace

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

void KeepBetween(std::span<const double> points, size_t from, size_t to, double withinM,
                 std::vector<bool> &keep) {
  if (to <= from + 1) { return; }
  size_t worst = from;
  double worstM = 0.0;
  for (size_t point = from + 1; point < to; ++point) {
    const double awayM = AwayFromChordM(points, point, from, to);
    if (awayM > worstM) {
      worstM = awayM;
      worst = point;
    }
  }
  if (worstM <= withinM) { return; }
  keep[worst] = true;
  KeepBetween(points, from, worst, withinM, keep);
  KeepBetween(points, worst, to, withinM, keep);
}

} // namespace

std::vector<double> Simplify(std::span<const double> eastNorthM, double withinM) {
  const size_t points = eastNorthM.size() / 2;
  if (points < 3 || !(withinM > 0.0)) { return {eastNorthM.begin(), eastNorthM.end()}; }
  std::vector<bool> keep(points, false);
  keep.front() = true;
  keep.back() = true;
  KeepBetween(eastNorthM, 0, points - 1, withinM, keep);

  std::vector<double> out;
  out.reserve(eastNorthM.size());
  for (size_t point = 0; point < points; ++point) {
    if (!keep[point]) { continue; }
    out.push_back(eastNorthM[2 * point]);
    out.push_back(eastNorthM[2 * point + 1]);
  }
  return out;
}

double CornerRadiusM(double turnRad, double shorterLegM, double withinM) {
  const double swing = std::fabs(turnRad);
  const double half = 0.5 * swing;
  if (half < 1.0e-9 || half >= 0.5 * kTurn * 0.5 || !(withinM > 0.0)) { return 0.0; }
  const double shiftShare = 1.0 + swing * swing / 96.0;
  const double byAccuracy = withinM / (shiftShare / std::cos(half) - 1.0);
  const double byRoom = 0.5 * shorterLegM / (shiftShare * std::tan(half) + 0.25 * swing);
  return byAccuracy < byRoom ? byAccuracy : byRoom;
}

Fitted Fit(std::span<const double> eastNorthM, double withinM, double tightestM,
           ReferenceLine &into) {
  Fitted out;
  const size_t points = eastNorthM.size() / 2;
  out.Vertices = points;

  if (points < 2) {
    out.Error = "a corridor is fitted through 2..N vertices and this one carries " +
                std::to_string(points);
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
  std::vector<double> radiusM(points, 0.0);
  std::vector<double> spiralM(points, 0.0);
  std::vector<double> arcM(points, 0.0);
  std::vector<double> tangentM(points, 0.0);
  for (size_t vertex = 1; vertex + 1 < points; ++vertex) {
    const double turn = Wrapped(headingRad[vertex] - headingRad[vertex - 1]);
    turnRad[vertex] = turn;
    const double half = std::fabs(0.5 * turn);
    if (half < 1.0e-9) { continue; }
    if (std::fabs(turn) > out.SharpestTurnRad) {
      out.SharpestTurnRad = std::fabs(turn);
      out.SharpestTurnAtM = (double)vertex;
    }
    if (std::fabs(turn) > 0.5 * std::numbers::pi) { ++out.TurnsPastRightAngle; }
    if (std::fabs(turn) > 0.75 * std::numbers::pi) { ++out.TurnsPastHalfCircle; }

    const double shorter = legM[vertex - 1] < legM[vertex] ? legM[vertex - 1] : legM[vertex];
    const double swing = std::fabs(turn);
    const double shiftShare = 1.0 + swing * swing / 96.0;
    const double radius = CornerRadiusM(turn, shorter, withinM);
    if (radius < tightestM) {
      ++out.Undrivable;
      if (out.Undrivable == 1) { out.UndrivableAtM = (double)vertex; }
      continue;
    }
    if (radius > 0.0) {
      radiusM[vertex] = radius;
      spiralM[vertex] = 0.5 * radius * swing;
      arcM[vertex] = 0.5 * radius * swing;
      tangentM[vertex] = radius * (shiftShare * std::tan(half) + 0.25 * swing);
    }
    if (radiusM[vertex] <= 0.0) {
      out.Error = "vertex " + std::to_string(vertex) + " turns " + std::to_string(turn) +
                  " rad between legs of " + std::to_string(legM[vertex - 1]) + " and " +
                  std::to_string(legM[vertex]) +
                  " m, and no corner fits that stays within " + std::to_string(withinM) +
                  " m of it -- a turn this sharp between legs this short is a REFUSAL and not a "
                  "corner to invent";
      return out;
    }
    if (out.TightestRadiusM <= 0.0 || radiusM[vertex] < out.TightestRadiusM) {
      out.TightestRadiusM = radiusM[vertex];
    }
  }

  std::vector<double> atVertexM(points, 0.0);
  for (int pass = 0; pass < 24; ++pass) {
  ++out.Passes;
  out.Straights = 0;
  out.Corners = 0;
  std::vector<Segment> along;
  along.reserve(3 * points);
  double laidM = 0.0;
  for (size_t leg = 0; leg + 1 < points; ++leg) {
    const double straightM = legM[leg] - tangentM[leg] - tangentM[leg + 1];
    if (straightM > 1.0e-6) {
      along.push_back(Segment{Curve::Straight, straightM, 0.0, 0.0});
      laidM += straightM;
      ++out.Straights;
    }
    const size_t vertex = leg + 1;
    atVertexM[vertex] = laidM + spiralM[vertex] + 0.5 * arcM[vertex];
    if (vertex + 1 >= points || radiusM[vertex] <= 0.0) { continue; }
    const double curvature = (turnRad[vertex] >= 0.0 ? 1.0 : -1.0) / radiusM[vertex];
    along.push_back(Segment{Curve::Spiral, spiralM[vertex], 0.0, curvature});
    along.push_back(Segment{Curve::Arc, arcM[vertex], curvature, curvature});
    along.push_back(Segment{Curve::Spiral, spiralM[vertex], curvature, 0.0});
    laidM += 2.0 * spiralM[vertex] + arcM[vertex];
    ++out.Corners;
  }
  if (along.empty()) {
    out.Error = "every leg was consumed by its corners, so the fit has no length";
    return out;
  }

  if (out.Undrivable > 0) {
    out.Error = std::to_string(out.Undrivable) +
                " of these vertices turn too sharply for anything that can only bend to " +
                std::to_string(tightestM) +
                " m, the first at index " + std::to_string((long)out.UndrivableAtM) +
                " -- a corner tighter than the vehicle's own lock is not a road to smooth, it is a "
                "route that doubles back on itself, and that is a finding about the graph";
    return out;
  }

  Placed from;
  from.EastM = eastNorthM[0];
  from.NorthM = eastNorthM[1];
  from.HeadingRad = headingRad[0];
  if (!into.Lay(from, along, out.Error)) { return out; }

  out.LengthM = into.LengthM();
  out.WorstOffsetM = 0.0;
  bool overran = false;
  for (size_t vertex = 0; vertex < points; ++vertex) {
    const double eastM = eastNorthM[2 * vertex];
    const double northM = eastNorthM[2 * vertex + 1];
    double windowM = 0.0;
    if (vertex > 0) { windowM += legM[vertex - 1]; }
    if (vertex + 1 < points) { windowM += legM[vertex]; }
    if (windowM < 1.0) { windowM = 1.0; }
    double alongM = 0.0;
    if (!into.Nearest(eastM, northM, atVertexM[vertex], windowM, alongM)) { continue; }
    Placed on;
    if (!into.At(alongM, on)) { continue; }
    const double east = eastM - on.EastM;
    const double north = northM - on.NorthM;
    const double awayM = std::sqrt(east * east + north * north);
    if (awayM > out.WorstOffsetM) {
      out.WorstOffsetM = awayM;
      out.WorstOffsetAtM = alongM;
      out.WorstVertex = (double)vertex;
      out.WorstLegInM = vertex > 0 ? legM[vertex - 1] : 0.0;
      out.WorstLegOutM = vertex + 1 < points ? legM[vertex] : 0.0;
      out.WorstTurnRad = turnRad[vertex];
      out.WorstRadiusM = radiusM[vertex];
      out.WorstStationM = alongM;
      out.WorstExpectedM = atVertexM[vertex];
    }
    double predictedM = 0.0;
    if (radiusM[vertex] > 0.0) {
      const double swing = std::fabs(turnRad[vertex]);
      const double shiftShare = 1.0 + swing * swing / 96.0;
      predictedM = radiusM[vertex] * (shiftShare / std::cos(0.5 * swing) - 1.0);
    }
    if (awayM > withinM && predictedM <= withinM) {
      if (awayM - predictedM > out.DriftM) { out.DriftM = awayM - predictedM; }
      continue;
    }
    if (awayM > withinM && radiusM[vertex] > 0.0) {
      const double shrink = withinM / awayM;
      const double swing = std::fabs(turnRad[vertex]);
      const double half = 0.5 * swing;
      const double shiftShare = 1.0 + swing * swing / 96.0;
      radiusM[vertex] *= shrink;
      spiralM[vertex] = 0.5 * radiusM[vertex] * swing;
      arcM[vertex] = 0.5 * radiusM[vertex] * swing;
      tangentM[vertex] = radiusM[vertex] * (shiftShare * std::tan(half) + 0.25 * swing);
      if (radiusM[vertex] < tightestM) {
        radiusM[vertex] = tightestM;
        spiralM[vertex] = 0.5 * tightestM * swing;
        arcM[vertex] = 0.5 * tightestM * swing;
        tangentM[vertex] = tightestM * (shiftShare * std::tan(half) + 0.25 * swing);
        ++out.Strained;
        if (awayM > out.StrainedWorstM) { out.StrainedWorstM = awayM; }
        continue;
      }
      ++out.Corrected;
      overran = true;
    }
  }
  if (!overran) {
    out.DriftPerCornerM = out.Corners > 0 ? out.DriftM / (double)out.Corners : 0.0;
    out.Laid = true;
    return out;
  }
  }

  out.Error = "corners kept overrunning " + std::to_string(withinM) +
              " m after twenty-four measured corrections, which is a fit that does not converge rather "
              "than a road";
  return out;
}

} // namespace outshine
