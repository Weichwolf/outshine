#include "RoadMesh.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {

namespace {

constexpr double kCrossfall = 0.025;

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

void Push(RoadRaised &into, double eastM, double upM, double southM) {
  into.PositionM.push_back((float)Snapped(eastM));
  into.PositionM.push_back((float)Snapped(upM));
  into.PositionM.push_back((float)Snapped(southM));
  into.NormalM.insert(into.NormalM.end(), {0.0f, 1.0f, 0.0f});
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

void RaiseRoad(Span<const RoadStation> along,
               double halfWidthM,
               RoadProfile profile,
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
           here.SouthM + outS * cut.Held[at].AcrossM);
    }
    for (size_t at = 0; at < across; ++at) {
      const size_t mirrored = across - 1u - at;
      Push(into,
           here.EastM + outE * cut.Held[mirrored].AcrossM,
           here.GradeM + cut.Held[mirrored].UpM - cut.DepthM,
           here.SouthM + outS * cut.Held[mirrored].AcrossM);
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
