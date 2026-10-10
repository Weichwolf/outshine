#include "math/Units.h"
#include "math/RenderFrame.h"
#include "ProfiledRoadMesher.h"
#include "RoadCrossSection.h"
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

struct PortVertex {
  Vec3f Position;
  double DepthM = 0;
  int Facing = 0;
};

struct PortTarget {
  size_t FirstVertex = 0;
  size_t Stations = 0;
  RibbonForm Form = RibbonForm::ClosedShell;
  bool AtEnd = false;
};

void StorePortVertex(RoadMeshBuffers &into, size_t vertex, Vec3f normal, PortVertex at) {
  for (size_t axis = 0; axis < 3; ++axis) {
    into.PositionM[vertex * 3 + axis] =
        static_cast<float>(at.Position[axis] - at.DepthM * normal[axis]);
    if (at.Facing != 0) {
      into.NormalM[vertex * 3 + axis] = static_cast<float>(at.Facing) * normal[axis];
    }
  }
}

void BindRoadPort(const RoadPort &port, PortTarget target, RoadMeshBuffers &into) {
  const bool closed = target.Form == RibbonForm::ClosedShell;
  const size_t stride = closed ? kRibbonAcross * 2 + 4 : kRibbonAcross;
  const auto positions = RoadPortPositions(port);
  const auto normal = RoadPortNormal(port.Plane);
  const size_t station = target.FirstVertex + (target.AtEnd ? (target.Stations - 1) * stride : 0);
  for (size_t across = 0; across < kRibbonAcross; ++across) {
    const auto position = positions[target.AtEnd ? kRibbonAcross - 1 - across : across];
    StorePortVertex(into, station + across, normal, {.Position = position, .Facing = 1});
    if (closed) {
      StorePortVertex(into,
                      station + kRibbonAcross + across,
                      normal,
                      {.Position = position, .DepthM = port.Gate.ThicknessM, .Facing = -1});
    }
  }
  if (!closed) { return; }
  for (size_t side = 0; side < 2; ++side) {
    const size_t across = side == 0 ? 0 : kRibbonAcross - 1;
    const auto position = positions[target.AtEnd ? kRibbonAcross - 1 - across : across];
    const size_t wall = station + kRibbonAcross * 2 + side * 2;
    StorePortVertex(into, wall, normal, {.Position = position});
    StorePortVertex(into, wall + 1, normal, {.Position = position, .DepthM = port.Gate.ThicknessM});
  }
  const size_t cap =
      target.FirstVertex + target.Stations * stride + (target.AtEnd ? kRibbonAcross * 2 : 0);
  for (size_t corner = 0; corner < kRibbonAcross * 2; ++corner) {
    for (size_t axis = 0; axis < 3; ++axis) {
      into.PositionM[(cap + corner) * 3 + axis] = into.PositionM[(station + corner) * 3 + axis];
    }
  }
}

void BindRoadPorts(const Ribbon &woven,
                   const RoadSweep &how,
                   size_t firstVertex,
                   RoadMeshBuffers &into) {
  for (size_t end = 0; end < how.EndPorts.size(); ++end) {
    const auto &port = how.EndPorts[end];
    if (!port) { continue; }
    BindRoadPort(*port,
                 {.FirstVertex = firstVertex,
                  .Stations = woven.Stations,
                  .Form = how.Form,
                  .AtEnd = end != 0},
                 into);
  }
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
  for (size_t end = 0; end < how.EndPorts.size(); ++end) {
    const auto &endPort = how.EndPorts[end];
    if (!endPort) { continue; }
    Placed pose;
    if (!line.At(bank[end].AlongM, pose)) {
      ++rejections.Rise;
      return false;
    }
    const RoadPlane &plane = endPort->Plane;
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
                             RoadSection(how.HalfWidthM, how.Profile),
                             0.0,
                             line.LengthM(),
                             StepFor(tightestM),
                             how.Form);
  if (!woven.Woven) {
    ++rejections.Sweep;
    return false;
  }
  const size_t firstVertex = into.PositionM.size() / 3;
  Pour(woven, how.WearsLinear, into);
  BindRoadPorts(woven, how, firstVertex, into);
  return true;
}

RoadSweep
PieceSettings(RoadSweep how, std::span<const RoadStation> along, size_t first, size_t total) {
  const Section section = RoadSection(how.HalfWidthM, how.Profile);
  if (first != 0) {
    how.EndPorts[0] = RoadPort{.Gate = RoadEndGate(along, false, section), .Plane = {}};
  }
  if (first + along.size() != total) {
    how.EndPorts[1] = RoadPort{.Gate = RoadEndGate(along, true, section), .Plane = {}};
  }
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
    if (count >= 2u) {
      const RoadSweep piece = PieceSettings(how, along.subspan(from, count), from, along.size());
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
        const Section section = RoadSection(halfWidthM, how.Profile);
        const std::array corner{RoadEndGate(along.subspan(at - 1, 2), true, section),
                                RoadEndGate(along.subspan(at, 2), false, section)};
        Junction(std::span<const RoadGate>(corner.data(), 2), {}, wearsLinear, into);
      }
    }
    from += upTo;
  }
  return tally;
}
}
