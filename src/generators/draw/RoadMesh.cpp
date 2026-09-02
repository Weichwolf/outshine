#include "Units.h"
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

constexpr double kSealedDepthM = 0.30;
constexpr double kTrackDepthM = 0.25;

constexpr double kSnapM = 0.001;

double Snapped(double metres) {
  return std::round(metres / kSnapM) * kSnapM;
}

void Push(RoadRaised &into, double eastM, double upM, double southM, const Vec3f &wearsLinear) {
  into.PositionM.push_back(static_cast<float>(Snapped(eastM)));
  into.PositionM.push_back(static_cast<float>(Snapped(upM)));
  into.PositionM.push_back(static_cast<float>(Snapped(southM)));
  into.NormalM.insert(into.NormalM.end(), {0.0f, 1.0f, 0.0f});
  into.ColourRgba.insert(into.ColourRgba.end(),
                         {wearsLinear[0], wearsLinear[1], wearsLinear[2], 1.0f});
}

void Facet(RoadRaised &into, uint32_t a, uint32_t b, uint32_t c) {
  const auto at = [&into](uint32_t one) { return &into.PositionM[static_cast<size_t>(one) * 3]; };
  const float *pa = at(a);
  const float *pb = at(b);
  const float *pc = at(c);
  const Vec3 u = {{static_cast<double>(pb[0]) - pa[0],
                   static_cast<double>(pb[1]) - pa[1],
                   static_cast<double>(pb[2]) - pa[2]}};
  const Vec3 v = {{static_cast<double>(pc[0]) - pa[0],
                   static_cast<double>(pc[1]) - pa[1],
                   static_cast<double>(pc[2]) - pa[2]}};
  const Vec3 n = {
      {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]}};
  const double run = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (!(run > kParallelCross)) { return; }
  for (const uint32_t one : {a, b, c}) {
    for (int axis = 0; axis < 3; ++axis) {
      into.NormalM[static_cast<size_t>(one) * 3 + static_cast<size_t>(axis)] +=
          static_cast<float>(n[axis] / run);
    }
  }
  into.Index.insert(into.Index.end(), {a, b, c});
}

} // namespace

void RaiseJunction(std::span<const RoadGate> gates, const Vec3f &wearsLinear, RoadRaised &into) {
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
    double EastM, SouthM, GradeM, AroundRad;
  };

  std::vector<Corner> around;
  around.reserve(gates.size() * 2u);
  for (const auto &gate : gates) {
    const double sideE = -gate.OutS * gate.HalfWidthM;
    const double sideS = gate.OutE * gate.HalfWidthM;
    for (const double hand : {1.0, -1.0}) {
      const double eastM = gate.EastM + sideE * hand;
      const double southM = gate.SouthM + sideS * hand;
      around.push_back(Corner{.EastM = eastM,
                              .SouthM = southM,
                              .GradeM = gate.GradeM,
                              .AroundRad = std::atan2(southM - centreS, eastM - centreE)});
    }
  }
  std::ranges::sort(around,
                    [](const Corner &a, const Corner &b) { return a.AroundRad < b.AroundRad; });

  const auto first = static_cast<uint32_t>(into.PositionM.size() / 3);
  Push(into, centreE, centreGrade, centreS, wearsLinear);
  Push(into, centreE, centreGrade - kSealedDepthM, centreS, wearsLinear);
  for (const Corner &one : around) {
    Push(into, one.EastM, one.GradeM, one.SouthM, wearsLinear);
    Push(into, one.EastM, one.GradeM - kSealedDepthM, one.SouthM, wearsLinear);
  }

  const auto rim = static_cast<uint32_t>(around.size());
  for (uint32_t at = 0; at < rim; ++at) {
    const uint32_t here = first + 2u + at * 2u;
    const uint32_t next = first + 2u + ((at + 1u) % rim) * 2u;
    Facet(into, first, here, next);
    Facet(into, first + 1u, next + 1u, here + 1u);
    Facet(into, here, here + 1u, next + 1u);
    Facet(into, here, next + 1u, next);
  }
}

namespace {

constexpr double kLayWithinM = 0.5;
constexpr double kLayTightestM = 5.5;
constexpr double kSagittaM = 0.20;
constexpr int kProfilePasses = 24;
constexpr double kLeastStepM = 2.0;
constexpr double kShutM = 0.5;
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

void DesignProfile(std::span<RoadStation> along, double mostGradient, double leastCrestK) {
  if (along.size() < 3 || !(mostGradient > 0.0) || !(leastCrestK > 0.0)) { return; }

  std::vector<double> reached(along.size(), 0.0);
  for (size_t one = 1; one < along.size(); ++one) {
    const double spanE = along[one].EastM - along[one - 1u].EastM;
    const double spanS = along[one].SouthM - along[one - 1u].SouthM;
    reached[one] = reached[one - 1u] + std::sqrt(spanE * spanE + spanS * spanS);
  }

  std::vector<double> grade(along.size() - 1u, 0.0);
  for (size_t one = 0; one + 1u < along.size(); ++one) {
    const double span = reached[one + 1u] - reached[one];
    grade[one] = span > kLeastTurnRad ? (along[one + 1u].GradeM - along[one].GradeM) / span : 0.0;
    grade[one] = std::clamp(grade[one], -mostGradient, mostGradient);
  }

  for (int pass = 0; pass < kProfilePasses; ++pass) {
    for (size_t one = 0; one + 2u < along.size(); ++one) {
      const double span = 0.5 * (reached[one + 2u] - reached[one]);
      if (!(span > kLeastTurnRad)) { continue; }
      const double most = span / (100.0 * leastCrestK);
      const double apart = grade[one + 1u] - grade[one];
      if (std::fabs(apart) <= most) { continue; }
      const double give = 0.5 * (std::fabs(apart) - most) * (apart > 0.0 ? 1.0 : -1.0);
      grade[one] += give;
      grade[one + 1u] -= give;
      grade[one] = std::clamp(grade[one], -mostGradient, mostGradient);
      grade[one + 1u] = std::clamp(grade[one + 1u], -mostGradient, mostGradient);
    }
  }

  std::vector<double> asFound(along.size(), 0.0);
  for (size_t one = 0; one < along.size(); ++one) { asFound[one] = along[one].GradeM; }
  for (size_t one = 1; one < along.size(); ++one) {
    along[one].GradeM =
        along[one - 1u].GradeM + grade[one - 1u] * (reached[one] - reached[one - 1u]);
  }
  {
    const double shutE = along[along.size() - 1u].EastM - along[0].EastM;
    const double shutS = along[along.size() - 1u].SouthM - along[0].SouthM;
    const double whole = reached[along.size() - 1u];
    if (shutE * shutE + shutS * shutS < kShutM * kShutM && whole > kLeastTurnRad) {
      const double adrift = along[along.size() - 1u].GradeM - along[0].GradeM;
      for (size_t one = 0; one < along.size(); ++one) {
        along[one].GradeM -= adrift * reached[one] / whole;
      }
    }
  }
  double apart = 0.0;
  size_t over = 0;
  for (size_t one = 0; one < along.size(); ++one) {
    if (along[one].Node == 0u) { continue; }
    apart += along[one].GradeM - asFound[one];
    ++over;
  }
  if (over == 0) {
    for (size_t one = 0; one < along.size(); ++one) { apart += along[one].GradeM - asFound[one]; }
    over = along.size();
  }
  apart /= static_cast<double>(over);
  for (auto &one : along) { one.GradeM -= apart; }
}

void SweepRoad(std::span<const RoadStation> along,
               double halfWidthM,
               RoadProfile profile,
               const Vec3f &wearsLinear,
               double crossfall,
               RoadRaised &into,
               size_t *piecesLaid,
               size_t *cutsMade,
               size_t *piecesRefused,
               RoadRefusals *why) {
  if (along.size() < 3 || !(halfWidthM > 0.0)) { return; }

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
        if (piecesLaid != nullptr) { ++*piecesLaid; }
      } else if (piecesRefused != nullptr) {
        ++*piecesRefused;
      }
    } else if (piecesRefused != nullptr) {
      ++*piecesRefused;
      if (why != nullptr) { ++why->TooShort; }
    }
    if (got.Laid) { break; }
    if (got.Undrivable == 0 || got.TightestDemandedAtVertex == 0) {
      if (piecesRefused != nullptr) { ++*piecesRefused; }
      break;
    }
    if (cutsMade != nullptr) { ++*cutsMade; }
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
        RaiseJunction(std::span<const RoadGate>(corner.data(), 2), wearsLinear, into);
      }
    }
    from += upTo;
  }
}
} // namespace outshine::Generators
