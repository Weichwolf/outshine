#include "BuildingShape.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "FacadeUv.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

namespace {

/* Two corners closer than this are the same corner: the source quantises to a tile grid, and a
 * zero-length edge would give the wall an undefined normal and the box an undefined axis. */
constexpr double kSameCornerM = 0.20;
/* A cut runs through vertices, so a vertex within this of the line counts as ON it rather than as a
 * crossing — otherwise the reflex corner a cut is taken from crosses its own line twice. */
constexpr double kOnCutM = 0.02;

/* [SET] German small-town floor-to-floor by class, metres. Residential lands on 2.75-3.00 measured
 * over storey heights in the Fachwerk quarters; an office block sits higher because of its services,
 * a hall has no storeys at all and its number is the clear height under the truss. */
constexpr double kFloorHouseM = 2.85;
constexpr double kFloorBlockM = 3.15;
constexpr double kFloorHallM = 5.50;
constexpr double kFloorTowerM = 3.40;

/* [SET] roof pitches, degrees. 38-45 is the German pantile range (below 22 a pantile does not seal),
 * a mansard's lower slope is near vertical at 70, a hall's shallow sheet roof is 6. */
constexpr double kPitchHouseDeg = 42.0;
constexpr double kPitchOutbuildingDeg = 22.0;
constexpr double kPitchHallDeg = 6.0;
constexpr double kPitchSpireDeg = 62.0;

/* [SET] the width of a burgher's plot in a German Altstadt, metres. Hameln's Osterstrasse is set out
 * on 6-11 m frontages because that is what a plot paid tax on; a single OSM outline spanning a whole
 * side of a street is a row of them that nobody drew separately. */
constexpr double kPlotM = 8.5;
constexpr int kMaxParts = 9;
/* A piece smaller than this is a bay window, not a building, and cutting one off costs a party wall
 * for nothing. */
constexpr double kLeastPieceM2 = 16.0;
constexpr double kLeastPieceFrac = 0.16;
/* [SET] the upper storey stands back this far where the plan is deep enough to carry a terrace. */
constexpr double kSetbackM = 2.4;
/* [SET] a flat roof's parapet: 0.90 m is the German fall-protection minimum for a roof that is
 * walked on. It is the structure's TOP and is therefore taken OUT of the resolved height rather than
 * added on top of it — the drawn silhouette used to stand 0.90 m above the number the point query
 * answers for the same building. A plan too narrow to walk on gets none. */
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

/* A FACT ABOUT THE PLACE, not about the call: the seed is the outline's own first corner rounded to
 * a decimetre, so the same house is the same house in both clients and in every pass. */
uint32_t SeedOfPlace(double latDeg, double lonDeg) {
  const int32_t la = (int32_t)std::llround(latDeg * 1.0e6);
  const int32_t lo = (int32_t)std::llround(lonDeg * 1.0e6);
  return Mix((uint32_t)la * 0x9e3779b9u ^ Mix((uint32_t)lo));
}

double UnitOf(uint32_t seed, int stream) {
  return (double)(Mix(seed + (uint32_t)stream * 0x85ebca6bu) >> 8) * (1.0 / 16777216.0);
}

std::vector<Plan2> RingInMetres(Span<const double> latLon) {
  std::vector<Plan2> ring;
  if (latLon.Size() < 6) return ring;
  const double refLat = latLon[0], refLon = latLon[1];
  ring.reserve(latLon.Size() / 2);
  for (size_t k = 0; k + 1 < latLon.Size(); k += 2) {
    Plan2 p;
    EnuOffsetM(refLat, refLon, latLon[k], latLon[k + 1], p.E, p.N);
    if (!ring.empty() && std::hypot(p.E - ring.back().E, p.N - ring.back().N) < kSameCornerM)
      continue;
    ring.push_back(p);
  }
  while (ring.size() >= 2 &&
         std::hypot(ring.front().E - ring.back().E, ring.front().N - ring.back().N) < kSameCornerM)
    ring.pop_back();
  return ring;
}

double SignedArea(const std::vector<Plan2> &ring) {
  double a = 0.0;
  for (size_t i = 0, n = ring.size(); i < n; i++) {
    const Plan2 &p = ring[i], &q = ring[(i + 1) % n];
    a += p.E * q.N - q.E * p.N;
  }
  return 0.5 * a;
}

/* One piece of a footprint on its way to becoming a mass: the ring, and which of its edges it does
 * not own alone. */
struct Piece {
  std::vector<Plan2> P;
  std::vector<uint8_t> Party;
};

Piece WholeOf(const std::vector<Plan2> &ring) {
  Piece p;
  p.P = ring;
  p.Party.assign(ring.size(), 0u);
  return p;
}

double SideOf(const Plan2 &at, const Plan2 &normal, const Plan2 &p) {
  return (p.E - at.E) * normal.E + (p.N - at.N) * normal.N;
}

/* A CUT ALONG A BOUNDARY EDGE LEAVES A SPUR: the edge lies ON the line, so both halves keep it and
 * the half it does not belong to closes over a run of zero width. The area is right and everything
 * measured off the RING is not — an L cut at its own notch came back with fill 0.48 and took a flat
 * roof for it. A vertex whose two edges double back on each other is that spur and nothing else. */
void DropSpurs(Piece *p) {
  bool again = true;
  while (again && p->P.size() > 3) {
    again = false;
    for (size_t i = 0; i < p->P.size(); i++) {
      const size_t n = p->P.size();
      const Plan2 &a = p->P[(i + n - 1) % n], &b = p->P[i], &c = p->P[(i + 1) % n];
      const double e1 = b.E - a.E, n1 = b.N - a.N, e2 = c.E - b.E, n2 = c.N - b.N;
      const double area2 = std::fabs(e1 * n2 - e2 * n1);
      if (area2 > kOnCutM * std::max(1.0, std::hypot(e1, n1) + std::hypot(e2, n2))) continue;
      if (e1 * e2 + n1 * n2 >= 0.0) continue;   /* straight through is a corner, not a spur */
      p->P.erase(p->P.begin() + (long)i);
      p->Party.erase(p->Party.begin() + (long)i);
      again = true;
      break;
    }
  }
}

/* CUT A RING BY A LINE, but only where the line crosses it EXACTLY TWICE. A third crossing means the
 * line leaves the plan and comes back — a U-shape cut across its arms — and each half would then be
 * several pieces joined by a channel of no width, which raises walls that enclose nothing. Two
 * crossings is decidable from the sign sequence alone, which is why it is the condition and not a
 * tolerance on the result. */
[[nodiscard]] bool CutPiece(const Piece &in, const Plan2 &at, const Plan2 &normal, Piece *back, Piece *front,
              double *cutLenM) {
  const size_t n = in.P.size();
  if (n < 3) return false;
  std::vector<double> s(n);
  std::vector<int> sg(n);
  for (size_t i = 0; i < n; i++) {
    s[i] = SideOf(at, normal, in.P[i]);
    sg[i] = s[i] > kOnCutM ? 1 : (s[i] < -kOnCutM ? -1 : 0);
  }
  int last = 0;
  for (size_t i = 0; i < n; i++)
    if (sg[i] != 0) last = sg[i];
  if (last == 0) return false;
  int crossings = 0, cur = last;
  for (size_t i = 0; i < n; i++) {
    if (sg[i] == 0) continue;
    if (sg[i] != cur) crossings++;
    cur = sg[i];
  }
  if (crossings != 2) return false;

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
      if (si * sj >= 0) continue;
      const double f = s[i] / (s[i] - s[j]);
      out->P.push_back({in.P[i].E + (in.P[j].E - in.P[i].E) * f,
                        in.P[i].N + (in.P[j].N - in.P[i].N) * f});
      out->Party.push_back(si > 0 ? 1u : in.Party[i]);
    }
  };
  build(front, 1);
  build(back, -1);
  DropSpurs(front);
  DropSpurs(back);
  if (front->P.size() < 3 || back->P.size() < 3) return false;

  double t0 = 1.0e30, t1 = -1.0e30;
  for (const Plan2 &p : front->P) {
    if (std::fabs(SideOf(at, normal, p)) > kOnCutM) continue;
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

/* Rotating calipers over the edges. A minimum-area rectangle is flush with an edge of the CONVEX
 * HULL (Freeman & Shapira 1975), and a hull edge of a concave ring need not be a ring edge — so
 * searching the ring's own edges can return a larger box, and over random concave polygons it often
 * does. It does not on THIS data: a building plan is rectilinear, and on L, T, U and notched bars at
 * arbitrary bearings every hull edge is a ring edge. The hull is skipped because it buys nothing
 * here, not because the search is equivalent in general. */
void MinAreaBox(const std::vector<Plan2> &ring, BuildingShape *out) {
  double best = 1.0e30;
  for (size_t i = 0, n = ring.size(); i < n; i++) {
    const Plan2 &p = ring[i], &q = ring[(i + 1) % n];
    double ax = q.E - p.E, ay = q.N - p.N;
    const double len = std::hypot(ax, ay);
    if (len < kSameCornerM) continue;
    ax /= len;
    ay /= len;
    double u0 = 1e30, u1 = -1e30, v0 = 1e30, v1 = -1e30;
    for (const Plan2 &r : ring) {
      const double u = r.E * ax + r.N * ay, v = -r.E * ay + r.N * ax;
      u0 = std::min(u0, u); u1 = std::max(u1, u);
      v0 = std::min(v0, v); v1 = std::max(v1, v);
    }
    const double area = (u1 - u0) * (v1 - v0);
    if (area >= best) continue;
    best = area;
    const double cu = 0.5 * (u0 + u1), cv = 0.5 * (v0 + v1);
    out->Centre = {cu * ax - cv * ay, cu * ay + cv * ax};
    out->AxisU = {ax, ay};
    out->HalfUm = 0.5 * (u1 - u0);
    out->HalfVm = 0.5 * (v1 - v0);
  }
  if (out->HalfUm < out->HalfVm) {
    std::swap(out->HalfUm, out->HalfVm);
    out->AxisU = {-out->AxisU.N, out->AxisU.E};
  }
}

[[nodiscard]] BuildingUse UseOf(double areaM2, double aspect, double heightM) {
  if (areaM2 < 26.0) return BuildingUse::Outbuilding;
  if (heightM > 21.0 && areaM2 < 260.0) return BuildingUse::Spire;
  if (heightM > 19.0) return BuildingUse::Tower;
  if (areaM2 > 1300.0) return BuildingUse::Hall;
  if (areaM2 > 380.0) return BuildingUse::Block;
  if (aspect > 2.2 && areaM2 > 90.0) return BuildingUse::Terrace;
  return BuildingUse::House;
}

/* A ring of eight or more corners whose plan fills about pi/4 of its box and whose box is nearly
 * square is the source's way of writing a circle — there is no tag for one. */
[[nodiscard]] bool ReadsAsRound(const BuildingShape &s) {
  return s.Ring.size() >= 8 && s.Fill > 0.70 && s.Fill < 0.84 && s.HalfUm < 1.30 * s.HalfVm;
}

/* THE OUTLINE DECIDES, and where it cannot the box does. A pitched roof is trimmed to the outline,
 * so a low fill costs no overhang — what it costs is a ridge running across a plan that has two
 * wings, which is why a plan that low was cut into wings before it got here. */
[[nodiscard]] RoofKind RoofOf(const BuildingShape &s, double aspect) {
  if (ReadsAsRound(s)) return RoofKind::Dome;
  const bool pitchable = s.Fill >= 0.74;
  switch (s.Use) {
    case BuildingUse::Outbuilding: return pitchable ? RoofKind::Shed : RoofKind::Flat;
    case BuildingUse::Spire:       return pitchable ? RoofKind::Hip : RoofKind::Flat;
    case BuildingUse::Tower:       return RoofKind::Flat;
    case BuildingUse::Hall:
      if (!pitchable) return RoofKind::Flat;
      return (aspect > 1.8 && s.AreaM2 > 2600.0) ? RoofKind::Sawtooth : RoofKind::Flat;
    case BuildingUse::Block:
      if (!pitchable) return RoofKind::Flat;
      if (s.Storeys >= 4) return RoofKind::Mansard;
      return aspect > 1.9 ? RoofKind::Gable : RoofKind::Hip;
    case BuildingUse::Terrace:     return pitchable ? RoofKind::Gable : RoofKind::Flat;
    case BuildingUse::House:       break;
  }
  if (!pitchable) return RoofKind::Flat;
  return aspect >= 1.30 ? RoofKind::Gable : RoofKind::Hip;
}

/* The jitter exists only where the height was invented. A measured top plus a wandering pitch is a
 * measurement dressed up as a guess; an invented top already is one, and a terrace of identical
 * pitches is the more visible lie. */
double PitchDegOf(BuildingUse use, uint32_t seed, bool heightMeasured) {
  const double jitter = heightMeasured ? 0.0 : (UnitOf(seed, 3) - 0.5) * 9.0;
  switch (use) {
    case BuildingUse::Outbuilding: return kPitchOutbuildingDeg + jitter;
    case BuildingUse::Hall:        return kPitchHallDeg;
    case BuildingUse::Spire:       return kPitchSpireDeg;
    case BuildingUse::Tower:
    case BuildingUse::Block:
    case BuildingUse::Terrace:
    case BuildingUse::House:       break;
  }
  return kPitchHouseDeg + jitter;
}

double FloorPreferenceM(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return 2.60;
    case BuildingUse::Hall:        return kFloorHallM;
    case BuildingUse::Tower:       return kFloorTowerM;
    case BuildingUse::Spire:       return 4.20;
    case BuildingUse::Block:       return kFloorBlockM;
    case BuildingUse::Terrace:
    case BuildingUse::House:       break;
  }
  return kFloorHouseM;
}

/* [SET] the bay a window rhythm is set out on, metres. A Fachwerk house repeats its posts every
 * 2.9-3.4 m, a post-war block sets out on 3.6, a hall's bays follow its frame at 5. */
double BayPreferenceM(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return 2.60;
    case BuildingUse::Hall:        return 5.00;
    case BuildingUse::Tower:       return 3.40;
    case BuildingUse::Spire:       return 4.00;
    case BuildingUse::Block:       return 3.60;
    case BuildingUse::Terrace:
    case BuildingUse::House:       break;
  }
  return 3.10;
}

/* The roof takes what it needs off the top and the wall keeps the rest, because the height the
 * streamer resolved is to the TOP of the structure and the point query answers with that same
 * number. Nothing here may move it. */
void SplitHeight(BuildingShape *s, double topM, double pitchDeg) {
  const double halfSpan = s->Roof == RoofKind::Hip || s->Roof == RoofKind::Gable ||
                                  s->Roof == RoofKind::Mansard
                              ? s->HalfVm
                              : s->HalfUm;
  double rise = 0.0;
  switch (s->Roof) {
    case RoofKind::Flat:     rise = s->HalfVm > kParapetLeastHalfVm ? kParapetM : 0.0; break;
    case RoofKind::Shed:     rise = 2.0 * s->HalfVm * std::tan(pitchDeg * kDeg2Rad); break;
    case RoofKind::Sawtooth: rise = 0.30 * s->PeriodM; break;
    case RoofKind::Dome:     rise = 0.85 * s->HalfVm; break;
    case RoofKind::Mansard:  rise = 0.62 * halfSpan * std::tan(pitchDeg * kDeg2Rad); break;
    case RoofKind::Gable:
    case RoofKind::Hip:      rise = halfSpan * std::tan(pitchDeg * kDeg2Rad); break;
  }
  /* [SET] A ROOF IS NEVER MORE THAN 45 % OF THE HOUSE. A pitch applied across the full short span of
   * a deep plan puts a 5 m roof on a 3 m wall, which is a barn and not a town house; the wall keeps
   * the majority and the pitch gives way. A spire is the exception it is named for. */
  const double roofShare = s->Use == BuildingUse::Spire ? 0.72 : 0.45;
  s->RiseM = s->Roof == RoofKind::Flat ? std::min(rise, 0.30 * topM)
                                       : std::min({rise, roofShare * topM, 11.0});
  s->EavesM = std::max(topM - s->RiseM, 2.40);
  s->RiseM = std::max(topM - s->EavesM, 0.0);

  const double want = FloorPreferenceM(s->Use);
  s->Storeys = std::max(1, (int)std::lround(s->EavesM / want));
  s->FloorM = s->EavesM / (double)s->Storeys;
  if (s->FloorM > 6.5) {   /* a clear height, not a storey: a hall gets one floor and keeps it */
    s->Storeys = std::max(1, (int)std::floor(s->EavesM / 6.5));
    s->FloorM = s->EavesM / (double)s->Storeys;
  }
  s->BreakFracV = 0.42;
  s->BreakRiseM = s->Roof == RoofKind::Mansard ? 0.78 * s->RiseM : 0.0;
}

/* Everything one piece has to be told that its own ring cannot say. */
struct PartOrder {
  double FootM = 0.0;
  double TopOverFootM = 0.0;
  uint32_t Seed = 0;
  bool HeightMeasured = false;
  std::optional<BuildingUse> Use;   /* empty: the piece's own plan decides */
};

BuildingShape Finish(Piece piece, const PartOrder &order) {
  BuildingShape s;
  if (piece.P.size() < 3) return s;
  s.Ring = std::move(piece.P);
  s.Party = std::move(piece.Party);
  const double signed2 = SignedArea(s.Ring);
  if (signed2 < 0.0) {
    std::reverse(s.Ring.begin(), s.Ring.end());
    /* Edge i of a reversed ring is edge n-1-i of the original: the flag belongs to the segment, not
     * to the corner it starts at. */
    std::reverse(s.Party.begin(), s.Party.end());
    std::rotate(s.Party.begin(), s.Party.begin() + 1, s.Party.end());
  }
  s.AreaM2 = std::fabs(signed2);
  MinAreaBox(s.Ring, &s);
  if (s.HalfUm < 0.5 || s.HalfVm < 0.5) { s.Ring.clear(); return s; }
  s.Fill = s.AreaM2 / (4.0 * s.HalfUm * s.HalfVm);
  s.Seed = order.Seed;
  s.Ident = (int)(Mix(order.Seed ^ 0x5bd1e995u) % (uint32_t)kIdentCount);
  s.FootM = order.FootM;

  const double top = std::max(order.TopOverFootM, 2.6);
  const double aspect = s.HalfUm / s.HalfVm;
  s.Use = order.Use ? *order.Use : UseOf(s.AreaM2, aspect, top);
  s.PeriodM = std::max(6.0, 2.0 * s.HalfUm / std::max(2.0, std::round(s.HalfUm / 6.0)));
  s.Storeys = std::max(1, (int)std::lround(top / FloorPreferenceM(s.Use)));
  s.Roof = RoofOf(s, aspect);
  SplitHeight(&s, top, PitchDegOf(s.Use, s.Seed, order.HeightMeasured));

  const double bay = BayPreferenceM(s.Use);
  s.BayM = bay * (0.92 + 0.16 * UnitOf(s.Seed, 7));
  /* An eaves overhang only exists where a roof falls; a parapet has none, and 0.4 m of nothing over
   * a flat roof would read as a lip that is not there. */
  const bool verged = s.Roof != RoofKind::Flat && s.Roof != RoofKind::Dome;
  s.OverhangM = verged ? (s.Use == BuildingUse::Hall ? 0.25 : 0.42) : 0.0;
  return s;
}

[[nodiscard]] bool IsReflex(const std::vector<Plan2> &ring, size_t i) {
  const size_t n = ring.size();
  const Plan2 &a = ring[(i + n - 1) % n], &b = ring[i], &c = ring[(i + 1) % n];
  return (b.E - a.E) * (c.N - b.N) - (c.E - b.E) * (b.N - a.N) < 0.0;
}

Plan2 UnitFrom(const Plan2 &a, const Plan2 &b) {
  const double e = b.E - a.E, n = b.N - a.N, l = std::hypot(e, n);
  return l < 1.0e-6 ? Plan2{1.0, 0.0} : Plan2{e / l, n / l};
}

/* THE STEP AT A RE-ENTRANT CORNER. An L is two rectangles and the line that separates them runs
 * along one of the two edges that meet at the notch; taking the SHORTEST such cut is what picks the
 * short arm of the L instead of slicing the long one in half. */
[[nodiscard]] bool WingCut(const Piece &whole, Piece *main, Piece *wing) {
  const std::vector<Plan2> &ring = whole.P;
  const double wholeM2 = std::fabs(SignedArea(ring));
  double bestLen = 1.0e30;
  bool found = false;
  Piece a, b;
  for (size_t i = 0; i < ring.size(); i++) {
    if (!IsReflex(ring, i)) continue;
    const Plan2 dirs[2] = {UnitFrom(ring[(i + ring.size() - 1) % ring.size()], ring[i]),
                           UnitFrom(ring[i], ring[(i + 1) % ring.size()])};
    for (const Plan2 &dir : dirs) {
      Piece lo, hi;
      double len = 0.0;
      if (!CutPiece(whole, ring[i], {dir.N, -dir.E}, &lo, &hi, &len)) continue;
      if (len < 1.0 || len >= bestLen) continue;
      if (!BothWorthIt(lo, hi, wholeM2)) continue;
      a = lo;
      b = hi;
      bestLen = len;
      found = true;
    }
  }
  if (!found) return false;
  const bool aIsMain = std::fabs(SignedArea(a.P)) >= std::fabs(SignedArea(b.P));
  *main = aIsMain ? a : b;
  *wing = aIsMain ? b : a;
  return true;
}

/* A ROW: a bar long enough to be several plots is cut across its long axis at plot width, and each
 * plot keeps the walls it shares. */
int RowCut(const Piece &whole, const BuildingShape &box, Piece *out, int room) {
  const double lengthM = 2.0 * box.HalfUm;
  /* WHAT MAKES A BAR A ROW and not a building: it has to be long, straight-sided and several plots
   * wide. Without all three, cutting turns a 26 m block into two 13 m ones that share a wall for no
   * reason the outline gives. */
  if (lengthM < 2.2 * kPlotM || box.HalfUm < 2.4 * box.HalfVm || box.Fill < 0.80) return 0;
  const int want = std::min(room, (int)std::lround(lengthM / kPlotM));
  if (want < 2) return 0;
  const double step = lengthM / (double)want;
  const double wholeM2 = std::fabs(SignedArea(whole.P));
  Piece rest = whole;
  int made = 0;
  for (int k = 1; k < want; k++) {
    const Plan2 at = box.FromBox(-box.HalfUm + step * (double)k, 0.0);
    Piece plot, beyond;
    double len = 0.0;
    if (!CutPiece(rest, at, box.AxisU, &plot, &beyond, &len)) break;
    if (std::fabs(SignedArea(plot.P)) < std::max(kLeastPieceM2, 0.4 * kPlotM * kPlotM)) break;
    if (std::fabs(SignedArea(beyond.P)) < std::max(kLeastPieceM2, kLeastPieceFrac * wholeM2 * 0.4))
      break;
    out[made++] = plot;
    rest = beyond;
  }
  if (made == 0) return 0;
  out[made++] = rest;
  return made;
}

double DistanceToKerb(const Frontage &street, const Plan2 &p) {
  return (p.E - street.KerbEm) * street.ToStreetE + (p.N - street.KerbNm) * street.ToStreetN;
}

/* WHICH WALL LOOKS AT THE STREET. The face has to point at the carriageway and stand back from the
 * kerb by a pavement's worth and no more — a wall 40 m behind the one on the frontage is a back
 * wall no matter which way it points. */
void FaceTheStreet(BuildingShape *s, const Frontage &street) {
  if (!street.Known || !s->OnGround()) return;
  const size_t n = s->Ring.size();
  double best = 0.35;
  for (size_t i = 0; i < n; i++) {
    if (s->Party[i]) continue;
    const Plan2 &p = s->Ring[i], &q = s->Ring[(i + 1) % n];
    const double e = q.E - p.E, nn = q.N - p.N, len = std::hypot(e, nn);
    if (len < 2.2) continue;
    const double outE = nn / len, outN = -e / len;
    const double look = outE * street.ToStreetE + outN * street.ToStreetN;
    if (look <= best) continue;
    const double standBack = DistanceToKerb(street, {0.5 * (p.E + q.E), 0.5 * (p.N + q.N)});
    if (standBack > -0.4 || standBack < -12.0) continue;
    best = look;
    s->FrontEdge = (int)i;
  }
}

}  // namespace

void BuildingShape::ToBox(const Plan2 &p, double *u, double *v) const {
  const double e = p.E - Centre.E, n = p.N - Centre.N;
  *u = e * AxisU.E + n * AxisU.N;
  *v = -e * AxisU.N + n * AxisU.E;
}

Plan2 BuildingShape::FromBox(double u, double v) const {
  return {Centre.E + u * AxisU.E - v * AxisU.N, Centre.N + u * AxisU.N + v * AxisU.E};
}

Massing MassOf(Span<const double> ringLatLon, double heightM, bool heightMeasured,
               const Frontage &street) {
  Massing out;
  out.Outline = RingInMetres(ringLatLon);
  if (out.Outline.size() < 3) { out.Outline.clear(); return out; }
  if (SignedArea(out.Outline) < 0.0) std::reverse(out.Outline.begin(), out.Outline.end());

  const uint32_t seed = SeedOfPlace(ringLatLon[0], ringLatLon[1]);
  const double topM = std::max(heightM, 2.6);
  PartOrder whole;
  whole.TopOverFootM = topM;
  whole.Seed = seed;
  whole.HeightMeasured = heightMeasured;
  const BuildingShape one = Finish(WholeOf(out.Outline), whole);
  if (!one.Valid()) { out.Outline.clear(); return out; }

  Piece row[kMaxParts];
  const int plots = RowCut(WholeOf(out.Outline), one, row, kMaxParts);
  Piece main, wing;
  const bool winged = plots == 0 && one.Fill < 0.94 && WingCut(WholeOf(out.Outline), &main, &wing);

  if (plots > 1) {
    for (int k = 0; k < plots; k++) {
      PartOrder o = whole;
      o.Seed = Mix(seed + 0x9e3779b9u * (uint32_t)(k + 1));
      /* A row is a row of DIFFERENT houses: neighbours that share a wall and a street line but not
       * an eaves line. The spread is one storey either way, which is what a German terrace does. */
      o.TopOverFootM = topM * (0.84 + 0.32 * UnitOf(o.Seed, 11));
      o.Use = one.Use == BuildingUse::Terrace || one.Use == BuildingUse::House
                  ? std::optional<BuildingUse>(BuildingUse::Terrace)
                  : std::optional<BuildingUse>();
      BuildingShape part = Finish(row[k], o);
      if (part.Valid()) out.Parts.push_back(std::move(part));
    }
  } else if (winged) {
    PartOrder m = whole;
    m.Seed = Mix(seed + 0x27220a95u);
    BuildingShape mainPart = Finish(main, m);
    PartOrder w = whole;
    w.Seed = Mix(seed + 0x165667b1u);
    /* The wing is the ancillary half — a rear extension, a workshop, a garage wing — and it is what
     * turns a stepped plan into two masses instead of one prism with a notch in it. */
    w.TopOverFootM = std::max(topM * (0.56 + 0.16 * UnitOf(w.Seed, 5)), 3.0);
    BuildingShape wingPart = Finish(wing, w);
    if (mainPart.Valid()) out.Parts.push_back(std::move(mainPart));
    if (wingPart.Valid()) out.Parts.push_back(std::move(wingPart));
  }

  if (out.Parts.empty()) out.Parts.push_back(one);

  /* THE SETBACK, and it is the same statement as the wing made vertically: where a plan is deep and
   * tall, the top storey stands back and the roof of the storey below it becomes a terrace. */
  std::vector<BuildingShape> stacked;
  for (BuildingShape &s : out.Parts) {
    const bool deep = std::min(s.HalfUm, s.HalfVm) >= 8.0 && s.Storeys >= 5 &&
                      s.Roof == RoofKind::Flat;
    std::vector<Plan2> inner = deep ? RoofSurface::Widened(s.Ring, -kSetbackM) : std::vector<Plan2>();
    if (inner.size() < 3) { stacked.push_back(std::move(s)); continue; }
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
    if (top.Valid()) stacked.push_back(std::move(top));
  }
  out.Parts.swap(stacked);

  for (BuildingShape &s : out.Parts) FaceTheStreet(&s, street);
  return out;
}

}  // namespace outshine::Generators
