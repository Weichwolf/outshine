#include "Units.h"
#include "ProfiledRoadMesher.h"
#include "RoadRibbonMesh.h"
#include "RoadCrossSection.h"
#include "Vec3.h"

#include "Fit.h"
#include "ReferenceLine.h"
#include "Ribbon.h"

#include <array>
#include <cmath>
#include <span>
#include <string>
#include <cstddef>
#include <vector>
#include <utility>

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

bool FitPiece(std::span<const double> eastNorthM,
              ReferenceLine &line,
              double &tightestM,
              RoadMeshingRejections &rejections) {
  tightestM = 0.0;
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
    const CurveFitResult laid = Fit(std::span<const double>(eastNorthM.data(), eastNorthM.size()),
                                    kLayWithinM,
                                    kLayTightestM,
                                    line);
    if (!laid.Succeeded || !(line.LengthM() > 0.0)) {
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

bool LayPiece(ReferenceLine line,
              double tightestM,
              ElevationSamples elevation,
              const RoadSweep &how,
              RoadMeshBuffers &into,
              RoadMeshingRejections &rejections,
              RoadDisplayRequest *display) {
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

  std::vector<double> requiredStations;
  double step = line.LengthM();
  if (display != nullptr) {
    auto plan = SampleRoadDisplay(line,
                                  RoadSection(how.HalfWidthM, how.Profile),
                                  display->ErrorM,
                                  display->Limits,
                                  RoadRibbonContacts(how));
    if (!plan) {
      ++rejections.Sweep;
      return false;
    }
    display->Limits.PoseQueriesMost -= plan->PoseQueries;
    display->Limits.StationsMost -= plan->StationsM.size();
    requiredStations = std::move(plan->StationsM);
  } else {
    step = StepFor(tightestM);
    requiredStations.reserve(rise.size());
    for (const Knot &knot : rise) { requiredStations.push_back(knot.AlongM); }
  }
  const Ribbon woven = Sweep(line,
                             RoadSection(how.HalfWidthM, how.Profile),
                             0.0,
                             line.LengthM(),
                             step,
                             how.Form,
                             requiredStations);
  if (!woven.Woven) {
    ++rejections.Sweep;
    return false;
  }
  AppendRoadRibbon(woven, how, into);
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

struct RoadSamples {
  std::vector<double> EastNorth;
  std::vector<double> Grade;
  std::vector<double> Reached;
};

RoadSamples GatherRoadSamples(std::span<const RoadStation> along) {
  RoadSamples result;
  result.EastNorth.reserve(along.size() * 2u);
  result.Grade.reserve(along.size());
  result.Reached.reserve(along.size());
  for (size_t one = 0; one < along.size(); ++one) {
    result.EastNorth.push_back(along[one].EastM);
    result.EastNorth.push_back(along[one].NorthM);
    result.Grade.push_back(along[one].GradeM);
    if (one == 0) {
      result.Reached.push_back(0.0);
    } else {
      const double spanE = along[one].EastM - along[one - 1u].EastM;
      const double spanN = along[one].NorthM - along[one - 1u].NorthM;
      result.Reached.push_back(result.Reached[one - 1u] + std::sqrt(spanE * spanE + spanN * spanN));
    }
  }

  return result;
}

bool AppendRoadCorner(const ProfiledRoadMesher &mesher,
                      std::span<const RoadStation> along,
                      size_t at,
                      const RoadSweep &surface,
                      RoadMeshBuffers &mesh) {
  if (at == 0 || at + 1 >= along.size()) { return true; }
  const Section section = RoadSection(surface.HalfWidthM, surface.Profile);
  const std::array gates{RoadEndGate(along.subspan(at - 1, 2), true, section),
                         RoadEndGate(along.subspan(at, 2), false, section)};
  const size_t before = mesh.Index.size();
  mesher.Junction(gates, {}, surface.WearsLinear, mesh);
  return mesh.Index.size() != before;
}

RoadMeshingStats AppendRoadPieces(const ProfiledRoadMesher &mesher,
                                  std::span<const RoadStation> along,
                                  const RoadSweep &how,
                                  const RoadSamples &samples,
                                  RoadMeshBuffers &into,
                                  RoadDisplayRequest *display) {
  RoadMeshingStats tally;
  RoadMeshingRejections &rejections = tally.Rejections;
  size_t from = 0;
  while (from + 2u <= along.size()) {
    ReferenceLine line;
    const CurveFitResult fit = Fit(
        std::span<const double>(samples.EastNorth.data() + from * 2u, (along.size() - from) * 2u),
        kLayWithinM,
        kLayTightestM,
        line);
    const size_t upTo = fit.Succeeded ? along.size() - from - 1u : fit.TightestDemandedAtVertex;
    const size_t count = upTo + 1u;
    if (count >= 2u) {
      const RoadSweep piece = PieceSettings(how, along.subspan(from, count), from, along.size());
      const bool reuse = fit.Succeeded && count > 2u;
      double tightestM = fit.TightestRadiusM;
      const bool fitted =
          reuse ||
          FitPiece(std::span<const double>(samples.EastNorth.data() + from * 2u, count * 2u),
                   line,
                   tightestM,
                   rejections);
      if (fitted &&
          LayPiece(std::move(line),
                   tightestM,
                   {.GradeM = std::span<const double>(samples.Grade.data() + from, count),
                    .ReachedM = std::span<const double>(samples.Reached.data() + from, count)},
                   piece,
                   into,
                   rejections,
                   display)) {
        ++tally.Pieces;
      } else {
        ++tally.Refused;
      }
    } else {
      ++tally.Refused;
      ++rejections.TooShort;
    }
    if (display != nullptr && tally.Refused != 0) { break; }
    if (fit.Succeeded) { break; }
    if (fit.Undrivable == 0 || fit.TightestDemandedAtVertex == 0) {
      ++tally.Refused;
      break;
    }
    ++tally.Cuts;
    const bool joined = AppendRoadCorner(mesher, along, from + upTo, how, into);
    if (display != nullptr && !joined) {
      ++tally.Refused;
      ++rejections.Sweep;
      break;
    }
    from += upTo;
  }
  return tally;
}

}

RoadMeshingStats ProfiledRoadMesher::Sweep(std::span<const RoadStation> along,
                                           RoadSweep how,
                                           RoadMeshBuffers &into) const {
  const double halfWidthM = how.HalfWidthM;
  RoadMeshingStats tally;
  RoadMeshingRejections &rejections = tally.Rejections;
  auto display = Display_;
  if (display && display->Policy == RoadDisplayPolicy::UpperWithOriginalBodies &&
      how.Form == RibbonForm::ClosedShell) {
    display.reset();
  }
  if (along.size() < 2 || !(halfWidthM > 0.0) || (display && how.Form != RibbonForm::Surface)) {
    if (display) {
      tally.Refused = 1;
      ++rejections.Sweep;
    }
    return tally;
  }
  const std::array before{
      into.PositionM.size(), into.NormalM.size(), into.ColourRgba.size(), into.Index.size()};

  const auto samples = GatherRoadSamples(along);
  tally = AppendRoadPieces(*this, along, how, samples, into, display ? &*display : nullptr);
  if (display && tally.Refused != 0) {
    into.PositionM.resize(before[0]);
    into.NormalM.resize(before[1]);
    into.ColourRgba.resize(before[2]);
    into.Index.resize(before[3]);
    tally.Pieces = 0;
  }
  return tally;
}
}
