#include "BuildingShape.h"

#include <algorithm>
#include <cmath>

#include "Geodesy.h"

namespace outshine::Generators {

namespace {

/* Two corners closer than this are the same corner: the source quantises to a tile grid, and a
 * zero-length edge would give the wall an undefined normal and the box an undefined axis. */
constexpr double kSameCornerM = 0.20;

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

/* Rotating calipers over the edges: a minimum-area rectangle has a side flush with an edge of the
 * convex hull, and at these vertex counts testing every edge of the ring itself costs less than
 * building the hull first and never returns a larger box than the hull test would. */
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

BuildingUse UseOf(double areaM2, double aspect, double heightM) {
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
bool ReadsAsRound(const BuildingShape &s) {
  return s.Ring.size() >= 8 && s.Fill > 0.70 && s.Fill < 0.84 && s.HalfUm < 1.30 * s.HalfVm;
}

/* THE OUTLINE DECIDES, and where it cannot the box does. A pitched roof is trimmed to the outline,
 * so a low fill costs no overhang — what it costs is a ridge running across a plan that has two
 * wings, and that is what the threshold refuses. */
RoofKind RoofOf(const BuildingShape &s, double aspect) {
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
    case RoofKind::Flat:     rise = 0.0; break;
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
  s->RiseM = std::min({rise, roofShare * topM, 11.0});
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

}  // namespace

void BuildingShape::ToBox(const Plan2 &p, double *u, double *v) const {
  const double e = p.E - Centre.E, n = p.N - Centre.N;
  *u = e * AxisU.E + n * AxisU.N;
  *v = -e * AxisU.N + n * AxisU.E;
}

Plan2 BuildingShape::FromBox(double u, double v) const {
  return {Centre.E + u * AxisU.E - v * AxisU.N, Centre.N + u * AxisU.N + v * AxisU.E};
}

BuildingShape Analyse(Span<const double> ringLatLon, double heightM, bool heightMeasured) {
  BuildingShape s;
  s.Ring = RingInMetres(ringLatLon);
  if (s.Ring.size() < 3) return s;

  const double signed2 = SignedArea(s.Ring);
  if (signed2 < 0.0) std::reverse(s.Ring.begin(), s.Ring.end());
  s.AreaM2 = std::fabs(signed2);
  MinAreaBox(s.Ring, &s);
  if (s.HalfUm < 0.5 || s.HalfVm < 0.5) { s.Ring.clear(); return s; }
  s.Fill = s.AreaM2 / (4.0 * s.HalfUm * s.HalfVm);
  s.Seed = SeedOfPlace(ringLatLon[0], ringLatLon[1]);

  const double aspect = s.HalfUm / s.HalfVm;
  s.Use = UseOf(s.AreaM2, aspect, heightM);
  s.PeriodM = std::max(6.0, 2.0 * s.HalfUm / std::max(2.0, std::round(s.HalfUm / 6.0)));
  s.Storeys = std::max(1, (int)std::lround(heightM / FloorPreferenceM(s.Use)));
  s.Roof = RoofOf(s, aspect);
  SplitHeight(&s, std::max(heightM, 2.6), PitchDegOf(s.Use, s.Seed, heightMeasured));

  const double bay = BayPreferenceM(s.Use);
  s.BayM = bay * (0.92 + 0.16 * UnitOf(s.Seed, 7));
  /* An eaves overhang only exists where a roof falls; a parapet has none, and 0.4 m of nothing over
   * a flat roof would read as a lip that is not there. */
  const bool verged = s.Roof != RoofKind::Flat && s.Roof != RoofKind::Dome;
  s.OverhangM = verged ? (s.Use == BuildingUse::Hall ? 0.25 : 0.42) : 0.0;
  return s;
}

}  // namespace outshine::Generators
