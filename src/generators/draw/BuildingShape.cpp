#include "BuildingShape.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "FacadeUv.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

namespace {

constexpr double kSameCornerM = 0.20;

constexpr double kOnCutM = 0.02;

constexpr double kFloorHouseM = 2.85;
constexpr double kFloorBlockM = 3.15;
constexpr double kFloorHallM = 5.50;
constexpr double kFloorTowerM = 3.40;

constexpr double kPitchHouseDeg = 42.0;
constexpr double kPitchOutbuildingDeg = 22.0;
constexpr double kPitchHallDeg = 6.0;
constexpr double kPitchSpireDeg = 62.0;

constexpr double kPlotM = 8.5;
constexpr int kMaxParts = 9;

constexpr double kLeastPieceM2 = 16.0;
constexpr double kLeastPieceFrac = 0.16;

constexpr double kSetbackM = 2.4;

constexpr double kParapetM = 0.90;
constexpr double kParapetLeastHalfVm = 2.2;

uint32_t Mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

uint32_t SeedOfPlace(double latDeg, double lonDeg) {
  const int32_t la = static_cast<int32_t>(std::llround(latDeg * 1.0e6));
  const int32_t lo = static_cast<int32_t>(std::llround(lonDeg * 1.0e6));
  return Mix(static_cast<uint32_t>(la) * 0x9e3779b9u ^ Mix(static_cast<uint32_t>(lo)));
}

double UnitOf(uint32_t seed, int stream) {
  return static_cast<double>(Mix(seed + static_cast<uint32_t>(stream) * 0x85ebca6bu) >> 8) *
         (1.0 / 16777216.0);
}

std::vector<En> RingInMetres(Span<const double> latLon) {
  std::vector<En> ring;
  if (latLon.Size() < 6) { return ring; }
  const double refLat = latLon[0], refLon = latLon[1];
  ring.reserve(latLon.Size() / 2);
  for (size_t k = 0; k + 1 < latLon.Size(); k += 2) {
    En p;
    EnuOffsetM(refLat, refLon, latLon[k], latLon[k + 1], p.E, p.N);
    if (!ring.empty() && std::hypot(p.E - ring.back().E, p.N - ring.back().N) < kSameCornerM) {
      continue;
    }
    ring.push_back(p);
  }
  while (ring.size() >= 2 && std::hypot(ring.front().E - ring.back().E,
                                        ring.front().N - ring.back().N) < kSameCornerM) {
    ring.pop_back();
  }
  return ring;
}

double SignedArea(const std::vector<En> &ring) {
  double a = 0.0;
  for (size_t i = 0, n = ring.size(); i < n; i++) {
    const En &p = ring[i], &q = ring[(i + 1) % n];
    a += p.E * q.N - q.E * p.N;
  }
  return 0.5 * a;
}

struct Piece {
  std::vector<En> P;
  std::vector<uint8_t> Party;
};

Piece WholeOf(const std::vector<En> &ring) {
  Piece p;
  p.P = ring;
  p.Party.assign(ring.size(), 0u);
  return p;
}

double SideOf(const En &at, const En &normal, const En &p) {
  return (p.E - at.E) * normal.E + (p.N - at.N) * normal.N;
}

void DropSpurs(Piece *p) {
  bool again = true;
  while (again && p->P.size() > 3) {
    again = false;
    for (size_t i = 0; i < p->P.size(); i++) {
      const size_t n = p->P.size();
      const En &a = p->P[(i + n - 1) % n], &b = p->P[i], &c = p->P[(i + 1) % n];
      const double e1 = b.E - a.E, n1 = b.N - a.N, e2 = c.E - b.E, n2 = c.N - b.N;
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

[[nodiscard]] bool CutPiece(
    const Piece &in, const En &at, const En &normal, Piece *back, Piece *front, double *cutLenM) {
  const size_t n = in.P.size();
  if (n < 3) { return false; }
  std::vector<double> s(n);
  std::vector<int> sg(n);
  for (size_t i = 0; i < n; i++) {
    s[i] = SideOf(at, normal, in.P[i]);
    sg[i] = s[i] > kOnCutM ? 1 : (s[i] < -kOnCutM ? -1 : 0);
  }
  int last = 0;
  for (size_t i = 0; i < n; i++) {
    if (sg[i] != 0) { last = sg[i]; }
  }
  if (last == 0) { return false; }
  int crossings = 0, cur = last;
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
      const int si = sg[i] * side, sj = sg[j] * side;
      if (si >= 0) {
        out->P.push_back(in.P[i]);
        out->Party.push_back(si == 0 && sj < 0 ? 1u : in.Party[i]);
      }
      if (si * sj >= 0) { continue; }
      const double f = s[i] / (s[i] - s[j]);
      out->P.push_back({.E = in.P[i].E + (in.P[j].E - in.P[i].E) * f,
                        .N = in.P[i].N + (in.P[j].N - in.P[i].N) * f});
      out->Party.push_back(si > 0 ? 1u : in.Party[i]);
    }
  };
  build(front, 1);
  build(back, -1);
  DropSpurs(front);
  DropSpurs(back);
  if (front->P.size() < 3 || back->P.size() < 3) { return false; }

  double t0 = 1.0e30, t1 = -1.0e30;
  for (const En &p : front->P) {
    if (std::fabs(SideOf(at, normal, p)) > kOnCutM) { continue; }
    const double t = (p.E - at.E) * -normal.N + (p.N - at.N) * normal.E;
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

void MinAreaBox(const std::vector<En> &ring, BuildingShape *out) {
  double best = 1.0e30;
  for (size_t i = 0, n = ring.size(); i < n; i++) {
    const En &p = ring[i], &q = ring[(i + 1) % n];
    double ax = q.E - p.E, ay = q.N - p.N;
    const double len = std::hypot(ax, ay);
    if (len < kSameCornerM) { continue; }
    ax /= len;
    ay /= len;
    double u0 = 1e30, u1 = -1e30, v0 = 1e30, v1 = -1e30;
    for (const En &r : ring) {
      const double u = r.E * ax + r.N * ay, v = -r.E * ay + r.N * ax;
      u0 = std::min(u0, u);
      u1 = std::max(u1, u);
      v0 = std::min(v0, v);
      v1 = std::max(v1, v);
    }
    const double area = (u1 - u0) * (v1 - v0);
    if (area >= best) { continue; }
    best = area;
    const double cu = 0.5 * (u0 + u1), cv = 0.5 * (v0 + v1);
    out->Centre = {.E = cu * ax - cv * ay, .N = cu * ay + cv * ax};
    out->AxisU = {.E = ax, .N = ay};
    out->HalfUm = 0.5 * (u1 - u0);
    out->HalfVm = 0.5 * (v1 - v0);
  }
  if (out->HalfUm < out->HalfVm) {
    std::swap(out->HalfUm, out->HalfVm);
    out->AxisU = {.E = -out->AxisU.N, .N = out->AxisU.E};
  }
}

[[nodiscard]] BuildingUse UseOf(double areaM2, double aspect, double heightM) {
  if (areaM2 < 26.0) { return BuildingUse::Outbuilding; }
  if (heightM > 21.0 && areaM2 < 260.0) { return BuildingUse::Spire; }
  if (heightM > 19.0) { return BuildingUse::Tower; }
  if (areaM2 > 1300.0) { return BuildingUse::Hall; }
  if (areaM2 > 380.0) { return BuildingUse::Block; }
  if (aspect > 2.2 && areaM2 > 90.0) { return BuildingUse::Terrace; }
  return BuildingUse::House;
}

[[nodiscard]] bool ReadsAsRound(const BuildingShape &s) {
  return s.Ring.size() >= 8 && s.Fill > 0.70 && s.Fill < 0.84 && s.HalfUm < 1.30 * s.HalfVm;
}

[[nodiscard]] RoofKind RoofOf(const BuildingShape &s, double aspect) {
  if (ReadsAsRound(s)) { return RoofKind::Dome; }
  const bool pitchable = s.Fill >= 0.74;
  switch (s.Use) {
    case BuildingUse::Outbuilding: return pitchable ? RoofKind::Shed : RoofKind::Flat;
    case BuildingUse::Spire: return pitchable ? RoofKind::Hip : RoofKind::Flat;
    case BuildingUse::Tower: return RoofKind::Flat;
    case BuildingUse::Hall:
      if (!pitchable) { return RoofKind::Flat; }
      return (aspect > 1.8 && s.AreaM2 > 2600.0) ? RoofKind::Sawtooth : RoofKind::Flat;
    case BuildingUse::Block:
      if (!pitchable) { return RoofKind::Flat; }
      if (s.Storeys >= 4) { return RoofKind::Mansard; }
      return aspect > 1.9 ? RoofKind::Gable : RoofKind::Hip;
    case BuildingUse::Terrace: return pitchable ? RoofKind::Gable : RoofKind::Flat;
    case BuildingUse::House: break;
  }
  if (!pitchable) { return RoofKind::Flat; }
  return aspect >= 1.30 ? RoofKind::Gable : RoofKind::Hip;
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

double FloorPreferenceM(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return 2.60;
    case BuildingUse::Hall: return kFloorHallM;
    case BuildingUse::Tower: return kFloorTowerM;
    case BuildingUse::Spire: return 4.20;
    case BuildingUse::Block: return kFloorBlockM;
    case BuildingUse::Terrace:
    case BuildingUse::House: break;
  }
  return kFloorHouseM;
}

double BayPreferenceM(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return 2.60;
    case BuildingUse::Hall: return 5.00;
    case BuildingUse::Tower: return 3.40;
    case BuildingUse::Spire: return 4.00;
    case BuildingUse::Block: return 3.60;
    case BuildingUse::Terrace:
    case BuildingUse::House: break;
  }
  return 3.10;
}

void SplitHeight(BuildingShape *s, double topM, double pitchDeg) {
  const double halfSpan =
      s->Roof == RoofKind::Hip || s->Roof == RoofKind::Gable || s->Roof == RoofKind::Mansard
          ? s->HalfVm
          : s->HalfUm;
  double rise = 0.0;
  switch (s->Roof) {
    case RoofKind::Flat: rise = s->HalfVm > kParapetLeastHalfVm ? kParapetM : 0.0; break;
    case RoofKind::Shed: rise = 2.0 * s->HalfVm * std::tan(pitchDeg * kDeg2Rad); break;
    case RoofKind::Sawtooth: rise = 0.30 * s->PeriodM; break;
    case RoofKind::Dome: rise = 0.85 * s->HalfVm; break;
    case RoofKind::Mansard: rise = 0.62 * halfSpan * std::tan(pitchDeg * kDeg2Rad); break;
    case RoofKind::Gable:
    case RoofKind::Hip: rise = halfSpan * std::tan(pitchDeg * kDeg2Rad); break;
  }

  const double roofShare = s->Use == BuildingUse::Spire ? 0.72 : 0.45;
  s->RiseM = s->Roof == RoofKind::Flat ? std::min(rise, 0.30 * topM)
                                       : std::min({rise, roofShare * topM, 11.0});
  s->EavesM = std::max(topM - s->RiseM, 2.40);
  s->RiseM = std::max(topM - s->EavesM, 0.0);

  const double want = FloorPreferenceM(s->Use);
  s->Storeys = std::max(1, static_cast<int>(std::lround(s->EavesM / want)));
  s->FloorM = s->EavesM / static_cast<double>(s->Storeys);
  if (s->FloorM > 6.5) {
    s->Storeys = std::max(1, static_cast<int>(std::floor(s->EavesM / 6.5)));
    s->FloorM = s->EavesM / static_cast<double>(s->Storeys);
  }
  s->BreakFracV = 0.42;
  s->BreakRiseM = s->Roof == RoofKind::Mansard ? 0.78 * s->RiseM : 0.0;
}

struct PartOrder {
  double FootM = 0.0;
  double TopOverFootM = 0.0;
  uint32_t Seed = 0;
  bool HeightMeasured = false;
  std::optional<BuildingUse> Use;
};

size_t TidyRing(std::vector<En> &ring, std::vector<uint8_t> &party) {
  if (ring.size() < 3) { return 0; }
  double least[2] = {ring[0].E, ring[0].N}, most[2] = {ring[0].E, ring[0].N};
  for (const En &p : ring) {
    least[0] = std::min(least[0], p.E);
    least[1] = std::min(least[1], p.N);
    most[0] = std::max(most[0], p.E);
    most[1] = std::max(most[1], p.N);
  }
  const double across = std::max(most[0] - least[0], most[1] - least[1]);
  const double flat = std::max(1.0e-6 * across * across, 1.0e-6);
  const double together = std::max(1.0e-3 * across, 1.0e-3);
  size_t dropped = 0;
  bool again = true;
  while (again && ring.size() > 3) {
    again = false;
    for (size_t at = 0; at < ring.size() && ring.size() > 3; ++at) {
      const size_t before = (at + ring.size() - 1) % ring.size();
      const size_t after = (at + 1) % ring.size();
      const double toward[2] = {ring[at].E - ring[before].E, ring[at].N - ring[before].N};
      const double onward[2] = {ring[after].E - ring[at].E, ring[after].N - ring[at].N};
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

BuildingShape Finish(Piece piece, const PartOrder &order) {
  BuildingShape s;
  if (piece.P.size() < 3) { return s; }
  s.Ring = std::move(piece.P);
  s.Party = std::move(piece.Party);
  s.TidiedAway = TidyRing(s.Ring, s.Party);
  if (s.Ring.size() < 3) { return BuildingShape{}; }
  const double signed2 = SignedArea(s.Ring);
  if (signed2 < 0.0) {
    std::reverse(s.Ring.begin(), s.Ring.end());

    std::reverse(s.Party.begin(), s.Party.end());
    std::rotate(s.Party.begin(), s.Party.begin() + 1, s.Party.end());
  }
  s.AreaM2 = std::fabs(signed2);
  MinAreaBox(s.Ring, &s);
  if (s.HalfUm < 0.5 || s.HalfVm < 0.5) {
    s.Ring.clear();
    return s;
  }
  s.Fill = s.AreaM2 / (4.0 * s.HalfUm * s.HalfVm);
  s.Seed = order.Seed;
  s.Ident = static_cast<int>(Mix(order.Seed ^ 0x5bd1e995u) % static_cast<uint32_t>(kIdentCount));
  s.FootM = order.FootM;

  const double top = std::max(order.TopOverFootM, 2.6);
  const double aspect = s.HalfUm / s.HalfVm;
  s.Use = order.Use ? *order.Use : UseOf(s.AreaM2, aspect, top);
  s.PeriodM = std::max(6.0, 2.0 * s.HalfUm / std::max(2.0, std::round(s.HalfUm / 6.0)));
  s.Storeys = std::max(1, static_cast<int>(std::lround(top / FloorPreferenceM(s.Use))));
  s.Roof = RoofOf(s, aspect);
  SplitHeight(&s, top, PitchDegOf(s.Use, s.Seed, order.HeightMeasured));

  const double bay = BayPreferenceM(s.Use);
  s.BayM = bay * (0.92 + 0.16 * UnitOf(s.Seed, 7));

  const bool verged = s.Roof != RoofKind::Flat && s.Roof != RoofKind::Dome;
  s.OverhangM = verged ? (s.Use == BuildingUse::Hall ? 0.25 : 0.42) : 0.0;
  return s;
}

[[nodiscard]] bool IsReflex(const std::vector<En> &ring, size_t i) {
  const size_t n = ring.size();
  const En &a = ring[(i + n - 1) % n], &b = ring[i], &c = ring[(i + 1) % n];
  return (b.E - a.E) * (c.N - b.N) - (c.E - b.E) * (b.N - a.N) < 0.0;
}

En UnitFrom(const En &a, const En &b) {
  const double e = b.E - a.E, n = b.N - a.N, l = std::hypot(e, n);
  return l < 1.0e-6 ? En{.E = 1.0, .N = 0.0} : En{.E = e / l, .N = n / l};
}

[[nodiscard]] bool WingCut(const Piece &whole, Piece *main, Piece *wing) {
  const std::vector<En> &ring = whole.P;
  const double wholeM2 = std::fabs(SignedArea(ring));
  double bestLen = 1.0e30;
  bool found = false;
  Piece a, b;
  for (size_t i = 0; i < ring.size(); i++) {
    if (!IsReflex(ring, i)) { continue; }
    const En dirs[2] = {UnitFrom(ring[(i + ring.size() - 1) % ring.size()], ring[i]),
                        UnitFrom(ring[i], ring[(i + 1) % ring.size()])};
    for (const En &dir : dirs) {
      Piece lo, hi;
      double len = 0.0;
      if (!CutPiece(whole, ring[i], {.E = dir.N, .N = -dir.E}, &lo, &hi, &len)) { continue; }
      if (len < 1.0 || len >= bestLen) { continue; }
      if (!BothWorthIt(lo, hi, wholeM2)) { continue; }
      a = lo;
      b = hi;
      bestLen = len;
      found = true;
    }
  }
  if (!found) { return false; }
  const bool aIsMain = std::fabs(SignedArea(a.P)) >= std::fabs(SignedArea(b.P));
  *main = aIsMain ? a : b;
  *wing = aIsMain ? b : a;
  return true;
}

int RowCut(const Piece &whole, const BuildingShape &box, Piece *out, int room) {
  const double lengthM = 2.0 * box.HalfUm;

  if (lengthM < 2.2 * kPlotM || box.HalfUm < 2.4 * box.HalfVm || box.Fill < 0.80) { return 0; }
  const int want = std::min(room, static_cast<int>(std::lround(lengthM / kPlotM)));
  if (want < 2) { return 0; }
  const double step = lengthM / static_cast<double>(want);
  const double wholeM2 = std::fabs(SignedArea(whole.P));
  Piece rest = whole;
  int made = 0;
  for (int k = 1; k < want; k++) {
    const En at = box.FromBox(-box.HalfUm + step * static_cast<double>(k), 0.0);
    Piece plot, beyond;
    double len = 0.0;
    if (!CutPiece(rest, at, box.AxisU, &plot, &beyond, &len)) { break; }
    if (std::fabs(SignedArea(plot.P)) < std::max(kLeastPieceM2, 0.4 * kPlotM * kPlotM)) { break; }
    if (std::fabs(SignedArea(beyond.P)) <
        std::max(kLeastPieceM2, kLeastPieceFrac * wholeM2 * 0.4)) {
      break;
    }
    out[made++] = plot;
    rest = beyond;
  }
  if (made == 0) { return 0; }
  out[made++] = rest;
  return made;
}

double DistanceToKerb(const Frontage &street, const En &p) {
  return (p.E - street.KerbEm) * street.ToStreetE + (p.N - street.KerbNm) * street.ToStreetN;
}

void FaceTheStreet(BuildingShape *s, const Frontage &street) {
  if (!street.Known || !s->OnGround()) { return; }
  const size_t n = s->Ring.size();
  double best = 0.35;
  for (size_t i = 0; i < n; i++) {
    if (s->Party[i]) { continue; }
    const En &p = s->Ring[i], &q = s->Ring[(i + 1) % n];
    const double e = q.E - p.E, nn = q.N - p.N, len = std::hypot(e, nn);
    if (len < 2.2) { continue; }
    const double outE = nn / len, outN = -e / len;
    const double look = outE * street.ToStreetE + outN * street.ToStreetN;
    if (look <= best) { continue; }
    const double standBack =
        DistanceToKerb(street, {.E = 0.5 * (p.E + q.E), .N = 0.5 * (p.N + q.N)});
    if (standBack > -0.4 || standBack < -12.0) { continue; }
    best = look;
    s->FrontEdge = static_cast<int>(i);
  }
}

} // namespace

void BuildingShape::ToBox(const En &p, double *u, double *v) const {
  const double e = p.E - Centre.E, n = p.N - Centre.N;
  *u = e * AxisU.E + n * AxisU.N;
  *v = -e * AxisU.N + n * AxisU.E;
}

En BuildingShape::FromBox(double u, double v) const {
  return {.E = Centre.E + u * AxisU.E - v * AxisU.N, .N = Centre.N + u * AxisU.N + v * AxisU.E};
}

Massing
MassOf(Span<const double> ringLatLon, double heightM, bool heightMeasured, const Frontage &street) {
  Massing out;
  out.Outline = RingInMetres(ringLatLon);
  if (out.Outline.size() < 3) {
    out.Outline.clear();
    return out;
  }
  if (SignedArea(out.Outline) < 0.0) { std::reverse(out.Outline.begin(), out.Outline.end()); }

  const uint32_t seed = SeedOfPlace(ringLatLon[0], ringLatLon[1]);
  const double topM = std::max(heightM, 2.6);
  PartOrder whole;
  whole.TopOverFootM = topM;
  whole.Seed = seed;
  whole.HeightMeasured = heightMeasured;
  const BuildingShape one = Finish(WholeOf(out.Outline), whole);
  if (!one.Valid()) {
    out.Outline.clear();
    return out;
  }

  Piece row[kMaxParts];
  const int plots = RowCut(WholeOf(out.Outline), one, row, kMaxParts);
  Piece main, wing;
  const bool winged = plots == 0 && one.Fill < 0.94 && WingCut(WholeOf(out.Outline), &main, &wing);

  if (plots > 1) {
    for (int k = 0; k < plots; k++) {
      PartOrder o = whole;
      o.Seed = Mix(seed + 0x9e3779b9u * static_cast<uint32_t>(k + 1));

      o.TopOverFootM = topM * (0.84 + 0.32 * UnitOf(o.Seed, 11));
      o.Use = one.Use == BuildingUse::Terrace || one.Use == BuildingUse::House
                  ? std::optional<BuildingUse>(BuildingUse::Terrace)
                  : std::optional<BuildingUse>();
      BuildingShape part = Finish(row[k], o);
      if (part.Valid()) { out.Parts.push_back(std::move(part)); }
    }
  } else if (winged) {
    PartOrder m = whole;
    m.Seed = Mix(seed + 0x27220a95u);
    BuildingShape mainPart = Finish(main, m);
    PartOrder w = whole;
    w.Seed = Mix(seed + 0x165667b1u);

    w.TopOverFootM = std::max(topM * (0.56 + 0.16 * UnitOf(w.Seed, 5)), 3.0);
    BuildingShape wingPart = Finish(wing, w);
    if (mainPart.Valid()) { out.Parts.push_back(std::move(mainPart)); }
    if (wingPart.Valid()) { out.Parts.push_back(std::move(wingPart)); }
  }

  if (out.Parts.empty()) { out.Parts.push_back(one); }

  std::vector<BuildingShape> stacked;
  for (BuildingShape &s : out.Parts) {
    const bool deep =
        std::min(s.HalfUm, s.HalfVm) >= 8.0 && s.Storeys >= 5 && s.Roof == RoofKind::Flat;
    std::vector<En> inner = deep ? RoofSurface::Widened(s.Ring, -kSetbackM) : std::vector<En>();
    if (inner.size() < 3) {
      stacked.push_back(std::move(s));
      continue;
    }
    const double lower = s.EavesM - s.FloorM;
    Piece cap;
    cap.P = std::move(inner);
    cap.Party.assign(cap.P.size(), 0u);
    PartOrder o;
    o.FootM = s.FootM + lower;
    o.TopOverFootM = s.EavesM + s.RiseM - lower;
    o.Seed = Mix(s.Seed + 0x3243f6a9u);
    o.HeightMeasured = heightMeasured;
    o.Use = s.Use;
    BuildingShape top = Finish(std::move(cap), o);
    BuildingShape base = s;
    SplitHeight(&base, lower, 0.0);
    stacked.push_back(std::move(base));
    if (top.Valid()) { stacked.push_back(std::move(top)); }
  }
  out.Parts.swap(stacked);

  for (BuildingShape &s : out.Parts) { FaceTheStreet(&s, street); }
  return out;
}

} // namespace outshine::Generators
