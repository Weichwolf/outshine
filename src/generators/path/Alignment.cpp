#include "Earth.h"
#include "Units.h"
#include "Alignment.h"
#include "Angle.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine {

namespace {

constexpr int kSpiralSteps = 96;
constexpr double kShiftDenominator = 96.0;
constexpr double kSpiralShiftDenominator = 24.0;

struct Turned {
  double TurnRad = 0.0;
  double HeadingRad = 0.0;
  double LegM = 0.0;
};

struct Ray {
  EastNorth From;
  double HeadingRad = 0.0;
};

[[nodiscard]] std::optional<EastNorth> Meets(Ray entering, Ray leaving) {
  const double aRunE = std::cos(entering.HeadingRad);
  const double aRunN = std::sin(entering.HeadingRad);
  const double bRunE = std::cos(leaving.HeadingRad);
  const double bRunN = std::sin(leaving.HeadingRad);
  const double cross = aRunE * bRunN - aRunN * bRunE;
  if (std::fabs(cross) < kParallelCross) { return std::nullopt; }
  const double alongA = ((leaving.From.EastM - entering.From.EastM) * bRunN -
                         (leaving.From.NorthM - entering.From.NorthM) * bRunE) /
                        cross;
  return EastNorth{.EastM = entering.From.EastM + alongA * aRunE,
                   .NorthM = entering.From.NorthM + alongA * aRunN};
}

[[nodiscard]] double ShiftShare(double swing) {
  return 1.0 + swing * swing / kShiftDenominator;
}

constexpr double kLeastClothoidShare = 1.0 / 3.0;
constexpr double kMostClothoidShare = 1.0;

[[nodiscard]] double SpiralAtLeast(double radiusM, bool againstAStraight) {
  return againstAStraight ? radiusM * kLeastClothoidShare * kLeastClothoidShare : 0.0;
}

[[nodiscard]] double SpiralAtMost(double radiusM) {
  return radiusM * kMostClothoidShare * kMostClothoidShare;
}

[[nodiscard]] double TangentFor(double radiusM, double swing, double spiralM) {
  return (radiusM + spiralM * spiralM / (kSpiralShiftDenominator * radiusM)) *
             std::tan(0.5 * swing) +
         0.5 * spiralM;
}

[[nodiscard]] double
SpiralInto(double radiusM, double swingRad, double roomM, bool againstAStraight) {
  if (!(radiusM > 0.0)) { return 0.0; }
  const double half = std::tan(0.5 * swingRad);
  const double bareM = radiusM * half;
  const double least = SpiralAtLeast(radiusM, againstAStraight);
  if (!(roomM > bareM)) { return least; }
  const double square = half / (24.0 * radiusM);
  const double solved = (-0.5 + std::sqrt(0.25 + 4.0 * square * (roomM - bareM))) / (2.0 * square);
  double held = solved < least ? least : solved;
  const double most = SpiralAtMost(radiusM);
  held = std::min(held, most);
  const double sweptOut = radiusM * swingRad;
  return held < sweptOut ? held : sweptOut;
}

[[nodiscard]] double FurthestFromArcM(
    std::span<const double> points, size_t from, size_t to, EastNorth centre, double radiusM) {
  double worst = 0.0;
  for (size_t at = from; at <= to; ++at) {
    const double east = points[2 * at] - centre.EastM;
    const double north = points[2 * at + 1] - centre.NorthM;
    const double away = std::fabs(std::sqrt(east * east + north * north) - radiusM);
    worst = away > worst ? away : worst;
  }
  return worst;
}

[[nodiscard]] double AwayM(EastNorth from, EastNorth to) {
  const double east = to.EastM - from.EastM;
  const double north = to.NorthM - from.NorthM;
  return std::sqrt(east * east + north * north);
}

[[nodiscard]] double AllowedAt(std::span<const double> withinAtM, size_t vertex, double withinM) {
  return vertex < withinAtM.size() && withinAtM[vertex] > 0.0 ? withinAtM[vertex] : withinM;
}

[[nodiscard]] double FurthestShareOfArc(std::span<const double> points,
                                        size_t from,
                                        size_t to,
                                        EastNorth centre,
                                        double radiusM,
                                        std::span<const double> withinAtM,
                                        double withinM) {
  double worst = 0.0;
  for (size_t at = from; at <= to; ++at) {
    const double east = points[2 * at] - centre.EastM;
    const double north = points[2 * at + 1] - centre.NorthM;
    const double away = std::fabs(std::sqrt(east * east + north * north) - radiusM);
    const double share = away / AllowedAt(withinAtM, at, withinM);
    worst = share > worst ? share : worst;
  }
  return worst;
}

[[nodiscard]] std::expected<Bend, Refusal> BendOver(std::span<const double> points,
                                                    std::span<const Turned> legs,
                                                    size_t at,
                                                    size_t last,
                                                    double withinM,
                                                    double tightestM,
                                                    std::span<const double> withinAtM) {
  Bend bend;
  bend.FirstVertex = at;
  bend.LastVertex = last;
  bend.TurnRad = Wrapped(legs[last].HeadingRad - legs[at - 1].HeadingRad);

  const double swing = std::fabs(bend.TurnRad);
  if (swing < kLeastTurnRad || swing > std::numbers::pi - kLeastTurnRad) {
    return std::unexpected(
        Refusal{.Said = "vertices " + std::to_string(at) + ".." + std::to_string(last) +
                        " turn through " + std::to_string(bend.TurnRad) +
                        " rad, which no single arc between two straights can carry"});
  }
  const std::optional<EastNorth> met =
      Meets(Ray{.From = {.EastM = points[2 * (at - 1)], .NorthM = points[2 * (at - 1) + 1]},
                .HeadingRad = legs[at - 1].HeadingRad},
            Ray{.From = {.EastM = points[2 * (last + 1)], .NorthM = points[2 * (last + 1) + 1]},
                .HeadingRad = legs[last].HeadingRad});
  if (!met) {
    return std::unexpected(Refusal{.Said = "the legs entering and leaving vertices " +
                                           std::to_string(at) + ".." + std::to_string(last) +
                                           " are parallel and meet nowhere"});
  }
  bend.PiEastM = met->EastM;
  bend.PiNorthM = met->NorthM;

  const double half = 0.5 * swing;
  const double toCentre = (bend.TurnRad > 0.0 ? 1.0 : -1.0) * (0.5 * std::numbers::pi);
  const double bisector = legs[at - 1].HeadingRad + 0.5 * bend.TurnRad + toCentre;
  const auto centreOf = [&](double radiusM, double &centreE, double &centreN) {
    const double away = radiusM / std::cos(half);
    centreE = bend.PiEastM + away * std::cos(bisector);
    centreN = bend.PiNorthM + away * std::sin(bisector);
  };

  const double intoM = AwayM({.EastM = bend.PiEastM, .NorthM = bend.PiNorthM},
                             {.EastM = points[2 * (at - 1)], .NorthM = points[2 * (at - 1) + 1]});
  const double outOfM =
      AwayM({.EastM = bend.PiEastM, .NorthM = bend.PiNorthM},
            {.EastM = points[2 * (last + 1)], .NorthM = points[2 * (last + 1) + 1]});
  const double roomM = intoM < outOfM ? intoM : outOfM;
  const bool againstAStraight =
      at > 1 || last + 2 < points.size() / 2 || std::fabs(intoM - outOfM) > kLeastRunM;
  const double byRoom = roomM / TangentFor(1.0, swing, SpiralAtLeast(1.0, againstAStraight));
  if (!(byRoom > tightestM)) {
    return std::unexpected(
        Refusal{.Said = "vertices " + std::to_string(at) + ".." + std::to_string(last) + " leave " +
                        std::to_string(roomM) +
                        " m of tangent between their straights, which carries no arc wider than " +
                        std::to_string(byRoom) + " m",
                .DemandedM = byRoom,
                .AtVertex = at,
                .Undrivable = 1});
  }

  if (last == at) {
    const double byAccuracy =
        AllowedAt(withinAtM, at, withinM) / (ShiftShare(swing) / std::cos(half) - 1.0);
    bend.RadiusM = byAccuracy < byRoom ? byAccuracy : byRoom;
  } else {
    double low = tightestM;
    double high = byRoom;
    for (int step = 0; step < kSpiralSteps; ++step) {
      const double oneThird = low + (high - low) / 3.0;
      const double twoThirds = high - (high - low) / 3.0;
      double aE = 0.0;
      double aN = 0.0;
      double bE = 0.0;
      double bN = 0.0;
      centreOf(oneThird, aE, aN);
      centreOf(twoThirds, bE, bN);
      if (FurthestShareOfArc(
              points, at, last, {.EastM = aE, .NorthM = aN}, oneThird, withinAtM, withinM) <
          FurthestShareOfArc(
              points, at, last, {.EastM = bE, .NorthM = bN}, twoThirds, withinAtM, withinM)) {
        high = twoThirds;
      } else {
        low = oneThird;
      }
    }
    bend.RadiusM = 0.5 * (low + high);
  }
  if (bend.RadiusM < tightestM) {
    return std::unexpected(Refusal{
        .Said = "vertices " + std::to_string(at) + ".." + std::to_string(last) + " turn through " +
                std::to_string(bend.TurnRad) + " rad and the widest arc that stays within " +
                std::to_string(withinM) + " m of them is " + std::to_string(bend.RadiusM) +
                " m, tighter than the " + std::to_string(tightestM) +
                " m this body can bend to -- a corner tighter than the lock is a route that "
                "doubles back on itself, and that is a finding about the graph",
        .DemandedM = bend.RadiusM,
        .AtVertex = at,
        .Undrivable = 1});
  }
  double centreE = 0.0;
  double centreN = 0.0;
  centreOf(bend.RadiusM, centreE, centreN);
  bend.AwayM =
      FurthestFromArcM(points, at, last, {.EastM = centreE, .NorthM = centreN}, bend.RadiusM);
  bend.AwayShare = FurthestShareOfArc(
      points, at, last, {.EastM = centreE, .NorthM = centreN}, bend.RadiusM, withinAtM, withinM);
  bend.SpiralM = SpiralInto(bend.RadiusM, swing, roomM, againstAStraight);
  bend.ArcM = bend.RadiusM * swing - bend.SpiralM;
  bend.TangentM = TangentFor(bend.RadiusM, swing, bend.SpiralM);
  bend.IntoHeadingRad = legs[at - 1].HeadingRad;
  bend.OutOfHeadingRad = legs[last].HeadingRad;
  bend.IntoEastM = bend.PiEastM - bend.TangentM * std::cos(bend.IntoHeadingRad);
  bend.IntoNorthM = bend.PiNorthM - bend.TangentM * std::sin(bend.IntoHeadingRad);
  bend.OutOfEastM = bend.PiEastM + bend.TangentM * std::cos(bend.OutOfHeadingRad);
  bend.OutOfNorthM = bend.PiNorthM + bend.TangentM * std::sin(bend.OutOfHeadingRad);
  return bend;
}

} // namespace

double JunctionKerbM(double halfAM, double halfBM, double deflectionRad, double shorterLegM) {
  const double swing = std::fabs(deflectionRad);
  if (!(swing > kLeastTurnRad) || swing >= std::numbers::pi - kLeastTurnRad) { return 0.0; }
  const double kerbM =
      std::sqrt(halfAM * halfAM + halfBM * halfBM - 2.0 * halfAM * halfBM * std::cos(swing)) /
      std::sin(swing);
  if (!(shorterLegM > 0.0)) { return kerbM; }
  return kerbM < shorterLegM ? kerbM : shorterLegM;
}

std::expected<Aligned, Refusal> Align(std::span<const double> eastNorthM,
                                      double withinM,
                                      double tightestM,
                                      std::span<const double> withinAtM) {
  const size_t points = eastNorthM.size() / 2;
  if (points < 3) {
    return std::unexpected(
        Refusal{.Said = "an alignment is fitted through 3..N vertices and this one carries " +
                        std::to_string(points)});
  }
  if (!(withinM > 0.0)) {
    return std::unexpected(Refusal{
        .Said =
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
    if (std::fabs(legs[at].TurnRad) < kLeastTurnRad) {
      ++at;
      continue;
    }
    const bool leftward = legs[at].TurnRad > 0.0;
    size_t last = at;
    while (last + 2 < points && std::fabs(legs[last + 1].TurnRad) >= kLeastTurnRad &&
           (legs[last + 1].TurnRad > 0.0) == leftward) {
      ++last;
    }
    const size_t runs = last;

    Bend bend;
    for (;;) {
      const auto held = BendOver(eastNorthM, legs, at, last, withinM, tightestM, withinAtM);
      if (!held) { return std::unexpected(held.error()); }
      if (held->AwayShare <= 1.0 || last == at) {
        bend = *held;
        out.SplitByAccuracy += last < runs ? 1u : 0u;
        break;
      }
      --last;
    }

    ++out.Runs;
    const size_t vertices = bend.LastVertex - bend.FirstVertex + 1u;
    out.LongestRunVertices = vertices > out.LongestRunVertices ? vertices : out.LongestRunVertices;
    out.WorstAwayM = bend.AwayM > out.WorstAwayM ? bend.AwayM : out.WorstAwayM;
    if (out.TightestRadiusM <= 0.0 || bend.RadiusM < out.TightestRadiusM) {
      out.TightestRadiusM = bend.RadiusM;
    }
    out.Bends.push_back(bend);
    at = bend.LastVertex + 1u;
  }

  const auto shrink = [&](Bend &bend, double toM) {
    const double swing = std::fabs(bend.TurnRad);
    bend.RadiusM = toM / TangentFor(1.0, swing, SpiralAtLeast(1.0, true));
    bend.SpiralM = SpiralInto(bend.RadiusM, swing, toM, true);
    bend.ArcM = bend.RadiusM * swing - bend.SpiralM;
    bend.TangentM = TangentFor(bend.RadiusM, swing, bend.SpiralM);
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
      const double betweenM = AwayM({.EastM = before.PiEastM, .NorthM = before.PiNorthM},
                                    {.EastM = after.PiEastM, .NorthM = after.PiNorthM});
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
          .Said = "the bend over vertices " + std::to_string(bend.FirstVertex) + ".." +
                  std::to_string(bend.LastVertex) +
                  " shares its straights with its neighbours and what is left carries only " +
                  std::to_string(bend.RadiusM) + " m, tighter than the " +
                  std::to_string(tightestM) + " m this body can bend to",
          .DemandedM = bend.RadiusM,
          .AtVertex = bend.FirstVertex,
          .Undrivable = 1});
    }
    if (out.TightestRadiusM <= 0.0 || bend.RadiusM < out.TightestRadiusM) {
      out.TightestRadiusM = bend.RadiusM;
    }
  }

  return out;
}

std::expected<Laid, Refusal>
LayAligned(std::span<const double> eastNorthM, const Aligned &aligned, ReferenceLine &into) {
  const size_t points = eastNorthM.size() / 2;
  if (points < 2) {
    return std::unexpected(
        Refusal{.Said = "an alignment is laid through 2..N vertices and this one carries " +
                        std::to_string(points)});
  }

  std::vector<Segment> along;
  along.reserve(4 * aligned.Bends.size() + 2);
  double atEast = eastNorthM[0];
  double atNorth = eastNorthM[1];
  double heading = std::atan2(eastNorthM[3] - eastNorthM[1], eastNorthM[2] - eastNorthM[0]);
  const Bend *untransitioned = nullptr;
  for (const Bend &bend : aligned.Bends) {
    const double straightM = AwayM({.EastM = atEast, .NorthM = atNorth},
                                   {.EastM = bend.IntoEastM, .NorthM = bend.IntoNorthM});
    const double ahead = (bend.IntoEastM - atEast) * std::cos(heading) +
                         (bend.IntoNorthM - atNorth) * std::sin(heading);
    if (ahead < -kLeastRunM) {
      return std::unexpected(Refusal{
          .Said = "the bend over vertices " + std::to_string(bend.FirstVertex) + ".." +
                  std::to_string(bend.LastVertex) + " begins " + std::to_string(-ahead) +
                  " m behind where the one before it ended -- two arcs whose tangents overlap are "
                  "one alignment the straights cannot separate",
          .DemandedM = bend.RadiusM,
          .AtVertex = bend.FirstVertex,
          .Undrivable = 1});
    }
    const bool leadsWithAStraight = straightM > kLeastRunM;
    if ((leadsWithAStraight || untransitioned != nullptr) && !(bend.SpiralM > kLeastRunM)) {
      return std::unexpected(Refusal{
          .Said = "the bend over vertices " + std::to_string(bend.FirstVertex) + ".." +
                  std::to_string(bend.LastVertex) + " meets " + std::to_string(straightM) +
                  " m of straight at radius " + std::to_string(bend.RadiusM) +
                  " m with NO transition: curvature may not step from 0 to " +
                  std::to_string(1.0 / bend.RadiusM) + " per m. Its turn is " +
                  std::to_string(bend.TurnRad) + " rad, its arc " + std::to_string(bend.ArcM) +
                  " m and the tangent it was given " + std::to_string(bend.TangentM) + " m",
          .DemandedM = bend.RadiusM,
          .AtVertex = bend.FirstVertex,
          .Undrivable = 1});
    }
    if (untransitioned != nullptr && straightM > kLeastRunM) {
      return std::unexpected(
          Refusal{.Said = "the bend over vertices " + std::to_string(untransitioned->FirstVertex) +
                          ".." + std::to_string(untransitioned->LastVertex) + " leaves into " +
                          std::to_string(straightM) + " m of straight at radius " +
                          std::to_string(untransitioned->RadiusM) +
                          " m with NO transition out, and its own " +
                          std::to_string(untransitioned->ArcM) + " m of arc ends at curvature " +
                          std::to_string(1.0 / untransitioned->RadiusM) + " per m",
                  .DemandedM = untransitioned->RadiusM,
                  .AtVertex = untransitioned->FirstVertex,
                  .Undrivable = 1});
    }
    untransitioned = nullptr;
    if (leadsWithAStraight) {
      along.push_back(Segment{.Shape = Curve::Straight,
                              .LengthM = straightM,
                              .EntryCurvature = 0.0,
                              .ExitCurvature = 0.0});
    }
    const double curvature = (bend.TurnRad >= 0.0 ? 1.0 : -1.0) / bend.RadiusM;
    if (bend.SpiralM > kLeastRunM) {
      along.push_back(Segment{.Shape = Curve::Spiral,
                              .LengthM = bend.SpiralM,
                              .EntryCurvature = 0.0,
                              .ExitCurvature = curvature});
    }
    if (bend.ArcM > kLeastRunM) {
      along.push_back(Segment{.Shape = Curve::Arc,
                              .LengthM = bend.ArcM,
                              .EntryCurvature = curvature,
                              .ExitCurvature = curvature});
    }
    if (bend.SpiralM > kLeastRunM) {
      along.push_back(Segment{.Shape = Curve::Spiral,
                              .LengthM = bend.SpiralM,
                              .EntryCurvature = curvature,
                              .ExitCurvature = 0.0});
    }
    if (!(bend.SpiralM > kLeastRunM)) { untransitioned = &bend; }
    atEast = bend.OutOfEastM;
    atNorth = bend.OutOfNorthM;
    heading = bend.OutOfHeadingRad;
  }
  const double lastM =
      AwayM({.EastM = atEast, .NorthM = atNorth},
            {.EastM = eastNorthM[2 * (points - 1)], .NorthM = eastNorthM[2 * (points - 1) + 1]});
  if (untransitioned != nullptr && lastM > kLeastRunM) {
    return std::unexpected(Refusal{
        .Said = "the bend over vertices " + std::to_string(untransitioned->FirstVertex) + ".." +
                std::to_string(untransitioned->LastVertex) + " leaves into the closing " +
                std::to_string(lastM) + " m of straight at radius " +
                std::to_string(untransitioned->RadiusM) + " m with NO transition out",
        .DemandedM = untransitioned->RadiusM,
        .AtVertex = untransitioned->FirstVertex,
        .Undrivable = 1});
  }
  if (lastM > kLeastRunM) {
    along.push_back(Segment{
        .Shape = Curve::Straight, .LengthM = lastM, .EntryCurvature = 0.0, .ExitCurvature = 0.0});
  }
  if (along.empty()) {
    return std::unexpected(
        Refusal{.Said = "every straight was consumed by its bends, so the alignment has no "
                        "length"});
  }

  Placed from;
  from.EastM = eastNorthM[0];
  from.NorthM = eastNorthM[1];
  from.HeadingRad = std::atan2(eastNorthM[3] - eastNorthM[1], eastNorthM[2] - eastNorthM[0]);
  std::string error;
  if (!into.Lay(from, along, error)) { return std::unexpected(Refusal{.Said = error}); }
  Laid said;
  said.LengthM = into.LengthM();
  for (const Segment &one : along) { said.Straights += one.Shape == Curve::Straight ? 1u : 0u; }
  return said;
}

} // namespace outshine
