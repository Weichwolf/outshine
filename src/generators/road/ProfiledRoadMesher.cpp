#include "math/Units.h"
#include "math/RenderFrame.h"
#include "ProfiledRoadMesher.h"
#include "JunctionFootprint.h"
#include "math/Vec3.h"

#include "Fit.h"
#include "ReferenceLine.h"
#include "Ribbon.h"

#include <array>
#include <cmath>
#include <span>
#include <utility>
#include <string>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace outshine::Generators {

namespace {

constexpr double kKerbAcrossM = 0.35;

constexpr double kShoulderDipM = 0.10;
constexpr double kShoulderFraction = 0.35;

constexpr double kTrackDepthM = 0.25;

constexpr double kSnapM = 0.001;

double Snapped(double metres) {
  return std::round(metres / kSnapM) * kSnapM;
}

void StoreVertex(RoadMeshBuffers &into,
                 const std::array<double, 3> &at,
                 const Vec3 &normal,
                 const Vec3f &wearsLinear) {
  for (const double one : at) { into.PositionM.push_back(static_cast<float>(one)); }
  for (int axis = 0; axis < 3; ++axis) { into.NormalM.push_back(static_cast<float>(normal[axis])); }
  into.ColourRgba.insert(into.ColourRgba.end(),
                         {wearsLinear[0], wearsLinear[1], wearsLinear[2], 1.0f});
}

}

void ProfiledRoadMesher::Junction(std::span<const RoadGate> gates,
                                  RoadPlane plane,
                                  const Vec3f &wearsLinear,
                                  RoadMeshBuffers &into) const {
  const auto footprint = BuildJunctionFootprint(gates);
  if (footprint.Rim.size() < 3) { return; }
  double centreGrade = 0;
  for (const auto &gate : gates) { centreGrade += gate.GradeM; }
  centreGrade /= static_cast<double>(gates.size());
  const auto vertexAt = [&](EastNorth at, double depth = 0.0) {
    const double east = static_cast<float>(Snapped(at.EastM));
    const double z = static_cast<float>(Snapped(RenderFrame::ZOfNorth(at.NorthM)));
    const double grade = centreGrade + plane.SlopeE * (east - footprint.Centre.EastM) +
                         plane.SlopeN * (RenderFrame::NorthOfZ(z) - footprint.Centre.NorthM);
    return std::array{east, grade - depth, z};
  };
  Vec3 up = {{-plane.SlopeE, 1.0, RenderFrame::ZOfNorth(-plane.SlopeN)}};
  const double normalLength = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
  for (int axis = 0; axis < 3; ++axis) { up[axis] /= normalLength; }
  const Vec3 down = {{-up[0], -up[1], -up[2]}};
  const auto rim = static_cast<uint32_t>(footprint.Rim.size());
  const auto top = static_cast<uint32_t>(into.PositionM.size() / 3);
  const size_t centre = static_cast<size_t>(top) * 3;
  StoreVertex(into, vertexAt(footprint.Centre), up, wearsLinear);
  for (const auto &at : footprint.Rim) { StoreVertex(into, vertexAt(at), up, wearsLinear); }
  const auto bottom = static_cast<uint32_t>(into.PositionM.size() / 3);
  StoreVertex(into, vertexAt(footprint.Centre, kSealedDepthM), down, wearsLinear);
  for (const auto &at : footprint.Rim) {
    StoreVertex(into, vertexAt(at, kSealedDepthM), down, wearsLinear);
  }
  for (uint32_t at = 0; at < rim; ++at) {
    const uint32_t next = (at + 1u) % rim;
    const auto here = vertexAt(footprint.Rim[at]);
    const auto after = vertexAt(footprint.Rim[next]);
    const double apartE = after[0] - here[0];
    const double apartZ = after[2] - here[2];
    const double run = std::hypot(apartE, apartZ);
    if (run < kSnapM) { continue; }
    const size_t corner = (static_cast<size_t>(top) + 1u + at) * 3u;
    const size_t following = (static_cast<size_t>(top) + 1u + next) * 3u;
    const double aE = static_cast<double>(into.PositionM[corner]) - into.PositionM[centre];
    const double aZ =
        static_cast<double>(into.PositionM[corner + 2u]) - into.PositionM[centre + 2u];
    const double bE = static_cast<double>(into.PositionM[following]) - into.PositionM[centre];
    const double bZ =
        static_cast<double>(into.PositionM[following + 2u]) - into.PositionM[centre + 2u];
    if (aE * bZ - aZ * bE > kSnapM * kSnapM) {
      into.Index.insert(into.Index.end(), {top, top + 1u + next, top + 1u + at});
      into.Index.insert(into.Index.end(), {bottom, bottom + 1u + at, bottom + 1u + next});
    }
    const Vec3 outward = {{apartZ / run, 0.0, -apartE / run}};
    const auto side = static_cast<uint32_t>(into.PositionM.size() / 3);
    StoreVertex(into, here, outward, wearsLinear);
    StoreVertex(into, vertexAt(footprint.Rim[at], kSealedDepthM), outward, wearsLinear);
    StoreVertex(into, after, outward, wearsLinear);
    StoreVertex(into, vertexAt(footprint.Rim[next], kSealedDepthM), outward, wearsLinear);
    into.Index.insert(into.Index.end(), {side, side + 3u, side + 1u, side, side + 2u, side + 3u});
  }
}

namespace {

constexpr double kLayWithinM = 0.5;
constexpr double kLayTightestM = 5.5;
constexpr double kSagittaM = 0.20;
constexpr double kLeastStepM = 2.0;
constexpr double kMostStepM = 32.0;

double StepFor(double radiusM) {
  if (!(radiusM > 0.0)) { return kMostStepM; }
  const double chord = std::sqrt(8.0 * radiusM * kSagittaM);
  if (chord < kLeastStepM) { return kLeastStepM; }
  return chord > kMostStepM ? kMostStepM : chord;
}

Section SectionFor(double halfWidthM, RoadProfile profile) {
  Section cut;
  cut.HalfWidthM = halfWidthM;
  switch (profile) {
    case RoadProfile::Rounded:
      cut.ShoulderM = halfWidthM * kShoulderFraction;
      cut.ThicknessM = kTrackDepthM;
      break;
    case RoadProfile::Simple:
      cut.ShoulderM = kShoulderDipM;
      cut.ThicknessM = kSealedDepthM;
      break;
    case RoadProfile::Kerbed:
      cut.ShoulderM = kKerbAcrossM;
      cut.ThicknessM = kSealedDepthM;
      break;
  }
  return cut;
}

void Pour(const Ribbon &woven, const Vec3f &wearsLinear, RoadMeshBuffers &into) {
  const auto firstVertex = static_cast<uint32_t>(into.PositionM.size() / 3u);
  const size_t vertices = woven.PositionM.size() / 3u;
  for (size_t one = 0; one < vertices; ++one) {
    into.PositionM.push_back(static_cast<float>(woven.PositionM[one * 3u] + woven.OriginM[0]));
    into.PositionM.push_back(static_cast<float>(woven.PositionM[one * 3u + 1u] + woven.OriginM[1]));
    into.PositionM.push_back(static_cast<float>(woven.PositionM[one * 3u + 2u] + woven.OriginM[2]));
    into.NormalM.push_back(woven.NormalM[one * 3u]);
    into.NormalM.push_back(woven.NormalM[one * 3u + 1u]);
    into.NormalM.push_back(woven.NormalM[one * 3u + 2u]);
    into.ColourRgba.push_back(wearsLinear[0]);
    into.ColourRgba.push_back(wearsLinear[1]);
    into.ColourRgba.push_back(wearsLinear[2]);
    into.ColourRgba.push_back(1.0f);
  }
  for (const uint32_t one : woven.Index) { into.Index.push_back(firstVertex + one); }
}

bool FitPiece(std::span<const double> eastNorthM,
              ReferenceLine &line,
              double &tightestM,
              RoadMeshingRejections &rejections) {
  if (eastNorthM.size() == 4) {
    const double runE = eastNorthM[2] - eastNorthM[0];
    const double runN = eastNorthM[3] - eastNorthM[1];
    const double runM = std::sqrt(runE * runE + runN * runN);
    if (!(runM > 0.0)) {
      ++rejections.TooShort;
      return false;
    }
    const Placed from{
        .EastM = eastNorthM[0], .NorthM = eastNorthM[1], .HeadingRad = std::atan2(runN, runE)};
    const Segment straight{.Shape = Curve::Straight, .LengthM = runM};
    std::string laidWhy;
    if (!line.Lay(from, std::span<const Segment>(&straight, 1), laidWhy)) {
      ++rejections.Fit;
      return false;
    }
  } else {
    const Fitted laid = Fit(std::span<const double>(eastNorthM.data(), eastNorthM.size()),
                            kLayWithinM,
                            kLayTightestM,
                            line);
    if (!laid.Laid || !(line.LengthM() > 0.0)) {
      ++rejections.Fit;
      return false;
    }
    tightestM = laid.TightestRadiusM;
  }

  return true;
}

struct ElevationSamples {
  std::span<const double> GradeM;
  std::span<const double> ReachedM;
};

std::vector<Knot> ElevationKnots(ElevationSamples samples, double lengthM) {
  const auto gradeM = samples.GradeM;
  const auto reachedM = samples.ReachedM;
  const double wholeM = reachedM[reachedM.size() - 1u] - reachedM[0];
  const double distanceScale = wholeM / lengthM;
  std::vector<Knot> rise;
  rise.reserve(gradeM.size());
  for (size_t one = 0; one < gradeM.size(); ++one) {
    const double part = wholeM > kLeastTurnRad ? (reachedM[one] - reachedM[0]) / wholeM : 0.0;
    double rate = 0.0;
    if (one + 1u < gradeM.size()) {
      const double span = reachedM[one + 1u] - reachedM[one];
      rate = span > kLeastTurnRad ? (gradeM[one + 1u] - gradeM[one]) / span : 0.0;
    } else if (one > 0) {
      const double span = reachedM[one] - reachedM[one - 1u];
      rate = span > kLeastTurnRad ? (gradeM[one] - gradeM[one - 1u]) / span : 0.0;
    }
    rise.push_back(
        Knot{.AlongM = part * lengthM, .Value = gradeM[one], .RatePerM = rate * distanceScale});
  }
  return rise;
}

std::array<RoadGate, 2>
CornerGates(std::span<const RoadStation> along, size_t at, double halfWidthM) {
  const auto facing = [&](size_t one, size_t two) {
    const double runE = along[two].EastM - along[one].EastM;
    const double runN = along[two].NorthM - along[one].NorthM;
    const double runM = std::sqrt(runE * runE + runN * runN);
    return runM > kLeastTurnRad ? std::pair<double, double>{runE / runM, runN / runM}
                                : std::pair<double, double>{0.0, 0.0};
  };
  const auto back = facing(at, at - 1u);
  const auto on = facing(at, at + 1u);
  return {{RoadGate{.EastM = along[at].EastM,
                    .NorthM = along[at].NorthM,
                    .GradeM = along[at].GradeM,
                    .OutE = back.first,
                    .OutN = back.second,
                    .HalfWidthM = halfWidthM},
           RoadGate{.EastM = along[at].EastM,
                    .NorthM = along[at].NorthM,
                    .GradeM = along[at].GradeM,
                    .OutE = on.first,
                    .OutN = on.second,
                    .HalfWidthM = halfWidthM}}};
}

bool LayPiece(std::span<const double> eastNorthM,
              ElevationSamples elevation,
              const RoadSweep &how,
              RoadMeshBuffers &into,
              RoadMeshingRejections &rejections) {
  ReferenceLine line;
  double tightestM = 0.0;
  if (!FitPiece(eastNorthM, line, tightestM, rejections)) { return false; }
  auto rise = ElevationKnots(elevation, line.LengthM());
  std::array<Knot, 2> bank = {
      {Knot{.AlongM = 0.0, .Value = how.Crossfall, .RatePerM = 0.0},
       Knot{.AlongM = line.LengthM(), .Value = how.Crossfall, .RatePerM = 0.0}}};
  for (size_t end = 0; end < how.EndPlanes.size(); ++end) {
    const auto &endPlane = how.EndPlanes[end];
    if (!endPlane) { continue; }
    Placed pose;
    if (!line.At(bank[end].AlongM, pose)) {
      ++rejections.Rise;
      return false;
    }
    const RoadPlane &plane = *endPlane;
    const double east = std::cos(pose.HeadingRad);
    const double north = std::sin(pose.HeadingRad);
    const double alongSlope = plane.SlopeE * east + plane.SlopeN * north;
    const double acrossSlope = -plane.SlopeE * north + plane.SlopeN * east;
    (end == 0 ? rise.front() : rise.back()).RatePerM = alongSlope;
    bank[end].Value = -std::atan(acrossSlope);
    bank[end].RatePerM = pose.CurvaturePerM * alongSlope / (1.0 + acrossSlope * acrossSlope);
  }
  std::string said;
  if (!line.Rise(std::span<const Knot>(rise.data(), rise.size()), said)) {
    ++rejections.Rise;
    return false;
  }
  if (!line.Bank(std::span<const Knot>(bank.data(), 2), said)) {
    ++rejections.Bank;
    return false;
  }

  const Ribbon woven = Sweep(line,
                             SectionFor(how.HalfWidthM, how.Profile),
                             0.0,
                             line.LengthM(),
                             StepFor(tightestM),
                             how.Form);
  if (!woven.Woven) {
    ++rejections.Sweep;
    return false;
  }
  Pour(woven, how.WearsLinear, into);
  return true;
}

RoadSweep PieceSettings(RoadSweep how, size_t first, size_t count, size_t total) {
  if (first != 0) { how.EndPlanes[0] = RoadPlane{}; }
  if (first + count != total) { how.EndPlanes[1] = RoadPlane{}; }
  return how;
}

}

RoadMeshingStats ProfiledRoadMesher::Sweep(std::span<const RoadStation> along,
                                           RoadSweep how,
                                           RoadMeshBuffers &into) const {
  const double halfWidthM = how.HalfWidthM;
  const Vec3f &wearsLinear = how.WearsLinear;
  RoadMeshingStats tally;
  RoadMeshingRejections &rejections = tally.Rejections;
  if (along.size() < 2 || !(halfWidthM > 0.0)) { return tally; }

  std::vector<double> eastNorth;
  std::vector<double> grade;
  std::vector<double> reached;
  eastNorth.reserve(along.size() * 2u);
  grade.reserve(along.size());
  reached.reserve(along.size());
  for (size_t one = 0; one < along.size(); ++one) {
    eastNorth.push_back(along[one].EastM);
    eastNorth.push_back(along[one].NorthM);
    grade.push_back(along[one].GradeM);
    if (one == 0) {
      reached.push_back(0.0);
    } else {
      const double spanE = along[one].EastM - along[one - 1u].EastM;
      const double spanN = along[one].NorthM - along[one - 1u].NorthM;
      reached.push_back(reached[one - 1u] + std::sqrt(spanE * spanE + spanN * spanN));
    }
  }

  size_t from = 0;
  while (from + 2u <= along.size()) {
    ReferenceLine probe;
    const Fitted got =
        Fit(std::span<const double>(eastNorth.data() + from * 2u, (along.size() - from) * 2u),
            kLayWithinM,
            kLayTightestM,
            probe);
    const size_t upTo = got.Laid ? along.size() - from - 1u : got.TightestDemandedAtVertex;
    const size_t count = upTo + 1u;
    const RoadSweep piece = PieceSettings(how, from, count, along.size());
    if (count >= 2u) {
      if (LayPiece(std::span<const double>(eastNorth.data() + from * 2u, count * 2u),
                   {.GradeM = std::span<const double>(grade.data() + from, count),
                    .ReachedM = std::span<const double>(reached.data() + from, count)},
                   piece,
                   into,
                   rejections)) {
        ++tally.Pieces;
      } else {
        ++tally.Refused;
      }
    } else {
      ++tally.Refused;
      ++rejections.TooShort;
    }
    if (got.Laid) { break; }
    if (got.Undrivable == 0 || got.TightestDemandedAtVertex == 0) {
      ++tally.Refused;
      break;
    }
    ++tally.Cuts;
    {
      const size_t at = from + upTo;
      if (at > 0 && at + 1u < along.size()) {
        const auto corner = CornerGates(along, at, halfWidthM);
        Junction(std::span<const RoadGate>(corner.data(), 2), {}, wearsLinear, into);
      }
    }
    from += upTo;
  }
  return tally;
}
}
