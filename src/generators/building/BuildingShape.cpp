#include "math/Units.h"
#include "math/Vec2.h"
#include "BuildingShape.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include <utility>

#include "BuildingScratch.h"
#include "FacadeUv.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

constexpr double kOverhangEavesM = 0.42;

namespace {

constexpr double kFrontLeastLook = 0.35;
constexpr double kFrontLeastEdgeM = 2.2;
constexpr double kKerbNearestM = -0.4;
constexpr double kKerbFarthestM = -12.0;

constexpr uint32_t kPlotWord = 0x9e3779b9u;
constexpr uint32_t kMainWord = 0x27220a95u;
constexpr uint32_t kWingWord = 0x165667b1u;
constexpr uint32_t kOutbuildingWord = 0x3243f6a9u;

constexpr double kPlotTopFloor = 0.84;
constexpr double kPlotTopSwing = 0.32;
constexpr int kPlotTopStream = 11;
constexpr double kWingTopFloor = 0.56;
constexpr double kWingTopSwing = 0.16;
constexpr int kWingTopStream = 5;
constexpr double kWingLeastM = 3.0;

constexpr double kPeriodPerHalfM = 6.0;
constexpr double kSliverFill = 0.80;

constexpr double kSawtoothRiseShare = 0.30;
constexpr double kDomeRiseShare = 0.85;
constexpr double kMansardRiseShare = 0.62;
constexpr double kFlatRiseShare = 0.30;
constexpr double kRiseMostM = 11.0;
constexpr double kEavesLeastM = 2.40;
constexpr double kTallFloorM = 6.5;
constexpr double kBreakFracV = 0.42;
constexpr double kMansardBreakShare = 0.78;

constexpr uint32_t kIdentWord = 0x5bd1e995u;
constexpr double kPeriodLeastM = 6.0;
constexpr double kPeriodHalvesLeast = 2.0;
constexpr double kBayJitterFloor = 0.92;
constexpr double kBayJitterSwing = 0.16;
constexpr int kBayJitterStream = 7;
constexpr double kOverhangHallM = 0.25;

constexpr double kPlotLengthFactor = 2.2;
constexpr double kPlotAspectFactor = 2.4;
constexpr double kSliverShare = 0.4;

constexpr double kSameCornerM = 0.20;

constexpr double kOnCutM = 0.02;

int SideSign(double away) {
  if (away > kOnCutM) { return 1; }
  return away < -kOnCutM ? -1 : 0;
}

constexpr double kFloorHouseM = 2.85;
constexpr double kFloorBlockM = 3.15;
constexpr double kFloorHallM = 5.50;
constexpr double kFloorTowerM = 3.40;

constexpr double kPitchHouseDeg = 42.0;
constexpr double kPitchOutbuildingDeg = 22.0;
constexpr double kPitchHallDeg = 6.0;
constexpr double kPitchSpireDeg = 62.0;

constexpr double kPlotM = 8.5;

constexpr double kLeastPieceM2 = 16.0;
constexpr double kLeastPieceFrac = 0.16;

constexpr double kLeastTopM = 2.6;
constexpr double kSetbackM = 2.4;
constexpr double kWingUnderFill = 0.94;
constexpr double kDeepFromHalfM = 8.0;
constexpr int kDeepFromStoreys = 5;

constexpr double kOutbuildingUnderM2 = 26.0;
constexpr double kSpireOverM = 21.0;
constexpr double kSpireUnderM2 = 260.0;
constexpr double kTowerOverM = 19.0;
constexpr double kHallOverM2 = 1300.0;
constexpr double kBlockOverM2 = 380.0;
constexpr double kTerraceOverAspect = 2.2;
constexpr double kTerraceOverM2 = 90.0;

constexpr size_t kRoundLeastCorners = 8;
constexpr double kRoundOverFill = 0.70;
constexpr double kRoundUnderFill = 0.84;
constexpr double kRoundUnderAspect = 1.30;

constexpr double kPitchedMajority = 0.5;

constexpr double kPitchableFromFill = 0.74;
constexpr double kSawtoothOverAspect = 1.8;
constexpr double kSawtoothOverM2 = 2600.0;
constexpr int kMansardFromStoreys = 4;
constexpr double kBlockGableOverAspect = 1.9;
constexpr double kHouseGableFromAspect = 1.30;

constexpr double kParapetM = 0.90;
constexpr double kParapetLeastHalfVm = 2.2;

constexpr uint32_t kMixFirst = 0x7feb352du;
constexpr uint32_t kMixSecond = 0x846ca68bu;
constexpr uint32_t kGoldenWord = 0x9e3779b9u;
constexpr uint32_t kStreamWord = 0x85ebca6bu;
constexpr unsigned kMixShiftWide = 16u;
constexpr unsigned kMixShiftNarrow = 15u;

constexpr unsigned kMantissaBits = 24u;
constexpr unsigned kDropForMantissa = 32u - kMantissaBits;
constexpr double kMantissaSteps = static_cast<double>(1u << kMantissaBits);

constexpr double kMicroDegree = 1.0e6;

uint32_t Mix(uint32_t x) {
  x ^= x >> kMixShiftWide;
  x *= kMixFirst;
  x ^= x >> kMixShiftNarrow;
  x *= kMixSecond;
  x ^= x >> kMixShiftWide;
  return x;
}

uint32_t SeedOfPlace(LongitudeLatitude at) {
  const auto la = static_cast<int32_t>(std::llround(at.LatitudeDeg * kMicroDegree));
  const auto lo = static_cast<int32_t>(std::llround(at.LongitudeDeg * kMicroDegree));
  return Mix(static_cast<uint32_t>(la) * kGoldenWord ^ Mix(static_cast<uint32_t>(lo)));
}

double UnitOf(uint32_t seed, int stream) {
  return static_cast<double>(Mix(seed + static_cast<uint32_t>(stream) * kStreamWord) >>
                             kDropForMantissa) /
         kMantissaSteps;
}

void RingInMetres(std::span<const double> latLon, std::vector<En> &ring) {
  ring.clear();
  if (latLon.size() < 6) { return; }
  const double refLat = latLon[0];
  const double refLon = latLon[1];
  for (size_t k = 0; k + 1 < latLon.size(); k += 2) {
    const En p = EnuOffsetM({.LongitudeDeg = refLon, .LatitudeDeg = refLat},
                            {.LongitudeDeg = latLon[k + 1], .LatitudeDeg = latLon[k]});
    if (!ring.empty() &&
        std::hypot(p.EastM - ring.back().EastM, p.NorthM - ring.back().NorthM) < kSameCornerM) {
      continue;
    }
    ring.push_back(p);
  }
  while (ring.size() >= 2 && std::hypot(ring.front().EastM - ring.back().EastM,
                                        ring.front().NorthM - ring.back().NorthM) < kSameCornerM) {
    ring.pop_back();
  }
}

double SignedArea(std::span<const En> ring) {
  double a = 0.0;
  for (size_t i = 0, n = ring.size(); i < n; i++) {
    const En &p = ring[i];
    const En &q = ring[(i + 1) % n];
    a += p.EastM * q.NorthM - q.EastM * p.NorthM;
  }
  return 0.5 * a;
}

void WholeOf(std::span<const En> ring, Piece &p) {
  p.P.assign(ring.begin(), ring.end());
  p.Party.assign(ring.size(), 0u);
}

double SideOf(const En &at, const En &normal, const En &p) {
  return (p.EastM - at.EastM) * normal.EastM + (p.NorthM - at.NorthM) * normal.NorthM;
}

void DropSpurs(Piece *p) {
  bool again = true;
  while (again && p->P.size() > 3) {
    again = false;
    for (size_t i = 0; i < p->P.size(); i++) {
      const size_t n = p->P.size();
      const En &a = p->P[(i + n - 1) % n];
      const En &b = p->P[i];
      const En &c = p->P[(i + 1) % n];
      const double e1 = b.EastM - a.EastM;
      const double n1 = b.NorthM - a.NorthM;
      const double e2 = c.EastM - b.EastM;
      const double n2 = c.NorthM - b.NorthM;
      const double area2 = std::fabs(e1 * n2 - e2 * n1);
      if (area2 > kOnCutM * std::max(1.0, std::hypot(e1, n1) + std::hypot(e2, n2))) { continue; }
      if (e1 * e2 + n1 * n2 >= 0.0) { continue; }
      p->P.erase(p->P.begin() + static_cast<long>(i));
      p->Party.erase(p->Party.begin() + static_cast<long>(i));
      again = true;
      break;
    }
  }
}

struct Cut {
  En At;
  En Normal;
};

[[nodiscard]] bool CutPiece(const Piece &in,
                            Cut along,
                            BuildingScratch &scratch,
                            Piece *back,
                            Piece *front,
                            double *cutLenM) {
  const En &at = along.At;
  const En &normal = along.Normal;
  const size_t n = in.P.size();
  if (n < 3) { return false; }
  std::vector<double> &s = scratch.Side;
  std::vector<int> &sg = scratch.Sign;
  s.assign(n, 0.0);
  sg.assign(n, 0);
  for (size_t i = 0; i < n; i++) {
    s[i] = SideOf(at, normal, in.P[i]);
    sg[i] = SideSign(s[i]);
  }
  int last = 0;
  for (size_t i = 0; i < n; i++) {
    if (sg[i] != 0) { last = sg[i]; }
  }
  if (last == 0) { return false; }
  int crossings = 0;
  int cur = last;
  for (size_t i = 0; i < n; i++) {
    if (sg[i] == 0) { continue; }
    if (sg[i] != cur) { crossings++; }
    cur = sg[i];
  }
  if (crossings != 2) { return false; }

  const auto build = [&](Piece *out, int side) {
    out->P.clear();
    out->Party.clear();
    for (size_t i = 0; i < n; i++) {
      const size_t j = (i + 1) % n;
      const int si = sg[i] * side;
      const int sj = sg[j] * side;
      if (si >= 0) {
        out->P.push_back(in.P[i]);
        out->Party.push_back(si == 0 && sj < 0 ? 1u : in.Party[i]);
      }
      if (si * sj >= 0) { continue; }
      const double f = s[i] / (s[i] - s[j]);
      out->P.push_back({.EastM = in.P[i].EastM + (in.P[j].EastM - in.P[i].EastM) * f,
                        .NorthM = in.P[i].NorthM + (in.P[j].NorthM - in.P[i].NorthM) * f});
      out->Party.push_back(si > 0 ? 1u : in.Party[i]);
    }
  };
  build(front, 1);
  build(back, -1);
  DropSpurs(front);
  DropSpurs(back);
  if (front->P.size() < 3 || back->P.size() < 3) { return false; }

  double t0 = kBeyondAnyCoordinate;
  double t1 = -kBeyondAnyCoordinate;
  for (const En &p : front->P) {
    if (std::fabs(SideOf(at, normal, p)) > kOnCutM) { continue; }
    const double t = (p.EastM - at.EastM) * -normal.NorthM + (p.NorthM - at.NorthM) * normal.EastM;
    t0 = std::min(t0, t);
    t1 = std::max(t1, t);
  }
  *cutLenM = t1 > t0 ? t1 - t0 : 0.0;
  return true;
}

[[nodiscard]] bool BothWorthIt(const Piece &a, const Piece &b, double wholeM2) {
  const double least = std::max(kLeastPieceM2, kLeastPieceFrac * wholeM2);
  return std::fabs(SignedArea(a.P)) >= least && std::fabs(SignedArea(b.P)) >= least;
}

void MinAreaBox(std::span<const En> ring, BuildingShape *out) {
  double best = kBeyondAnyCoordinate;
  for (size_t i = 0, n = ring.size(); i < n; i++) {
    const En &p = ring[i];
    const En &q = ring[(i + 1) % n];
    double ax = q.EastM - p.EastM;
    double ay = q.NorthM - p.NorthM;
    const double len = std::hypot(ax, ay);
    if (len < kSameCornerM) { continue; }
    ax /= len;
    ay /= len;
    double u0 = kBeyondAnyCoordinate;
    double u1 = -kBeyondAnyCoordinate;
    double v0 = kBeyondAnyCoordinate;
    double v1 = -kBeyondAnyCoordinate;
    for (const En &r : ring) {
      const double u = r.EastM * ax + r.NorthM * ay;
      const double v = -r.EastM * ay + r.NorthM * ax;
      u0 = std::min(u0, u);
      u1 = std::max(u1, u);
      v0 = std::min(v0, v);
      v1 = std::max(v1, v);
    }
    const double area = (u1 - u0) * (v1 - v0);
    if (area >= best) { continue; }
    best = area;
    const double cu = 0.5 * (u0 + u1);
    const double cv = 0.5 * (v0 + v1);
    out->Centre = {.EastM = cu * ax - cv * ay, .NorthM = cu * ay + cv * ax};
    out->AxisU = {.EastM = ax, .NorthM = ay};
    out->HalfUm = 0.5 * (u1 - u0);
    out->HalfVm = 0.5 * (v1 - v0);
  }
  if (out->HalfUm < out->HalfVm) {
    std::swap(out->HalfUm, out->HalfVm);
    out->AxisU = {.EastM = -out->AxisU.NorthM, .NorthM = out->AxisU.EastM};
  }
}

struct Proportions {
  double AreaM2 = 0.0;
  double Aspect = 0.0;
  double HeightM = 0.0;
};

[[nodiscard]] BuildingUse UseOf(Proportions of) {
  if (of.AreaM2 < kOutbuildingUnderM2) { return BuildingUse::Outbuilding; }
  if (of.HeightM > kSpireOverM && of.AreaM2 < kSpireUnderM2) { return BuildingUse::Spire; }
  if (of.HeightM > kTowerOverM) { return BuildingUse::Tower; }
  if (of.AreaM2 > kHallOverM2) { return BuildingUse::Hall; }
  if (of.AreaM2 > kBlockOverM2) { return BuildingUse::Block; }
  if (of.Aspect > kTerraceOverAspect && of.AreaM2 > kTerraceOverM2) { return BuildingUse::Terrace; }
  return BuildingUse::House;
}

[[nodiscard]] bool ReadsAsRound(const BuildingShape &s) {
  return s.Ring.size() >= kRoundLeastCorners && s.Fill > kRoundOverFill &&
         s.Fill < kRoundUnderFill && s.HalfUm < kRoundUnderAspect * s.HalfVm;
}

[[nodiscard]] RoofKind RoofOf(const BuildingShape &s, double aspect, double pitchedShare) {
  if (ReadsAsRound(s)) { return RoofKind::Dome; }

  const bool pitchable =
      pitchedShare >= 0.0 ? pitchedShare >= kPitchedMajority : s.Fill >= kPitchableFromFill;

  switch (s.Use) {
    case BuildingUse::Outbuilding: return pitchable ? RoofKind::Shed : RoofKind::Flat;
    case BuildingUse::Spire: return pitchable ? RoofKind::Hip : RoofKind::Flat;
    case BuildingUse::Tower: return RoofKind::Flat;
    case BuildingUse::Hall:
      if (!pitchable) { return RoofKind::Flat; }
      return (aspect > kSawtoothOverAspect && s.AreaM2 > kSawtoothOverM2) ? RoofKind::Sawtooth
                                                                          : RoofKind::Flat;
    case BuildingUse::Block:
      if (!pitchable) { return RoofKind::Flat; }
      if (s.Storeys >= kMansardFromStoreys) { return RoofKind::Mansard; }
      return aspect > kBlockGableOverAspect ? RoofKind::Gable : RoofKind::Hip;
    case BuildingUse::Terrace: return pitchable ? RoofKind::Gable : RoofKind::Flat;
    case BuildingUse::House: break;
  }

  if (!pitchable) { return RoofKind::Flat; }
  return aspect >= kHouseGableFromAspect ? RoofKind::Gable : RoofKind::Hip;
}

double PitchDegOf(BuildingUse use, uint32_t seed, bool heightMeasured) {
  const double jitter = heightMeasured ? 0.0 : (UnitOf(seed, 3) - 0.5) * 9.0;
  switch (use) {
    case BuildingUse::Outbuilding: return kPitchOutbuildingDeg + jitter;
    case BuildingUse::Hall: return kPitchHallDeg;
    case BuildingUse::Spire: return kPitchSpireDeg;
    case BuildingUse::Tower:
    case BuildingUse::Block:
    case BuildingUse::Terrace:
    case BuildingUse::House: break;
  }
  return kPitchHouseDeg + jitter;
}

constexpr double kFloorOutbuildingM = 2.60;
constexpr double kFloorSpireM = 4.20;

constexpr double kBayOutbuildingM = 2.60;
constexpr double kBayHallM = 5.00;
constexpr double kBayTowerM = 3.40;
constexpr double kBaySpireM = 4.00;
constexpr double kBayBlockM = 3.60;
constexpr double kBayHouseM = 3.10;

double FloorPreferenceM(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return kFloorOutbuildingM;
    case BuildingUse::Hall: return kFloorHallM;
    case BuildingUse::Tower: return kFloorTowerM;
    case BuildingUse::Spire: return kFloorSpireM;
    case BuildingUse::Block: return kFloorBlockM;
    case BuildingUse::Terrace:
    case BuildingUse::House: break;
  }
  return kFloorHouseM;
}

double BayPreferenceM(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return kBayOutbuildingM;
    case BuildingUse::Hall: return kBayHallM;
    case BuildingUse::Tower: return kBayTowerM;
    case BuildingUse::Spire: return kBaySpireM;
    case BuildingUse::Block: return kBayBlockM;
    case BuildingUse::Terrace:
    case BuildingUse::House: break;
  }
  return kBayHouseM;
}

struct Roofing {
  double TopM = 0.0;
  double PitchDeg = 0.0;
};

void SplitHeight(BuildingShape *s, Roofing under) {
  const double halfSpan =
      s->Roof == RoofKind::Hip || s->Roof == RoofKind::Gable || s->Roof == RoofKind::Mansard
          ? s->HalfVm
          : s->HalfUm;
  double rise = 0.0;
  switch (s->Roof) {
    case RoofKind::Flat: rise = s->HalfVm > kParapetLeastHalfVm ? kParapetM : 0.0; break;
    case RoofKind::Shed: rise = 2.0 * s->HalfVm * std::tan(under.PitchDeg * kDeg2Rad); break;
    case RoofKind::Sawtooth: rise = kSawtoothRiseShare * s->PeriodM; break;
    case RoofKind::Dome: rise = kDomeRiseShare * s->HalfVm; break;
    case RoofKind::Mansard:
      rise = kMansardRiseShare * halfSpan * std::tan(under.PitchDeg * kDeg2Rad);
      break;
    case RoofKind::Gable:
    case RoofKind::Hip: rise = halfSpan * std::tan(under.PitchDeg * kDeg2Rad); break;
  }

  const double roofShare = s->Use == BuildingUse::Spire ? 0.72 : 0.45;
  s->RiseM = s->Roof == RoofKind::Flat ? std::min(rise, kFlatRiseShare * under.TopM)
                                       : std::min({rise, roofShare * under.TopM, kRiseMostM});
  s->EavesM = std::max(under.TopM - s->RiseM, kEavesLeastM);
  s->RiseM = std::max(under.TopM - s->EavesM, 0.0);

  const double want = FloorPreferenceM(s->Use);
  s->Storeys = std::max(1, static_cast<int>(std::lround(s->EavesM / want)));
  s->FloorM = s->EavesM / static_cast<double>(s->Storeys);
  if (s->FloorM > kTallFloorM) {
    s->Storeys = std::max(1, static_cast<int>(std::floor(s->EavesM / kTallFloorM)));
    s->FloorM = s->EavesM / static_cast<double>(s->Storeys);
  }
  s->BreakFracV = kBreakFracV;
  s->BreakRiseM = s->Roof == RoofKind::Mansard ? kMansardBreakShare * s->RiseM : 0.0;
}

struct PartOrder {
  double FootM = 0.0;
  double TopOverFootM = 0.0;
  double PitchedShare = -1.0;
  uint32_t Seed = 0;
  bool HeightMeasured = false;
  std::optional<BuildingUse> Use;
};

size_t TidyRing(std::vector<En> &ring, std::vector<uint8_t> &party) {
  if (ring.size() < 3) { return 0; }
  Vec2 least = {{ring[0].EastM, ring[0].NorthM}};
  Vec2 most = {{ring[0].EastM, ring[0].NorthM}};
  for (const En &p : ring) {
    least[0] = std::min(least[0], p.EastM);
    least[1] = std::min(least[1], p.NorthM);
    most[0] = std::max(most[0], p.EastM);
    most[1] = std::max(most[1], p.NorthM);
  }
  const double across = std::max(most[0] - least[0], most[1] - least[1]);
  const double flat = std::max(kLeastRunM * across * across, kLeastRunM);
  const double together = std::max(1.0e-3 * across, 1.0e-3);
  size_t dropped = 0;
  bool again = true;
  while (again && ring.size() > 3) {
    again = false;
    for (size_t at = 0; at < ring.size() && ring.size() > 3; ++at) {
      const size_t before = (at + ring.size() - 1) % ring.size();
      const size_t after = (at + 1) % ring.size();
      const Vec2 toward = {
          {ring[at].EastM - ring[before].EastM, ring[at].NorthM - ring[before].NorthM}};
      const Vec2 onward = {
          {ring[after].EastM - ring[at].EastM, ring[after].NorthM - ring[at].NorthM}};
      const double reach = std::sqrt(toward[0] * toward[0] + toward[1] * toward[1]);
      const double turn = std::fabs(toward[0] * onward[1] - toward[1] * onward[0]);
      if (reach > together && turn > 2.0 * flat) { continue; }
      ring.erase(ring.begin() + static_cast<long>(at));
      if (at < party.size()) { party.erase(party.begin() + static_cast<long>(at)); }
      ++dropped;
      again = true;
      break;
    }
  }
  return dropped;
}

void Finish(Piece &piece, const PartOrder &order, BuildingShape &s) {
  s.Ring.clear();
  s.Party.clear();
  s.TidiedAway = 0;
  s.FrontEdge = -1;
  if (piece.P.size() < 3) { return; }
  s.Ring.swap(piece.P);
  s.Party.swap(piece.Party);
  s.TidiedAway = TidyRing(s.Ring, s.Party);
  if (s.Ring.size() < 3) {
    s.Ring.clear();
    return;
  }
  const double signed2 = SignedArea(s.Ring);
  if (signed2 < 0.0) {
    std::ranges::reverse(s.Ring);

    std::ranges::reverse(s.Party);
    std::ranges::rotate(s.Party, s.Party.begin() + 1);
  }
  s.AreaM2 = std::fabs(signed2);
  MinAreaBox(s.Ring, &s);
  if (s.HalfUm < 0.5 || s.HalfVm < 0.5) {
    s.Ring.clear();
    return;
  }
  s.Fill = s.AreaM2 / (4.0 * s.HalfUm * s.HalfVm);
  s.Seed = order.Seed;
  s.Ident = static_cast<int>(Mix(order.Seed ^ kIdentWord) % static_cast<uint32_t>(kIdentCount));
  s.FootM = order.FootM;

  const double top = std::max(order.TopOverFootM, kLeastTopM);
  const double aspect = s.HalfUm / s.HalfVm;
  s.Use = order.Use ? *order.Use : UseOf({.AreaM2 = s.AreaM2, .Aspect = aspect, .HeightM = top});
  s.PeriodM = std::max(kPeriodLeastM,
                       kPeriodHalvesLeast * s.HalfUm /
                           std::max(kPeriodHalvesLeast, std::round(s.HalfUm / kPeriodPerHalfM)));
  s.Storeys = std::max(1, static_cast<int>(std::lround(top / FloorPreferenceM(s.Use))));
  s.Roof = RoofOf(s, aspect, order.PitchedShare);
  SplitHeight(&s, {.TopM = top, .PitchDeg = PitchDegOf(s.Use, s.Seed, order.HeightMeasured)});

  const double bay = BayPreferenceM(s.Use);
  s.BayM = bay * (kBayJitterFloor + kBayJitterSwing * UnitOf(s.Seed, kBayJitterStream));

  const bool verged = s.Roof != RoofKind::Flat && s.Roof != RoofKind::Dome;
  const double eaves = s.Use == BuildingUse::Hall ? kOverhangHallM : kOverhangEavesM;
  s.OverhangM = verged ? eaves : 0.0;
}

[[nodiscard]] bool IsReflex(std::span<const En> ring, size_t i) {
  const size_t n = ring.size();
  const En &a = ring[(i + n - 1) % n];
  const En &b = ring[i];
  const En &c = ring[(i + 1) % n];
  return (b.EastM - a.EastM) * (c.NorthM - b.NorthM) - (c.EastM - b.EastM) * (b.NorthM - a.NorthM) <
         0.0;
}

En UnitFrom(const En &a, const En &b) {
  const double e = b.EastM - a.EastM;
  const double n = b.NorthM - a.NorthM;
  const double l = std::hypot(e, n);
  return l < kLeastRunM ? En{.EastM = 1.0, .NorthM = 0.0} : En{.EastM = e / l, .NorthM = n / l};
}

[[nodiscard]] bool WingCut(const Piece &whole, BuildingScratch &scratch) {
  const std::vector<En> &ring = whole.P;
  const double wholeM2 = std::fabs(SignedArea(ring));
  double bestLen = kBeyondAnyCoordinate;
  bool found = false;
  Piece &a = scratch.Plot;
  Piece &b = scratch.Beyond;
  for (size_t i = 0; i < ring.size(); i++) {
    if (!IsReflex(ring, i)) { continue; }
    const std::array<En, 2> dirs = {{UnitFrom(ring[(i + ring.size() - 1) % ring.size()], ring[i]),
                                     UnitFrom(ring[i], ring[(i + 1) % ring.size()])}};
    for (const En &dir : dirs) {
      Piece &lo = scratch.Lo;
      Piece &hi = scratch.Hi;
      double len = 0.0;
      if (!CutPiece(whole,
                    {.At = ring[i], .Normal = {.EastM = dir.NorthM, .NorthM = -dir.EastM}},
                    scratch,
                    &lo,
                    &hi,
                    &len)) {
        continue;
      }
      if (len < 1.0 || len >= bestLen) { continue; }
      if (!BothWorthIt(lo, hi, wholeM2)) { continue; }
      a.P = lo.P;
      a.Party = lo.Party;
      b.P = hi.P;
      b.Party = hi.Party;
      bestLen = len;
      found = true;
    }
  }
  if (!found) { return false; }
  const bool aIsMain = std::fabs(SignedArea(a.P)) >= std::fabs(SignedArea(b.P));
  Piece &main = aIsMain ? a : b;
  Piece &wing = aIsMain ? b : a;
  scratch.Main.P.swap(main.P);
  scratch.Main.Party.swap(main.Party);
  scratch.Wing.P.swap(wing.P);
  scratch.Wing.Party.swap(wing.Party);
  return true;
}

int RowCut(const Piece &whole,
           const BuildingShape &box,
           BuildingScratch &scratch,
           std::span<Piece> out) {
  const double lengthM = 2.0 * box.HalfUm;

  if (lengthM < kPlotLengthFactor * kPlotM || box.HalfUm < kPlotAspectFactor * box.HalfVm ||
      box.Fill < kSliverFill) {
    return 0;
  }
  const int want =
      std::min(static_cast<int>(out.size()), static_cast<int>(std::lround(lengthM / kPlotM)));
  if (want < 2) { return 0; }
  const double step = lengthM / static_cast<double>(want);
  const double wholeM2 = std::fabs(SignedArea(whole.P));
  Piece &rest = scratch.Rest;
  rest.P = whole.P;
  rest.Party = whole.Party;
  int made = 0;
  for (int k = 1; k < want; k++) {
    const En at = box.FromBox({.U = -box.HalfUm + step * static_cast<double>(k), .V = 0.0});
    Piece &plot = scratch.Plot;
    Piece &beyond = scratch.Beyond;
    double len = 0.0;
    if (!CutPiece(rest, {.At = at, .Normal = box.AxisU}, scratch, &plot, &beyond, &len)) { break; }
    if (std::fabs(SignedArea(plot.P)) < std::max(kLeastPieceM2, kSliverShare * kPlotM * kPlotM)) {
      break;
    }
    if (std::fabs(SignedArea(beyond.P)) <
        std::max(kLeastPieceM2, kLeastPieceFrac * wholeM2 * kSliverShare)) {
      break;
    }
    out[made].P.swap(plot.P);
    out[made].Party.swap(plot.Party);
    ++made;
    rest.P.swap(beyond.P);
    rest.Party.swap(beyond.Party);
  }
  if (made == 0) { return 0; }
  out[made].P.swap(rest.P);
  out[made].Party.swap(rest.Party);
  ++made;
  return made;
}

double DistanceToKerb(const Frontage &street, const En &p) {
  return (p.EastM - street.KerbEm) * street.ToStreetE +
         (p.NorthM - street.KerbNm) * street.ToStreetN;
}

void FaceTheStreet(BuildingShape *s, const Frontage &street) {
  if (!street.Known || !s->OnGround()) { return; }
  const size_t n = s->Ring.size();
  double best = kFrontLeastLook;
  for (size_t i = 0; i < n; i++) {
    if (s->Party[i] != 0u) { continue; }
    const En &p = s->Ring[i];
    const En &q = s->Ring[(i + 1) % n];
    const double e = q.EastM - p.EastM;
    const double nn = q.NorthM - p.NorthM;
    const double len = std::hypot(e, nn);
    if (len < kFrontLeastEdgeM) { continue; }
    const double outE = nn / len;
    const double outN = -e / len;
    const double look = outE * street.ToStreetE + outN * street.ToStreetN;
    if (look <= best) { continue; }
    const double standBack = DistanceToKerb(
        street, {.EastM = 0.5 * (p.EastM + q.EastM), .NorthM = 0.5 * (p.NorthM + q.NorthM)});
    if (standBack > kKerbNearestM || standBack < kKerbFarthestM) { continue; }
    best = look;
    s->FrontEdge = static_cast<int>(i);
  }
}

} // namespace

Boxed BuildingShape::ToBox(const En &p) const {
  const double e = p.EastM - Centre.EastM;
  const double n = p.NorthM - Centre.NorthM;
  return {.U = e * AxisU.EastM + n * AxisU.NorthM, .V = -e * AxisU.NorthM + n * AxisU.EastM};
}

En BuildingShape::FromBox(Boxed at) const {
  const double u = at.U;
  const double v = at.V;
  return {.EastM = Centre.EastM + u * AxisU.EastM - v * AxisU.NorthM,
          .NorthM = Centre.NorthM + u * AxisU.NorthM + v * AxisU.EastM};
}

namespace {

void PlotParts(const PartOrder &whole, int plots, BuildingScratch &scratch) {
  const BuildingShape &one = scratch.One;
  for (int k = 0; k < plots; k++) {
    PartOrder o = whole;
    o.Seed = Mix(whole.Seed + kPlotWord * static_cast<uint32_t>(k + 1));
    o.TopOverFootM =
        whole.TopOverFootM * (kPlotTopFloor + kPlotTopSwing * UnitOf(o.Seed, kPlotTopStream));
    o.Use = one.Use == BuildingUse::Terrace || one.Use == BuildingUse::House
                ? std::optional<BuildingUse>(BuildingUse::Terrace)
                : std::optional<BuildingUse>();
    BuildingShape &part = scratch.Made;
    Finish(scratch.Row[static_cast<size_t>(k)], o, part);
    if (part.Valid()) { scratch.Parts.Next() = part; }
  }
}

void WingParts(const PartOrder &whole, BuildingScratch &scratch) {
  PartOrder m = whole;
  m.Seed = Mix(whole.Seed + kMainWord);
  BuildingShape &mainPart = scratch.Made;
  Finish(scratch.Main, m, mainPart);
  if (mainPart.Valid()) { scratch.Parts.Next() = mainPart; }
  PartOrder w = whole;
  w.Seed = Mix(whole.Seed + kWingWord);
  w.TopOverFootM = std::max(whole.TopOverFootM *
                                (kWingTopFloor + kWingTopSwing * UnitOf(w.Seed, kWingTopStream)),
                            kWingLeastM);
  BuildingShape &wingPart = scratch.Made;
  Finish(scratch.Wing, w, wingPart);
  if (wingPart.Valid()) { scratch.Parts.Next() = wingPart; }
}

void StackDeep(bool heightMeasured, BuildingScratch &scratch) {
  for (BuildingShape &s : scratch.Parts.Standing()) {
    const bool deep = std::min(s.HalfUm, s.HalfVm) >= kDeepFromHalfM &&
                      s.Storeys >= kDeepFromStoreys && s.Roof == RoofKind::Flat;
    std::vector<En> &inner = scratch.Inner;
    inner.clear();
    if (deep) { RoofSurface::Widened(s.Ring, -kSetbackM, {}, inner); }
    if (inner.size() < 3) {
      scratch.Stacked.Next() = s;
      continue;
    }
    const double lower = s.EavesM - s.FloorM;
    Piece &cap = scratch.Cap;
    cap.P.swap(inner);
    cap.Party.assign(cap.P.size(), 0u);
    PartOrder o;
    o.FootM = s.FootM + lower;
    o.TopOverFootM = s.EavesM + s.RiseM - lower;
    o.Seed = Mix(s.Seed + kOutbuildingWord);
    o.HeightMeasured = heightMeasured;
    o.Use = s.Use;
    BuildingShape &top = scratch.Made;
    Finish(cap, o, top);
    BuildingShape &base = scratch.Stacked.Next();
    base = s;
    SplitHeight(&base, {.TopM = lower, .PitchDeg = 0.0});
    if (top.Valid()) { scratch.Stacked.Next() = top; }
  }
  scratch.Parts.Swap(scratch.Stacked);
}

} // namespace

std::span<BuildingShape> MassOf(std::span<const double> ringLatLon,
                                Order order,
                                const Frontage &street,
                                BuildingScratch &scratch) {
  scratch.Parts.Reset();
  scratch.Stacked.Reset();
  std::vector<En> &outline = scratch.Outline;
  RingInMetres(ringLatLon, outline);
  if (outline.size() < 3) { return {}; }
  if (SignedArea(outline) < 0.0) { std::ranges::reverse(outline); }

  PartOrder whole;
  whole.TopOverFootM = std::max(order.HeightM, kLeastTopM);
  whole.Seed = SeedOfPlace({.LongitudeDeg = ringLatLon[1], .LatitudeDeg = ringLatLon[0]});
  whole.HeightMeasured = order.HeightMeasured;
  whole.PitchedShare = order.PitchedShare;
  BuildingShape &one = scratch.One;
  WholeOf(outline, scratch.Whole);
  Finish(scratch.Whole, whole, one);
  if (!one.Valid()) { return {}; }

  WholeOf(outline, scratch.Whole);
  const int plots = RowCut(scratch.Whole, one, scratch, scratch.Row);
  if (plots > 1) {
    PlotParts(whole, plots, scratch);
  } else if (plots == 0 && one.Fill < kWingUnderFill) {
    WholeOf(outline, scratch.Whole);
    if (WingCut(scratch.Whole, scratch)) { WingParts(whole, scratch); }
  }
  if (scratch.Parts.Count() == 0) { scratch.Parts.Next() = one; }

  StackDeep(order.HeightMeasured, scratch);
  for (BuildingShape &s : scratch.Parts.Standing()) { FaceTheStreet(&s, street); }
  return scratch.Parts.Standing();
}
} // namespace outshine::Generators
