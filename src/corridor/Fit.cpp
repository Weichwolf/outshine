#include "Fit.h"

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

Fitted Fit(const std::vector<double> &eastNorthM, double withinM, ReferenceLine &into) {
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
    if (std::fabs(turn) > out.SharpestTurnRad) { out.SharpestTurnRad = std::fabs(turn); }

    const double shorter = legM[vertex - 1] < legM[vertex] ? legM[vertex - 1] : legM[vertex];
    const double swing = std::fabs(turn);
    const double shiftShare = 1.0 + swing * swing / 96.0;
    const double byAccuracy = withinM / (shiftShare / std::cos(half) - 1.0);
    const double byRoom =
        0.5 * shorter / (shiftShare * std::tan(half) + 0.25 * swing);
    const double radius = byAccuracy < byRoom ? byAccuracy : byRoom;
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

  std::vector<Segment> along;
  along.reserve(3 * points);
  for (size_t leg = 0; leg + 1 < points; ++leg) {
    const double straightM = legM[leg] - tangentM[leg] - tangentM[leg + 1];
    if (straightM > 1.0e-6) {
      along.push_back(Segment{Curve::Straight, straightM, 0.0, 0.0});
      ++out.Straights;
    }
    const size_t vertex = leg + 1;
    if (vertex + 1 >= points || radiusM[vertex] <= 0.0) { continue; }
    const double curvature = (turnRad[vertex] >= 0.0 ? 1.0 : -1.0) / radiusM[vertex];
    along.push_back(Segment{Curve::Spiral, spiralM[vertex], 0.0, curvature});
    along.push_back(Segment{Curve::Arc, arcM[vertex], curvature, curvature});
    along.push_back(Segment{Curve::Spiral, spiralM[vertex], curvature, 0.0});
    ++out.Corners;
  }
  if (along.empty()) {
    out.Error = "every leg was consumed by its corners, so the fit has no length";
    return out;
  }

  Placed from;
  from.EastM = eastNorthM[0];
  from.NorthM = eastNorthM[1];
  from.HeadingRad = headingRad[0];
  if (!into.Lay(from, along, out.Error)) { return out; }

  out.LengthM = into.LengthM();
  for (size_t vertex = 0; vertex < points; ++vertex) {
    const double eastM = eastNorthM[2 * vertex];
    const double northM = eastNorthM[2 * vertex + 1];
    double alongM = 0.0;
    if (!into.Nearest(eastM, northM, 0.5 * out.LengthM, out.LengthM, alongM)) { continue; }
    Placed on;
    if (!into.At(alongM, on)) { continue; }
    const double east = eastM - on.EastM;
    const double north = northM - on.NorthM;
    const double awayM = std::sqrt(east * east + north * north);
    if (awayM > out.WorstOffsetM) {
      out.WorstOffsetM = awayM;
      out.WorstOffsetAtM = alongM;
    }
  }

  out.Laid = true;
  return out;
}

} // namespace outshine
