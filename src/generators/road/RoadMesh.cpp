#include "math/Units.h"
#include "RoadMesh.h"
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

void Vertex(RoadRaised &into,
            const std::array<double, 3> &at,
            const Vec3 &normal,
            const Vec3f &wearsLinear) {
  for (const double one : at) { into.PositionM.push_back(static_cast<float>(Snapped(one))); }
  for (int axis = 0; axis < 3; ++axis) { into.NormalM.push_back(static_cast<float>(normal[axis])); }
  into.ColourRgba.insert(into.ColourRgba.end(),
                         {wearsLinear[0], wearsLinear[1], wearsLinear[2], 1.0f});
}

} // namespace

void RoadMesh::Junction(std::span<const RoadGate> gates,
                        const Vec3f &wearsLinear,
                        RoadRaised &into) const {
  if (gates.size() < 2) { return; }
  double centreE = 0.0;
  double centreS = 0.0;
  double centreGrade = 0.0;
  for (const auto &gate : gates) {
    centreE += gate.EastM;
    centreS += gate.SouthM;
    centreGrade += gate.GradeM;
  }
  centreE /= static_cast<double>(gates.size());
  centreS /= static_cast<double>(gates.size());
  centreGrade /= static_cast<double>(gates.size());

  struct Corner {
    double EastM = 0.0;
    double SouthM = 0.0;
    double GradeM = 0.0;
    double AroundRad = 0.0;
    size_t Gate = 0;
  };

  std::vector<Corner> around;
  around.reserve(gates.size() * 2u);
  for (size_t at = 0; at < gates.size(); ++at) {
    const RoadGate &gate = gates[at];
    const double sideE = -gate.OutS * gate.HalfWidthM;
    const double sideS = gate.OutE * gate.HalfWidthM;
    for (const double hand : {1.0, -1.0}) {
      const double eastM = gate.EastM + sideE * hand;
      const double southM = gate.SouthM + sideS * hand;
      around.push_back(Corner{.EastM = eastM,
                              .SouthM = southM,
                              .GradeM = gate.GradeM,
                              .AroundRad = std::atan2(southM - centreS, eastM - centreE),
                              .Gate = at * 2u + (hand > 0.0 ? 0u : 1u)});
    }
  }
  std::ranges::sort(around, [](const Corner &a, const Corner &b) {
    return a.AroundRad != b.AroundRad ? a.AroundRad < b.AroundRad : a.Gate < b.Gate;
  });
  const auto rim = static_cast<uint32_t>(around.size());
  const auto top = static_cast<uint32_t>(into.PositionM.size() / 3);
  Vertex(into, {centreE, centreGrade, centreS}, {{0.0, 1.0, 0.0}}, wearsLinear);
  for (const Corner &one : around) {
    Vertex(into, {one.EastM, one.GradeM, one.SouthM}, {{0.0, 1.0, 0.0}}, wearsLinear);
  }
  const auto bottom = static_cast<uint32_t>(into.PositionM.size() / 3);
  Vertex(into, {centreE, centreGrade - kSealedDepthM, centreS}, {{0.0, -1.0, 0.0}}, wearsLinear);
  for (const Corner &one : around) {
    Vertex(
        into, {one.EastM, one.GradeM - kSealedDepthM, one.SouthM}, {{0.0, -1.0, 0.0}}, wearsLinear);
  }
  for (uint32_t at = 0; at < rim; ++at) {
    const uint32_t next = (at + 1u) % rim;
    into.Index.insert(into.Index.end(), {top, top + 1u + next, top + 1u + at});
    into.Index.insert(into.Index.end(), {bottom, bottom + 1u + at, bottom + 1u + next});
    const Corner &here = around[at];
    const Corner &after = around[next];
    Vec3 outward = {{0.5 * (here.EastM + after.EastM) - centreE,
                     0.0,
                     0.5 * (here.SouthM + after.SouthM) - centreS}};
    const double run = std::sqrt(outward[0] * outward[0] + outward[2] * outward[2]);
    if (!(run > kParallelCross)) { continue; }
    outward[0] /= run;
    outward[2] /= run;
    const auto side = static_cast<uint32_t>(into.PositionM.size() / 3);
    Vertex(into, {here.EastM, here.GradeM, here.SouthM}, outward, wearsLinear);
    Vertex(into, {here.EastM, here.GradeM - kSealedDepthM, here.SouthM}, outward, wearsLinear);
    Vertex(into, {after.EastM, after.GradeM, after.SouthM}, outward, wearsLinear);
    Vertex(into, {after.EastM, after.GradeM - kSealedDepthM, after.SouthM}, outward, wearsLinear);
    into.Index.insert(into.Index.end(), {side, side + 1u, side + 3u, side, side + 3u, side + 2u});
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

void Pour(const Ribbon &woven, const Vec3f &wearsLinear, RoadRaised &into) {
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

bool LayPiece(std::span<const double> eastNorthM,
              std::span<const double> gradeM,
              std::span<const double> reachedM,
              double halfWidthM,
              RoadProfile profile,
              const Vec3f &wearsLinear,
              double crossfall,
              RoadRaised &into,
              RoadRefusals *why) {
  ReferenceLine line;
  double tightestM = 0.0;
  if (eastNorthM.size() == 4) {
    const double runE = eastNorthM[2] - eastNorthM[0];
    const double runN = eastNorthM[3] - eastNorthM[1];
    const double runM = std::sqrt(runE * runE + runN * runN);
    if (!(runM > 0.0)) {
      if (why != nullptr) { ++why->TooShort; }
      return false;
    }
    const Placed from{
        .EastM = eastNorthM[0], .NorthM = eastNorthM[1], .HeadingRad = std::atan2(runN, runE)};
    const Segment straight{.Shape = Curve::Straight, .LengthM = runM};
    std::string laidWhy;
    if (!line.Lay(from, std::span<const Segment>(&straight, 1), laidWhy)) {
      if (why != nullptr) { ++why->Fit; }
      return false;
    }
  } else {
    const Fitted laid = Fit(std::span<const double>(eastNorthM.data(), eastNorthM.size()),
                            kLayWithinM,
                            kLayTightestM,
                            line);
    if (!laid.Laid || !(line.LengthM() > 0.0)) {
      if (why != nullptr) { ++why->Fit; }
      return false;
    }
    tightestM = laid.TightestRadiusM;
  }

  const double wholeM = reachedM[reachedM.size() - 1u] - reachedM[0];
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
    rise.push_back(Knot{.AlongM = part * line.LengthM(), .Value = gradeM[one], .RatePerM = rate});
  }
  std::string said;
  if (!line.Rise(std::span<const Knot>(rise.data(), rise.size()), said)) {
    if (why != nullptr) { ++why->Rise; }
    return false;
  }
  const std::array<Knot, 2> bank = {
      {Knot{.AlongM = 0.0, .Value = crossfall, .RatePerM = 0.0},
       Knot{.AlongM = line.LengthM(), .Value = crossfall, .RatePerM = 0.0}}};
  if (!line.Bank(std::span<const Knot>(bank.data(), 2), said)) {
    if (why != nullptr) { ++why->Bank; }
    return false;
  }

  const Ribbon woven =
      Sweep(line, SectionFor(halfWidthM, profile), 0.0, line.LengthM(), StepFor(tightestM));
  if (!woven.Woven) {
    if (why != nullptr) { ++why->Sweep; }
    return false;
  }
  Pour(woven, wearsLinear, into);
  return true;
}

} // namespace

RoadTallied
RoadMesh::Sweep(std::span<const RoadStation> along, RoadSweep how, RoadRaised &into) const {
  const double halfWidthM = how.HalfWidthM;
  const RoadProfile profile = how.Profile;
  const Vec3f &wearsLinear = how.WearsLinear;
  const double crossfall = how.Crossfall;
  RoadTallied tally;
  RoadRefusals *const why = &tally.Why;
  if (along.size() < 3 || !(halfWidthM > 0.0)) { return tally; }

  std::vector<double> eastNorth;
  std::vector<double> grade;
  std::vector<double> reached;
  eastNorth.reserve(along.size() * 2u);
  grade.reserve(along.size());
  reached.reserve(along.size());
  for (size_t one = 0; one < along.size(); ++one) {
    eastNorth.push_back(along[one].EastM);
    eastNorth.push_back(-along[one].SouthM);
    grade.push_back(along[one].GradeM);
    if (one == 0) {
      reached.push_back(0.0);
    } else {
      const double spanE = along[one].EastM - along[one - 1u].EastM;
      const double spanS = along[one].SouthM - along[one - 1u].SouthM;
      reached.push_back(reached[one - 1u] + std::sqrt(spanE * spanE + spanS * spanS));
    }
  }

  size_t from = 0;
  while (from + 3u <= along.size()) {
    ReferenceLine probe;
    const Fitted got =
        Fit(std::span<const double>(eastNorth.data() + from * 2u, (along.size() - from) * 2u),
            kLayWithinM,
            kLayTightestM,
            probe);
    const size_t upTo = got.Laid ? along.size() - from - 1u : got.TightestDemandedAtVertex;
    const size_t count = upTo + 1u;
    if (count >= 2u) {
      if (LayPiece(std::span<const double>(eastNorth.data() + from * 2u, count * 2u),
                   std::span<const double>(grade.data() + from, count),
                   std::span<const double>(reached.data() + from, count),
                   halfWidthM,
                   profile,
                   wearsLinear,
                   crossfall,
                   into,
                   why)) {
        ++tally.Pieces;
      } else {
        ++tally.Refused;
      }
    } else {
      ++tally.Refused;
      if (why != nullptr) { ++why->TooShort; }
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
        const auto facing = [&](size_t one, size_t two) {
          const double runE = along[two].EastM - along[one].EastM;
          const double runS = along[two].SouthM - along[one].SouthM;
          const double runM = std::sqrt(runE * runE + runS * runS);
          return runM > kLeastTurnRad ? std::pair<double, double>{runE / runM, runS / runM}
                                      : std::pair<double, double>{0.0, 0.0};
        };
        const auto back = facing(at, at - 1u);
        const auto on = facing(at, at + 1u);
        const std::array<RoadGate, 2> corner = {{RoadGate{.EastM = along[at].EastM,
                                                          .SouthM = along[at].SouthM,
                                                          .GradeM = along[at].GradeM,
                                                          .OutE = back.first,
                                                          .OutS = back.second,
                                                          .HalfWidthM = halfWidthM},
                                                 RoadGate{.EastM = along[at].EastM,
                                                          .SouthM = along[at].SouthM,
                                                          .GradeM = along[at].GradeM,
                                                          .OutE = on.first,
                                                          .OutS = on.second,
                                                          .HalfWidthM = halfWidthM}}};
        Junction(std::span<const RoadGate>(corner.data(), 2), wearsLinear, into);
      }
    }
    from += upTo;
  }
  return tally;
}
} // namespace outshine::Generators
