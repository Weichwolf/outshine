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

[[nodiscard]] double ShiftShare(double swing) { return 1.0 + swing * swing / 96.0; }

constexpr double kSpiralShare = 0.5;

[[nodiscard]] double TangentShare(double swing) {
  return ShiftShare(swing) * std::tan(0.5 * swing) + 0.25 * swing;
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

[[nodiscard]] double AwayM(double fromE, double fromN, double toE, double toN) {
  const double east = toE - fromE, north = toN - fromN;
  return std::sqrt(east * east + north * north);
}

[[nodiscard]] std::expected<Bend, Refusal> BendOver(std::span<const double> points,
                                                        std::span<const Turned> legs, size_t at,
                                                        size_t last, double withinM,
                                                        double tightestM) {
  Bend bend;
  bend.FirstVertex = at;
  bend.LastVertex = last;
  bend.TurnRad = Wrapped(legs[last].HeadingRad - legs[at - 1].HeadingRad);

  const double swing = std::fabs(bend.TurnRad);
  if (swing < 1.0e-9 || swing > std::numbers::pi - 1.0e-9) {
    return std::unexpected(Refusal{"vertices " + std::to_string(at) + ".." + std::to_string(last) +
                           " turn through " + std::to_string(bend.TurnRad) +
                           " rad, which no single arc between two straights can carry"});
  }
  if (!Meets(points[2 * (at - 1)], points[2 * (at - 1) + 1], legs[at - 1].HeadingRad,
             points[2 * (last + 1)], points[2 * (last + 1) + 1], legs[last].HeadingRad,
             bend.PiEastM, bend.PiNorthM)) {
    return std::unexpected(Refusal{"the legs entering and leaving vertices " + std::to_string(at) + ".." +
                           std::to_string(last) + " are parallel and meet nowhere"});
  }

  const double half = 0.5 * swing;
  const double toCentre = (bend.TurnRad > 0.0 ? 1.0 : -1.0) * (0.5 * std::numbers::pi);
  const double bisector = legs[at - 1].HeadingRad + 0.5 * bend.TurnRad + toCentre;
  const auto centreOf = [&](double radiusM, double &centreE, double &centreN) {
    const double away = radiusM / std::cos(half);
    centreE = bend.PiEastM + away * std::cos(bisector);
    centreN = bend.PiNorthM + away * std::sin(bisector);
  };

  const double intoM = AwayM(bend.PiEastM, bend.PiNorthM, points[2 * (at - 1)],
                             points[2 * (at - 1) + 1]);
  const double outOfM = AwayM(bend.PiEastM, bend.PiNorthM, points[2 * (last + 1)],
                              points[2 * (last + 1) + 1]);
  const double roomM = intoM < outOfM ? intoM : outOfM;
  const double byRoom = roomM / TangentShare(swing);
  if (!(byRoom > tightestM)) {
    return std::unexpected(
        Refusal{"vertices " + std::to_string(at) + ".." + std::to_string(last) + " leave " +
                    std::to_string(roomM) +
                    " m of tangent between their straights, which carries no arc wider than " +
                    std::to_string(byRoom) + " m",
                byRoom, at, 1});
  }

  if (last == at) {
    const double byAccuracy = withinM / (ShiftShare(swing) / std::cos(half) - 1.0);
    bend.RadiusM = byAccuracy < byRoom ? byAccuracy : byRoom;
  } else {
    double low = tightestM, high = byRoom;
    for (int step = 0; step < 96; ++step) {
      const double oneThird = low + (high - low) / 3.0;
      const double twoThirds = high - (high - low) / 3.0;
      double aE = 0.0, aN = 0.0, bE = 0.0, bN = 0.0;
      centreOf(oneThird, aE, aN);
      centreOf(twoThirds, bE, bN);
      if (FurthestFromArcM(points, at, last, aE, aN, oneThird) <
          FurthestFromArcM(points, at, last, bE, bN, twoThirds)) {
        high = twoThirds;
      } else {
        low = oneThird;
      }
    }
    bend.RadiusM = 0.5 * (low + high);
  }
  if (bend.RadiusM < tightestM) {
    return std::unexpected(Refusal{
        "vertices " + std::to_string(at) + ".." + std::to_string(last) + " turn through " +
            std::to_string(bend.TurnRad) + " rad and the widest arc that stays within " +
            std::to_string(withinM) + " m of them is " + std::to_string(bend.RadiusM) +
            " m, tighter than the " + std::to_string(tightestM) +
            " m this vehicle can bend to -- a corner tighter than the lock is a route that "
            "doubles back on itself, and that is a finding about the graph",
        bend.RadiusM, at, 1});
  }
  double centreE = 0.0, centreN = 0.0;
  centreOf(bend.RadiusM, centreE, centreN);
  bend.AwayM = FurthestFromArcM(points, at, last, centreE, centreN, bend.RadiusM);
  bend.TangentM = bend.RadiusM * TangentShare(swing);
  bend.SpiralM = kSpiralShare * bend.RadiusM * swing;
  bend.ArcM = (1.0 - kSpiralShare) * bend.RadiusM * swing;
  bend.IntoHeadingRad = legs[at - 1].HeadingRad;
  bend.OutOfHeadingRad = legs[last].HeadingRad;
  bend.IntoEastM = bend.PiEastM - bend.TangentM * std::cos(bend.IntoHeadingRad);
  bend.IntoNorthM = bend.PiNorthM - bend.TangentM * std::sin(bend.IntoHeadingRad);
  bend.OutOfEastM = bend.PiEastM + bend.TangentM * std::cos(bend.OutOfHeadingRad);
  bend.OutOfNorthM = bend.PiNorthM + bend.TangentM * std::sin(bend.OutOfHeadingRad);
  return bend;
}

}

std::expected<Aligned, Refusal> Align(std::span<const double> eastNorthM, double withinM,
                                          double tightestM) {
  const size_t points = eastNorthM.size() / 2;
  if (points < 3) {
    return std::unexpected(Refusal{"an alignment is fitted through 3..N vertices and this one carries " +
                           std::to_string(points)});
  }
  if (!(withinM > 0.0)) {
    return std::unexpected(Refusal{
        "an alignment is bounded by how far it may leave the vertices, and this one declares " +
        std::to_string(withinM) + " m"});
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
    for (;;) {
      const auto held = BendOver(eastNorthM, legs, at, last, withinM, tightestM);
      if (!held) { return std::unexpected(held.error()); }
      if (held->AwayM <= withinM || last == at) {
        bend = *held;
        break;
      }
      --last;
    }

    ++out.Runs;
    const size_t vertices = bend.LastVertex - bend.FirstVertex + 1u;
    out.LongestRunVertices =
        vertices > out.LongestRunVertices ? vertices : out.LongestRunVertices;
    out.WorstAwayM = bend.AwayM > out.WorstAwayM ? bend.AwayM : out.WorstAwayM;
    if (out.TightestRadiusM <= 0.0 || bend.RadiusM < out.TightestRadiusM) {
      out.TightestRadiusM = bend.RadiusM;
    }
    out.Bends.push_back(bend);
    at = bend.LastVertex + 1u;
  }

  const auto shrink = [&](Bend &bend, double toM) {
    const double swing = std::fabs(bend.TurnRad);
    bend.RadiusM = toM / TangentShare(swing);
    bend.TangentM = toM;
    bend.SpiralM = kSpiralShare * bend.RadiusM * swing;
    bend.ArcM = (1.0 - kSpiralShare) * bend.RadiusM * swing;
    bend.IntoEastM = bend.PiEastM - toM * std::cos(bend.IntoHeadingRad);
    bend.IntoNorthM = bend.PiNorthM - toM * std::sin(bend.IntoHeadingRad);
    bend.OutOfEastM = bend.PiEastM + toM * std::cos(bend.OutOfHeadingRad);
    bend.OutOfNorthM = bend.PiNorthM + toM * std::sin(bend.OutOfHeadingRad);
  };

  for (int pass = 0; pass < 8; ++pass) {
    bool crowded = false;
    for (size_t one = 0; one + 1 < out.Bends.size(); ++one) {
      Bend &before = out.Bends[one];
      Bend &after = out.Bends[one + 1];
      const double betweenM = AwayM(before.PiEastM, before.PiNorthM, after.PiEastM,
                                    after.PiNorthM);
      const double wantedM = before.TangentM + after.TangentM;
      if (wantedM <= betweenM) { continue; }
      crowded = true;
      const double halfM = 0.5 * betweenM;
      if (before.TangentM <= halfM) {
        shrink(after, betweenM - before.TangentM);
      } else if (after.TangentM <= halfM) {
        shrink(before, betweenM - after.TangentM);
      } else {
        shrink(before, halfM);
        shrink(after, halfM);
      }
    }
    if (!crowded) { break; }
  }

  out.TightestRadiusM = 0.0;
  for (const Bend &bend : out.Bends) {
    if (bend.RadiusM < tightestM) {
      return std::unexpected(Refusal{
          "the bend over vertices " + std::to_string(bend.FirstVertex) + ".." +
              std::to_string(bend.LastVertex) +
              " shares its straights with its neighbours and what is left carries only " +
              std::to_string(bend.RadiusM) + " m, tighter than the " +
              std::to_string(tightestM) + " m this vehicle can bend to",
          bend.RadiusM, bend.FirstVertex, 1});
    }
    if (out.TightestRadiusM <= 0.0 || bend.RadiusM < out.TightestRadiusM) {
      out.TightestRadiusM = bend.RadiusM;
    }
  }

  return out;
}

std::expected<Laying, Refusal> LayAligned(std::span<const double> eastNorthM,
                                              const Aligned &aligned, ReferenceLine &into) {
  const size_t points = eastNorthM.size() / 2;
  if (points < 2) {
    return std::unexpected(Refusal{"an alignment is laid through 2..N vertices and this one carries " +
                           std::to_string(points)});
  }

  std::vector<Segment> along;
  along.reserve(4 * aligned.Bends.size() + 2);
  double atEast = eastNorthM[0], atNorth = eastNorthM[1];
  double heading = std::atan2(eastNorthM[3] - eastNorthM[1], eastNorthM[2] - eastNorthM[0]);
  for (const Bend &bend : aligned.Bends) {
    const double straightM = AwayM(atEast, atNorth, bend.IntoEastM, bend.IntoNorthM);
    const double ahead = (bend.IntoEastM - atEast) * std::cos(heading) +
                         (bend.IntoNorthM - atNorth) * std::sin(heading);
    if (ahead < -1.0e-6) {
      return std::unexpected(Refusal{
          "the bend over vertices " + std::to_string(bend.FirstVertex) + ".." +
              std::to_string(bend.LastVertex) + " begins " + std::to_string(-ahead) +
              " m behind where the one before it ended -- two arcs whose tangents overlap are "
              "one alignment the straights cannot separate",
          bend.RadiusM, bend.FirstVertex, 1});
    }
    if (straightM > 1.0e-6) { along.push_back(Segment{Curve::Straight, straightM, 0.0, 0.0}); }
    const double curvature = (bend.TurnRad >= 0.0 ? 1.0 : -1.0) / bend.RadiusM;
    along.push_back(Segment{Curve::Spiral, bend.SpiralM, 0.0, curvature});
    along.push_back(Segment{Curve::Arc, bend.ArcM, curvature, curvature});
    along.push_back(Segment{Curve::Spiral, bend.SpiralM, curvature, 0.0});
    atEast = bend.OutOfEastM;
    atNorth = bend.OutOfNorthM;
    heading = bend.OutOfHeadingRad;
  }
  const double lastM =
      AwayM(atEast, atNorth, eastNorthM[2 * (points - 1)], eastNorthM[2 * (points - 1) + 1]);
  if (lastM > 1.0e-6) { along.push_back(Segment{Curve::Straight, lastM, 0.0, 0.0}); }
  if (along.empty()) {
    return std::unexpected(Refusal{"every straight was consumed by its bends, so the alignment has no "
                           "length"});
  }

  Placed from;
  from.EastM = eastNorthM[0];
  from.NorthM = eastNorthM[1];
  from.HeadingRad = std::atan2(eastNorthM[3] - eastNorthM[1], eastNorthM[2] - eastNorthM[0]);
  std::string error;
  if (!into.Lay(from, along, error)) { return std::unexpected(Refusal{error}); }
  Laying said;
  said.LengthM = into.LengthM();
  for (const Segment &one : along) { said.Straights += one.Shape == Curve::Straight ? 1u : 0u; }
  return said;
}

}
