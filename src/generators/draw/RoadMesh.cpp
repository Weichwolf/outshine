#include "RoadMesh.h"

#include "Fit.h"
#include "ReferenceLine.h"
#include "Ribbon.h"

#include <cmath>
#include <span>
#include <string>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace outshine::Generators {

namespace {

constexpr double kKerbUpM = 0.14;
constexpr double kKerbAcrossM = 0.35;

constexpr double kShoulderDipM = 0.10;
constexpr double kShoulderFraction = 0.35;

constexpr double kSealedDepthM = 0.30;
constexpr double kTrackDepthM = 0.25;

constexpr size_t kMostAcross = 8;

struct Across {
  double AcrossM;
  double UpM;
};

struct Section {
  Across Held[kMostAcross];
  size_t Points = 0;
  double DepthM = 0.0;
};

Section SectionOf(double halfM, RoadProfile profile) {
  const double crown = halfM * kCrossfall;
  Section made;
  switch (profile) {
    case RoadProfile::Rounded: {
      const double outM = halfM * (1.0 + kShoulderFraction);
      made.Held[0] = {.AcrossM = -outM, .UpM = -kShoulderDipM};
      made.Held[1] = {.AcrossM = -halfM, .UpM = 0.0};
      made.Held[2] = {.AcrossM = 0.0, .UpM = crown};
      made.Held[3] = {.AcrossM = halfM, .UpM = 0.0};
      made.Held[4] = {.AcrossM = outM, .UpM = -kShoulderDipM};
      made.Points = 5;
      made.DepthM = kTrackDepthM;
      return made;
    }
    case RoadProfile::Kerbed: {
      made.Held[0] = {.AcrossM = -halfM - kKerbAcrossM, .UpM = kKerbUpM};
      made.Held[1] = {.AcrossM = -halfM, .UpM = kKerbUpM};
      made.Held[2] = {.AcrossM = -halfM, .UpM = 0.0};
      made.Held[3] = {.AcrossM = 0.0, .UpM = crown};
      made.Held[4] = {.AcrossM = halfM, .UpM = 0.0};
      made.Held[5] = {.AcrossM = halfM, .UpM = kKerbUpM};
      made.Held[6] = {.AcrossM = halfM + kKerbAcrossM, .UpM = kKerbUpM};
      made.Points = 7;
      made.DepthM = kSealedDepthM;
      return made;
    }
    case RoadProfile::Simple: break;
  }
  made.Held[0] = {.AcrossM = -halfM, .UpM = 0.0};
  made.Held[1] = {.AcrossM = 0.0, .UpM = crown};
  made.Held[2] = {.AcrossM = halfM, .UpM = 0.0};
  made.Points = 3;
  made.DepthM = kSealedDepthM;
  return made;
}

constexpr double kSnapM = 0.001;

double Snapped(double metres) {
  return std::round(metres / kSnapM) * kSnapM;
}

void Push(RoadRaised &into, double eastM, double upM, double southM, const float wearsLinear[3]) {
  into.PositionM.push_back((float)Snapped(eastM));
  into.PositionM.push_back((float)Snapped(upM));
  into.PositionM.push_back((float)Snapped(southM));
  into.NormalM.insert(into.NormalM.end(), {0.0f, 1.0f, 0.0f});
  into.ColourRgba.insert(into.ColourRgba.end(),
                         {wearsLinear[0], wearsLinear[1], wearsLinear[2], 1.0f});
}

void Facet(RoadRaised &into, uint32_t a, uint32_t b, uint32_t c) {
  const auto at = [&into](uint32_t one) { return &into.PositionM[(size_t)one * 3]; };
  const float *pa = at(a);
  const float *pb = at(b);
  const float *pc = at(c);
  const double u[3] = {(double)pb[0] - pa[0], (double)pb[1] - pa[1], (double)pb[2] - pa[2]};
  const double v[3] = {(double)pc[0] - pa[0], (double)pc[1] - pa[1], (double)pc[2] - pa[2]};
  const double n[3] = {
      u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
  const double run = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (!(run > 1.0e-12)) { return; }
  for (const uint32_t one : {a, b, c}) {
    for (int axis = 0; axis < 3; ++axis) {
      into.NormalM[(size_t)one * 3 + (size_t)axis] += (float)(n[axis] / run);
    }
  }
  into.Index.insert(into.Index.end(), {a, b, c});
}

} // namespace

void RaiseJunction(Span<const RoadGate> gates, const float wearsLinear[3], RoadRaised &into) {
  if (gates.Size() < 2) { return; }
  double centreE = 0.0;
  double centreS = 0.0;
  double centreGrade = 0.0;
  for (size_t at = 0; at < gates.Size(); ++at) {
    centreE += gates[at].EastM;
    centreS += gates[at].SouthM;
    centreGrade += gates[at].GradeM;
  }
  centreE /= (double)gates.Size();
  centreS /= (double)gates.Size();
  centreGrade /= (double)gates.Size();

  std::vector<size_t> order(gates.Size());
  for (size_t at = 0; at < order.size(); ++at) { order[at] = at; }
  std::ranges::sort(order, [&gates](size_t a, size_t b) {
    return std::atan2(gates[a].OutS, gates[a].OutE) < std::atan2(gates[b].OutS, gates[b].OutE);
  });

  const auto first = (uint32_t)(into.PositionM.size() / 3);
  Push(into, centreE, centreGrade, centreS, wearsLinear);
  Push(into, centreE, centreGrade - kSealedDepthM, centreS, wearsLinear);
  for (const size_t at : order) {
    const RoadGate &gate = gates[at];
    const double sideE = -gate.OutS * gate.HalfWidthM;
    const double sideS = gate.OutE * gate.HalfWidthM;
    for (const double hand : {1.0, -1.0}) {
      Push(into, gate.EastM + sideE * hand, gate.GradeM, gate.SouthM + sideS * hand, wearsLinear);
      Push(into,
           gate.EastM + sideE * hand,
           gate.GradeM - kSealedDepthM,
           gate.SouthM + sideS * hand,
           wearsLinear);
    }
  }

  const auto rim = (uint32_t)(gates.Size() * 2u);
  for (uint32_t at = 0; at < rim; ++at) {
    const uint32_t here = first + 2u + at * 2u;
    const uint32_t next = first + 2u + ((at + 1u) % rim) * 2u;
    Facet(into, first, here, next);
    Facet(into, first + 1u, next + 1u, here + 1u);
    Facet(into, here, here + 1u, next + 1u);
    Facet(into, here, next + 1u, next);
  }
}

void RaiseRoad(Span<const RoadStation> along,
               double halfWidthM,
               RoadProfile profile,
               const float wearsLinear[3],
               RoadRaised &into) {
  if (along.Size() < 2 || !(halfWidthM > 0.0)) { return; }
  const Section cut = SectionOf(halfWidthM, profile);
  const size_t across = cut.Points;
  const size_t ring = across * 2u;
  const auto first = (uint32_t)(into.PositionM.size() / 3);

  for (size_t station = 0; station < along.Size(); ++station) {
    const RoadStation &here = along[station];
    const RoadStation &back = along[station == 0 ? station : station - 1];
    const RoadStation &next = along[station + 1 < along.Size() ? station + 1 : station];
    double alongE = next.EastM - back.EastM;
    double alongS = next.SouthM - back.SouthM;
    const double run = std::sqrt(alongE * alongE + alongS * alongS);
    if (!(run > 1.0e-9)) {
      alongE = 1.0;
      alongS = 0.0;
    } else {
      alongE /= run;
      alongS /= run;
    }
    const double outE = -alongS;
    const double outS = alongE;
    for (size_t at = 0; at < across; ++at) {
      Push(into,
           here.EastM + outE * cut.Held[at].AcrossM,
           here.GradeM + cut.Held[at].UpM,
           here.SouthM + outS * cut.Held[at].AcrossM,
           wearsLinear);
    }
    for (size_t at = 0; at < across; ++at) {
      const size_t mirrored = across - 1u - at;
      Push(into,
           here.EastM + outE * cut.Held[mirrored].AcrossM,
           here.GradeM + cut.Held[mirrored].UpM - cut.DepthM,
           here.SouthM + outS * cut.Held[mirrored].AcrossM,
           wearsLinear);
    }
  }

  for (size_t station = 0; station + 1 < along.Size(); ++station) {
    const auto here = (uint32_t)(first + station * ring);
    const auto next = (uint32_t)(here + ring);
    for (size_t at = 0; at < ring; ++at) {
      const auto step = (uint32_t)((at + 1u) % ring);
      Facet(into, here + (uint32_t)at, next + (uint32_t)at, next + step);
      Facet(into, here + (uint32_t)at, next + step, here + step);
    }
  }

  for (const size_t station : {(size_t)0, along.Size() - 1u}) {
    const auto ringAt = (uint32_t)(first + station * ring);
    for (size_t at = 1; at + 1 < ring; ++at) {
      if (station == 0) {
        Facet(into, ringAt, ringAt + (uint32_t)at + 1u, ringAt + (uint32_t)at);
      } else {
        Facet(into, ringAt, ringAt + (uint32_t)at, ringAt + (uint32_t)at + 1u);
      }
    }
  }

  for (size_t one = first; one * 3 + 2 < into.NormalM.size(); ++one) {
    float *held = &into.NormalM[one * 3];
    const double run = std::sqrt((double)held[0] * held[0] + (double)held[1] * held[1] +
                                 (double)held[2] * held[2]);
    if (run > 1.0e-9) {
      for (int axis = 0; axis < 3; ++axis) { held[axis] = (float)(held[axis] / run); }
    } else {
      held[0] = 0.0f;
      held[1] = 1.0f;
      held[2] = 0.0f;
    }
  }
}

} // namespace outshine::Generators

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

outshine::Section SectionFor(double halfWidthM, RoadProfile profile) {
  outshine::Section cut;
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

void Pour(const Ribbon &woven, const float wearsLinear[3], RoadRaised &into) {
  const auto firstVertex = (uint32_t)(into.PositionM.size() / 3u);
  const size_t vertices = woven.PositionM.size() / 3u;
  for (size_t one = 0; one < vertices; ++one) {
    into.PositionM.push_back((float)(woven.PositionM[one * 3u] + woven.OriginM[0]));
    into.PositionM.push_back((float)(woven.PositionM[one * 3u + 1u] + woven.OriginM[1]));
    into.PositionM.push_back((float)(woven.PositionM[one * 3u + 2u] + woven.OriginM[2]));
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

bool LayPiece(Span<const double> eastNorthM,
              Span<const double> gradeM,
              Span<const double> reachedM,
              double halfWidthM,
              RoadProfile profile,
              const float wearsLinear[3],
              double crossfall,
              RoadRaised &into) {
  ReferenceLine line;
  const Fitted laid = Fit(std::span<const double>(eastNorthM.Data(), eastNorthM.Size()),
                          kLayWithinM,
                          kLayTightestM,
                          line);
  if (!laid.Laid || !(line.LengthM() > 0.0)) { return false; }

  const double wholeM = reachedM[reachedM.Size() - 1u] - reachedM[0];
  std::vector<Knot> rise;
  rise.reserve(gradeM.Size());
  for (size_t one = 0; one < gradeM.Size(); ++one) {
    const double part = wholeM > 1.0e-9 ? (reachedM[one] - reachedM[0]) / wholeM : 0.0;
    double rate = 0.0;
    if (one + 1u < gradeM.Size()) {
      const double span = reachedM[one + 1u] - reachedM[one];
      rate = span > 1.0e-9 ? (gradeM[one + 1u] - gradeM[one]) / span : 0.0;
    } else if (one > 0) {
      const double span = reachedM[one] - reachedM[one - 1u];
      rate = span > 1.0e-9 ? (gradeM[one] - gradeM[one - 1u]) / span : 0.0;
    }
    rise.push_back(Knot{.AlongM = part * line.LengthM(), .Value = gradeM[one], .RatePerM = rate});
  }
  std::string why;
  if (!line.Rise(std::span<const Knot>(rise.data(), rise.size()), why)) { return false; }
  const Knot bank[2] = {Knot{.AlongM = 0.0, .Value = crossfall, .RatePerM = 0.0},
                        Knot{.AlongM = line.LengthM(), .Value = crossfall, .RatePerM = 0.0}};
  if (!line.Bank(std::span<const Knot>(bank, 2), why)) { return false; }

  const Ribbon woven = Sweep(
      line, SectionFor(halfWidthM, profile), 0.0, line.LengthM(), StepFor(laid.TightestRadiusM));
  if (!woven.Woven) { return false; }
  Pour(woven, wearsLinear, into);
  return true;
}

} // namespace

void SweepRoad(Span<const RoadStation> along,
               double halfWidthM,
               RoadProfile profile,
               const float wearsLinear[3],
               double crossfall,
               RoadRaised &into,
               size_t *piecesLaid,
               size_t *cutsMade,
               size_t *piecesRefused) {
  if (along.Size() < 3 || !(halfWidthM > 0.0)) { return; }

  std::vector<double> eastNorth;
  std::vector<double> grade;
  std::vector<double> reached;
  eastNorth.reserve(along.Size() * 2u);
  grade.reserve(along.Size());
  reached.reserve(along.Size());
  for (size_t one = 0; one < along.Size(); ++one) {
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
  while (from + 3u <= along.Size()) {
    ReferenceLine probe;
    const Fitted got =
        Fit(std::span<const double>(eastNorth.data() + from * 2u, (along.Size() - from) * 2u),
            kLayWithinM,
            kLayTightestM,
            probe);
    const size_t upTo = got.Laid ? along.Size() - from - 1u : got.TightestDemandedAtVertex;
    const size_t count = upTo + 1u;
    if (count >= 3u) {
      if (LayPiece(Span<const double>(eastNorth.data() + from * 2u, count * 2u),
                   Span<const double>(grade.data() + from, count),
                   Span<const double>(reached.data() + from, count),
                   halfWidthM,
                   profile,
                   wearsLinear,
                   crossfall,
                   into)) {
        if (piecesLaid != nullptr) { ++*piecesLaid; }
      } else if (piecesRefused != nullptr) {
        ++*piecesRefused;
      }
    } else if (piecesRefused != nullptr) {
      ++*piecesRefused;
    }
    if (got.Laid) { break; }
    if (got.Undrivable == 0 || got.TightestDemandedAtVertex == 0) {
      if (piecesRefused != nullptr) { ++*piecesRefused; }
      break;
    }
    if (cutsMade != nullptr) { ++*cutsMade; }
    from += upTo + 1u;
  }
}

} // namespace outshine::Generators
