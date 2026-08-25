#include "Alignment.h"
#include "Angle.h"

#include <cmath>
#include <numbers>

namespace outshine {

namespace {

struct Turned {
  double TurnRad = 0.0;
  double HeadingRad = 0.0;
  double LegM = 0.0;
};

[[nodiscard]] bool Meets(double aEast, double aNorth, double aHeading, double bEast,
                         double bNorth, double bHeading, double &atEast, double &atNorth) {
  const double aRunE = std::cos(aHeading), aRunN = std::sin(aHeading);
  const double bRunE = std::cos(bHeading), bRunN = std::sin(bHeading);
  const double cross = aRunE * bRunN - aRunN * bRunE;
  if (std::fabs(cross) < 1.0e-12) { return false; }
  const double alongA = ((bEast - aEast) * bRunN - (bNorth - aNorth) * bRunE) / cross;
  atEast = aEast + alongA * aRunE;
  atNorth = aNorth + alongA * aRunN;
  return true;
}

[[nodiscard]] double FurthestFromArcM(std::span<const double> points, size_t from, size_t to,
                                      double centreE, double centreN, double radiusM) {
  double worst = 0.0;
  for (size_t at = from; at <= to; ++at) {
    const double east = points[2 * at] - centreE;
    const double north = points[2 * at + 1] - centreN;
    const double away = std::fabs(std::sqrt(east * east + north * north) - radiusM);
    worst = away > worst ? away : worst;
  }
  return worst;
}

}

std::expected<Aligned, std::string> Align(std::span<const double> eastNorthM, double withinM,
                                          double tightestM) {
  const size_t points = eastNorthM.size() / 2;
  if (points < 3) {
    return std::unexpected("an alignment is fitted through 3..N vertices and this one carries " +
                           std::to_string(points));
  }
  if (!(withinM > 0.0)) {
    return std::unexpected(
        "an alignment is bounded by how far it may leave the vertices, and this one declares " +
        std::to_string(withinM) + " m");
  }

  std::vector<Turned> legs(points - 1);
  for (size_t leg = 0; leg + 1 < points; ++leg) {
    const double east = eastNorthM[2 * (leg + 1)] - eastNorthM[2 * leg];
    const double north = eastNorthM[2 * (leg + 1) + 1] - eastNorthM[2 * leg + 1];
    legs[leg].LegM = std::sqrt(east * east + north * north);
    legs[leg].HeadingRad = std::atan2(north, east);
  }
  for (size_t vertex = 1; vertex + 1 < points; ++vertex) {
    legs[vertex].TurnRad = Wrapped(legs[vertex].HeadingRad - legs[vertex - 1].HeadingRad);
  }

  Aligned out;
  size_t at = 1;
  while (at + 1 < points) {
    if (std::fabs(legs[at].TurnRad) < 1.0e-9) {
      ++at;
      continue;
    }
    const bool leftward = legs[at].TurnRad > 0.0;
    size_t last = at;
    while (last + 2 < points && std::fabs(legs[last + 1].TurnRad) >= 1.0e-9 &&
           (legs[last + 1].TurnRad > 0.0) == leftward) {
      ++last;
    }

    Bend bend;
    bend.FirstVertex = at;
    bend.LastVertex = last;
    bend.TurnRad = Wrapped(legs[last].HeadingRad - legs[at - 1].HeadingRad);
    ++out.Runs;
    const size_t held = last - at + 1u;
    out.LongestRunVertices = held > out.LongestRunVertices ? held : out.LongestRunVertices;

    const double swing = std::fabs(bend.TurnRad);
    if (swing < 1.0e-9 || swing > std::numbers::pi - 1.0e-9) {
      return std::unexpected("vertices " + std::to_string(at) + ".." + std::to_string(last) +
                             " turn through " + std::to_string(bend.TurnRad) +
                             " rad, which no single arc between two straights can carry");
    }
    if (!Meets(eastNorthM[2 * (at - 1)], eastNorthM[2 * (at - 1) + 1], legs[at - 1].HeadingRad,
               eastNorthM[2 * (last + 1)], eastNorthM[2 * (last + 1) + 1], legs[last].HeadingRad,
               bend.PiEastM, bend.PiNorthM)) {
      return std::unexpected("the legs entering and leaving vertices " + std::to_string(at) +
                             ".." + std::to_string(last) + " are parallel and meet nowhere");
    }

    const double half = 0.5 * swing;
    const double toCentre = (leftward ? 1.0 : -1.0) * (0.5 * std::numbers::pi);
    const double bisector = legs[at - 1].HeadingRad + 0.5 * bend.TurnRad + toCentre;
    const auto centreOf = [&](double radiusM, double &centreE, double &centreN) {
      const double away = radiusM / std::cos(half);
      centreE = bend.PiEastM + away * std::cos(bisector);
      centreN = bend.PiNorthM + away * std::sin(bisector);
    };

    const double intoM =
        std::sqrt(std::pow(bend.PiEastM - eastNorthM[2 * (at - 1)], 2.0) +
                  std::pow(bend.PiNorthM - eastNorthM[2 * (at - 1) + 1], 2.0));
    const double outOfM =
        std::sqrt(std::pow(bend.PiEastM - eastNorthM[2 * (last + 1)], 2.0) +
                  std::pow(bend.PiNorthM - eastNorthM[2 * (last + 1) + 1], 2.0));
    const double roomM = intoM < outOfM ? intoM : outOfM;
    const double byRoom = roomM / std::tan(half);

    double low = tightestM, high = byRoom;
    if (!(high > low)) {
      return std::unexpected("vertices " + std::to_string(at) + ".." + std::to_string(last) +
                             " leave " + std::to_string(roomM) +
                             " m of tangent between their straights, which carries no arc "
                             "wider than " + std::to_string(byRoom) + " m");
    }
    for (int step = 0; step < 96; ++step) {
      const double oneThird = low + (high - low) / 3.0;
      const double twoThirds = high - (high - low) / 3.0;
      double aE = 0.0, aN = 0.0, bE = 0.0, bN = 0.0;
      centreOf(oneThird, aE, aN);
      centreOf(twoThirds, bE, bN);
      const double atOne = FurthestFromArcM(eastNorthM, at, last, aE, aN, oneThird);
      const double atTwo = FurthestFromArcM(eastNorthM, at, last, bE, bN, twoThirds);
      if (atOne < atTwo) {
        high = twoThirds;
      } else {
        low = oneThird;
      }
    }
    bend.RadiusM = 0.5 * (low + high);
    double centreE = 0.0, centreN = 0.0;
    centreOf(bend.RadiusM, centreE, centreN);
    bend.AwayM = FurthestFromArcM(eastNorthM, at, last, centreE, centreN, bend.RadiusM);
    bend.TangentM = bend.RadiusM * std::tan(half);

    out.WorstAwayM = bend.AwayM > out.WorstAwayM ? bend.AwayM : out.WorstAwayM;
    if (out.TightestRadiusM <= 0.0 || bend.RadiusM < out.TightestRadiusM) {
      out.TightestRadiusM = bend.RadiusM;
    }
    out.Bends.push_back(bend);
    at = last + 1;
  }

  return out;
}

}
