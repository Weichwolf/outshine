#include "BuildingMesh.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <vector>

#include "BuildingShape.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

namespace {
std::atomic<size_t> gBuried{0};
std::atomic<size_t> gDeepestMm{0};
}

size_t BuildingMesh::BuriedTaken() { return gBuried.exchange(0u); }
size_t BuildingMesh::DeepestBuriedMmTaken() { return gDeepestMm.exchange(0u); }


namespace {

constexpr double kSinkM = 0.30;
constexpr double kThinnestM2 = 0.01;
constexpr double kLongestNeedleM = 5.0;

constexpr double kSlabM = 0.20;
constexpr double kParapetThickM = 0.32;
constexpr double kCorniceM = 0.16;
constexpr double kChimneyWideM = 0.55;
constexpr double kChimneyOverRidgeM = 0.85;

constexpr double kPlinthM = 0.50;
constexpr double kPlinthProudM = 0.09;

constexpr double kKerbUpM = 0.12;
constexpr double kKerbTopM = 0.16;
constexpr double kKerbSkirtM = 0.10;

constexpr double kPavementLeastM = 0.6;
constexpr double kPavementMostM = 24.0;
constexpr double kFootwayMostM = 5.0;

struct Vtx {
  En P;
  double Z = 0.0;
  float U = 0.0f, V = 0.0f;
};

[[nodiscard]] FacadeStyle StyleOf(BuildingUse use) {
  switch (use) {
    case BuildingUse::Outbuilding: return FacadeStyle::Outbuilding;
    case BuildingUse::Terrace:     return FacadeStyle::Terrace;
    case BuildingUse::Block:       return FacadeStyle::Block;
    case BuildingUse::Hall:        return FacadeStyle::Hall;
    case BuildingUse::Tower:       return FacadeStyle::Tower;
    case BuildingUse::Spire:       return FacadeStyle::Spire;
    case BuildingUse::House:       break;
  }
  return FacadeStyle::House;
}

double EavesZ(const BuildingShape &s) { return s.FootM + s.EavesM; }

Vtx Wall(const BuildingShape &s, const En &p, double z, double bays, Fields stand) {
  return {p, z, FacadeUvX(StyleOf(s.Use), stand, (float)bays),
          FacadeUvY(s.Ident, (float)((z - s.FootM) / s.FloorM))};
}

Vtx Face(const BuildingShape &s, const En &p, double z, Facade kind) {
  return {p, z, FaceUvX(kind, s.Ident), (float)z};
}

class Site {
public:
  Site(const StructurePlan &plan, std::vector<float> &soup) : Soup_(soup) {
    const double lat = plan.RingLatLon[0], lon = plan.RingLatLon[1];
    double origin[3];
    GeoToEcef(lat, lon, plan.BaseAslM, origin);
    EnuAxesEcef(lat, lon, East_, North_, Up_);
    for (int c = 0; c < 3; c++) Origin_[c] = origin[c] - plan.AnchorEcef[c];
    ReachM_ = std::sqrt(Origin_[0] * Origin_[0] + Origin_[1] * Origin_[1] + Origin_[2] * Origin_[2]);
  }

  [[nodiscard]] double ReachM() const { return ReachM_; }

  void Tri(const Vtx &a, const Vtx &b, const Vtx &c) {
    const double e1 = b.P.E - a.P.E, n1 = b.P.N - a.P.N, z1 = b.Z - a.Z;
    const double e2 = c.P.E - a.P.E, n2 = c.P.N - a.P.N, z2 = c.Z - a.Z;
    double nrm[3] = {n1 * z2 - z1 * n2, z1 * e2 - e1 * z2, e1 * n2 - n1 * e2};
    // A NEEDLE IS THIN AND LONG, AND BOTH HALVES MATTER. Refusing on AREA alone took 22 162
    // triangles out of Rothenburg to remove 5 slivers, because window mullions and cornices are
    // small and perfectly well-shaped. Refusing on the ASPECT RATIO alone left 760 standing,
    // because a triangle of 0.009 m2 with a 6 m edge is 2.5e-4 -- above any ratio loose enough to
    // spare a mullion. What the eye actually sees is a bright line: almost no area, carrying metres.
    // So both conditions, and they are the same two the instrument counts by, taken from the frame
    // rather than from a formula. Unreal drops degenerates in its mesh build and RAGE validates at
    // export; neither asks the renderer to carry them.
    const double len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < 1.0e-9) { return; }
    const double e3 = c.P.E - b.P.E, n3 = c.P.N - b.P.N, z3 = c.Z - b.Z;
    const double longest = std::max({e1 * e1 + n1 * n1 + z1 * z1, e2 * e2 + n2 * n2 + z2 * z2,
                                     e3 * e3 + n3 * n3 + z3 * z3});
    if (0.5 * len < kThinnestM2 && longest > kLongestNeedleM * kLongestNeedleM) { return; }
    for (int c2 = 0; c2 < 3; c2++) nrm[c2] /= len;
    Push(a, nrm);
    Push(b, nrm);
    Push(c, nrm);
  }

  void Quad(const Vtx &a, const Vtx &b, const Vtx &c, const Vtx &d) {
    Tri(a, b, c);
    Tri(a, c, d);
  }

private:
  void Push(const Vtx &v, const double nrm[3]) {
    for (int c = 0; c < 3; c++)
      Soup_.push_back((float)(Origin_[c] + v.P.E * East_[c] + v.P.N * North_[c] + v.Z * Up_[c]));
    Soup_.push_back(v.U);
    Soup_.push_back(v.V);
    for (int c = 0; c < 3; c++)
      Soup_.push_back((float)(nrm[0] * East_[c] + nrm[1] * North_[c] + nrm[2] * Up_[c]));
  }

  std::vector<float> &Soup_;
  double Origin_[3], East_[3], North_[3], Up_[3];
  double ReachM_ = 0.0;
};

class Site2Ground {
public:
  Site2Ground(Span<const double> ringLatLon, Span<const double> cornerAslM, double baseAslM) {
    const size_t n = std::min(cornerAslM.Size(), ringLatLon.Size() / 2);
    if (n < 3) return;
    double m[3][4] = {};
    for (size_t k = 0; k < n; k++) {
      double e = 0.0, nn = 0.0;
      EnuOffsetM(ringLatLon[0], ringLatLon[1], ringLatLon[k * 2], ringLatLon[k * 2 + 1], e, nn);
      const double z = cornerAslM[k] - baseAslM;
      const double b[3] = {1.0, e, nn};
      for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) m[r][c] += b[r] * b[c];
        m[r][3] += b[r] * z;
      }
    }
    for (int c = 0; c < 3; c++) {
      int piv = c;
      for (int r = c + 1; r < 3; r++)
        if (std::fabs(m[r][c]) > std::fabs(m[piv][c])) piv = r;
      if (std::fabs(m[piv][c]) < 1.0e-6) return;
      for (int k = 0; k < 4; k++) std::swap(m[c][k], m[piv][k]);
      for (int r = 0; r < 3; r++) {
        if (r == c) continue;
        const double f = m[r][c] / m[c][c];
        for (int k = c; k < 4; k++) m[r][k] -= f * m[c][k];
      }
    }
    Const_ = m[0][3] / m[0][0];
    SlopeE_ = m[1][3] / m[1][1];
    SlopeN_ = m[2][3] / m[2][2];
  }

  double At(const En &p) const { return Const_ + SlopeE_ * p.E + SlopeN_ * p.N; }

private:
  double Const_ = 0.0, SlopeE_ = 0.0, SlopeN_ = 0.0;
};

double EdgeLength(const En &p, const En &q) { return std::hypot(q.E - p.E, q.N - p.N); }

En Along(const En &p, const En &q, double t) {
  return {p.E + (q.E - p.E) * t, p.N + (q.N - p.N) * t};
}

double BaysOn(double lengthM, double bayM) {
  if (lengthM < 1.9) return 0.0;
  return std::max(1.0, std::round(lengthM / bayM));
}

void WallPanel(const BuildingShape &s, const En &p, const En &q, double bay0, double bay1,
               double lowZ, double highZ, Fields stand, Site &site) {
  site.Quad(Wall(s, p, lowZ, bay0, stand), Wall(s, q, lowZ, bay1, stand),
            Wall(s, q, highZ, bay1, stand), Wall(s, p, highZ, bay0, stand));
}

void FrontWall(const BuildingShape &s, const En &p, const En &q, double bays, double lowZ,
               double highZ, Site &site) {
  const double door = std::floor(0.5 * bays);
  const double t0 = door / bays, t1 = (door + 1.0) / bays;
  const En a = Along(p, q, t0), b = Along(p, q, t1);
  if (door > 0.0) WallPanel(s, p, a, 0.0, door, lowZ, highZ, Fields::Front, site);
  WallPanel(s, a, b, door, door + 1.0, lowZ, highZ, Fields::Entrance, site);
  if (door + 1.0 < bays)
    WallPanel(s, b, q, door + 1.0, bays, lowZ, highZ, Fields::Front, site);
}

// THE WALL HEAD IS BROKEN WHERE THE ROOF IS. One quad from corner to corner leaves an edge that the
// gable above it -- broken at the ridge -- has no matching edge for, so a wall that meets its own
// gable perfectly along a straight line still reads as a hole to a walk over the triangles.
// ONE SUBDIVISION FOR FOUR BUILDERS. Wall, gable, soffit and covering all end on the same seam, and
// a seam pairs only if both sides carry the SAME vertices. The roof breaks a footprint edge at one
// set of places and the overhung edge outside it at another -- the offset moves the crossing -- so
// every builder takes the UNION of the two, and the covering is handed a ring already carrying it.
void BreaksBoth(const RoofSurface &roof, const En &p, const En &q, const En &wp, const En &wq,
                bool overhung, std::vector<double> &at) {
  std::vector<double> other;
  roof.BreaksAlong(p, q, at);
  if (overhung) {
    roof.BreaksAlong(wp, wq, other);
    at.insert(at.end(), other.begin(), other.end());
    std::sort(at.begin(), at.end());
    at.erase(std::unique(at.begin(), at.end(),
                         [](double a, double b) { return std::fabs(a - b) < 1.0e-3; }),
             at.end());
  }
}

// THE PARAMETERS COME FROM THE FOOTPRINT, WHATEVER RING IS BEING EMITTED. A plinth ring is the
// footprint pushed out by 9 cm, so breaking IT against the overhang lands the split about 10 cm from
// where breaking the footprint does -- and a floor and the plinth wall standing on it then meet along
// two polylines that agree everywhere except at the splits. Sixteen holes in a ten centimetre band
// at the very bottom of a building whose defect I had assumed was in its roof.
std::vector<En> RefinedLike(std::span<const En> along, const std::vector<En> &wide,
                            std::span<const En> emit, const RoofSurface &roof) {
  const size_t n = along.size();
  const bool overhung = wide.size() == n;
  std::vector<En> out;
  std::vector<double> at;
  if (emit.size() != n) { return out; }
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof, along[i], along[j], overhung ? wide[i] : along[i],
               overhung ? wide[j] : along[j], overhung, at);
    out.push_back(emit[i]);
    for (double t : at) { out.push_back(Along(emit[i], emit[j], t)); }
  }
  return out;
}

std::vector<En> Refined(std::span<const En> ring, const std::vector<En> &wide,
                        const RoofSurface &roof, bool takeWide) {
  const size_t n = ring.size();
  const bool overhung = wide.size() == n;
  std::vector<En> out;
  std::vector<double> at;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof, ring[i], ring[j], overhung ? wide[i] : ring[i], overhung ? wide[j] : ring[j],
               overhung, at);
    const En &from = takeWide && overhung ? wide[i] : ring[i];
    const En &to = takeWide && overhung ? wide[j] : ring[j];
    out.push_back(from);
    for (double t : at) { out.push_back(Along(from, to, t)); }
  }
  return out;
}

void Walls(const BuildingShape &s, const RoofSurface &roof, const std::vector<En> &wide,
           double lowZ, double topZ, Site &site) {
  const size_t n = s.Ring.size();
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < 0.05) continue;
    const double bays = s.Party[i] ? 0.0 : BaysOn(len, s.BayM);
    if ((int)i == s.FrontEdge && bays >= 2.0) {
      FrontWall(s, p, q, bays, lowZ, topZ, site);
      continue;
    }
    const bool overhung = wide.size() == n;
    BreaksBoth(roof, p, q, overhung ? wide[i] : p, overhung ? wide[(i + 1) % n] : q, overhung,
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      WallPanel(s, Along(p, q, was), Along(p, q, now), bays * was, bays * now, lowZ, topZ,
                (int)i == s.FrontEdge ? Fields::Entrance : Fields::Back, site);
      was = now;
    }
  }
}

// A BUILDING SITS ON THE HIGHEST GROUND UNDER ITS FOOTPRINT, and the ground has to be SAMPLED to
// know where that is. Reading the ring's CORNERS alone misses any rise between them -- a footprint
// spanning a crest touches its highest point in the middle of an edge, not at an end -- and the
// building then stands below ground along that stretch. Sampled every 2 m, which is finer than the
// terrain mesh's own grid at any zoom that reaches a building, so the walk cannot step over a rise
// the mesh actually carries.
//
// Its limit, stated here: the samples are on the RING, so a rise strictly INSIDE a large footprint
// is still invisible. That is the same question as board:2028's, and a proper answer is a ray query
// against the drawn mesh rather than a denser walk.
constexpr double kGroundStepM = 2.0;

void SampleGround(const BuildingShape &s, const Site2Ground &ground, double *lowest,
                  double *highest) {
  bool first = true;
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    const int steps = 1 + (int)(len / kGroundStepM);
    for (int step = 0; step < steps; ++step) {
      const double at = ground.At(Along(p, q, (double)step / (double)steps));
      if (first) {
        *lowest = *highest = at;
        first = false;
        continue;
      }
      if (at < *lowest) { *lowest = at; }
      if (at > *highest) { *highest = at; }
    }
  }
  if (first) { *lowest = *highest = 0.0; }
}

// HOW FAR A PLINTH REACHES DOWN, derived from the site rather than chosen. A building is placed on
// the DEM and drawn against the terrain MESH, and those two differ by whatever the mesh's grid
// missed -- so a 0.30 m sink leaves a gap under anything on a slope. The spread of the ground across
// the footprint's own ring measures the local gradient over the building's own extent; carrying that
// same gradient one terrain vertex further is the smallest honest cover, and the vertex spacing at
// the finest level is about a kilometre over a 33-wide grid, so roughly 31 m. The building's own
// width is the only length it knows, so the spread is doubled rather than scaled by a grid this tier
// cannot see.
double PlinthFootZ(const BuildingShape &s, const Site2Ground &ground) {
  double lowest = 0.0, highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  const double spread = highest - lowest;
  return lowest - (spread > kSinkM ? 2.0 * spread : kSinkM);
}

// THE LEDGE MEETS THE WALL, so it breaks where the wall breaks. Its inner edge is the wall's foot,
// and a single corner-to-corner edge cannot pair with a foot the roof has already broken at the
// ridge -- which is why holes stood at the BOTTOM of a building whose defect was at the top.
void Plinth(const BuildingShape &s, const RoofSurface &roof, const std::vector<En> &wide,
            const Site2Ground &ground, double topZ, Site &site) {
  const std::vector<En> out = RoofSurface::Widened(s.Ring, kPlinthProudM);
  if (out.size() != s.Ring.size()) return;
  const size_t n = s.Ring.size();
  const double lowZ = PlinthFootZ(s, ground);
  const bool overhung = wide.size() == n;
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof, s.Ring[i], s.Ring[j], overhung ? wide[i] : s.Ring[i],
               overhung ? wide[j] : s.Ring[j], overhung, breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En oa = Along(out[i], out[j], was), ob = Along(out[i], out[j], now);
      const En ra = Along(s.Ring[i], s.Ring[j], was), rb = Along(s.Ring[i], s.Ring[j], now);
      was = now;
      site.Quad(Face(s, oa, lowZ, Facade::Plinth), Face(s, ob, lowZ, Facade::Plinth),
                Face(s, ob, topZ, Facade::Plinth), Face(s, oa, topZ, Facade::Plinth));
      site.Quad(Face(s, oa, topZ, Facade::Ledge), Face(s, ob, topZ, Facade::Ledge),
                Face(s, rb, topZ, Facade::Ledge), Face(s, ra, topZ, Facade::Ledge));
    }
  }
}

// A SOLID IS CLOSED AT THE BOTTOM TOO. Without this the base ring is an open boundary and the
// terrain is what stops you seeing inside -- which holds until the terrain slopes, and stops holding
// the moment it does. The ring is the plinth's outer one where a plinth stands, so the floor meets
// the lowest wall the part actually has, and it is wound the other way round from a roof because its
// outward direction is DOWN.
void Floor(const BuildingShape &s, const std::vector<En> &ring, double atZ, Site &site) {
  std::vector<En> tris;
  (void)RoofSurface::Fill(ring, tris);
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    site.Tri(Face(s, tris[i + 2], atZ, Facade::Plinth), Face(s, tris[i + 1], atZ, Facade::Plinth),
             Face(s, tris[i], atZ, Facade::Plinth));
  }
}

// THE GABLE FOLLOWS THE ROOF, NOT ITS CORNERS. Reading only the two ends saw nothing to build on a
// rectangle whose ridge runs along its long axis: all four corners sit at eaves height, so every
// edge was skipped and the roof rose over an open end. The edge is broken where the covering is
// broken, so the two boundaries pair.
void Gables(const BuildingShape &s, const RoofSurface &roof, const std::vector<En> &wide,
            Site &site) {
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < 0.05) continue;
    const double bays = s.Party[i] ? 0.0 : BaysOn(len, s.BayM);
    const bool overhung = wide.size() == n;
    BreaksBoth(roof, p, q, overhung ? wide[i] : p, overhung ? wide[(i + 1) % n] : q, overhung,
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En a = Along(p, q, was), b = Along(p, q, now);
      const double ha = std::max(roof.HeightAt(a), 0.0), hb = std::max(roof.HeightAt(b), 0.0);
      was = now;
      if (ha < 0.03 && hb < 0.03) continue;
      site.Quad(Wall(s, a, eaves, 0.0, Fields::Back), Wall(s, b, eaves, bays, Fields::Back),
                Wall(s, b, eaves + hb, bays, Fields::Back),
                Wall(s, a, eaves + ha, 0.0, Fields::Back));
    }
  }
}

void Covering(const BuildingShape &s, const RoofSurface &roof, const std::vector<En> &plan,
              double deckZ, Site &site) {
  std::vector<En> tris;
  roof.Cover(plan, tris);
  const Facade kind = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    Vtx v[3];
    for (int k = 0; k < 3; k++)
      v[k] = Face(s, tris[i + (size_t)k], deckZ + roof.HeightAt(tris[i + (size_t)k]) + kSlabM, kind);
    site.Tri(v[0], v[1], v[2]);
  }
}

void Eaves(const BuildingShape &s, const RoofSurface &roof, const std::vector<En> &wide,
           Site &site) {
  const size_t n = s.Ring.size();
  if (wide.size() != n) return;
  const double eaves = EavesZ(s);
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof, s.Ring[i], s.Ring[j], wide[i], wide[j], true, breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En wa = Along(wide[i], wide[j], was), wb = Along(wide[i], wide[j], now);
      const En ra = Along(s.Ring[i], s.Ring[j], was), rb = Along(s.Ring[i], s.Ring[j], now);
      was = now;
      const double za = eaves + roof.HeightAt(wa), zb = eaves + roof.HeightAt(wb);
      const double rza = eaves + roof.HeightAt(ra), rzb = eaves + roof.HeightAt(rb);
      site.Quad(Face(s, ra, rza, Facade::Soffit), Face(s, rb, rzb, Facade::Soffit),
                Face(s, wb, zb, Facade::Soffit), Face(s, wa, za, Facade::Soffit));
      site.Quad(Face(s, wa, za, Facade::Trim), Face(s, wb, zb, Facade::Trim),
                Face(s, wb, zb + kSlabM, Facade::Trim), Face(s, wa, za + kSlabM, Facade::Trim));
    }
  }
}

void Crown(const BuildingShape &s, const std::vector<En> &inner, const std::vector<En> &out,
           Site &site) {
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  const double band = eaves - 0.34, lo = eaves, hi = eaves + s.RiseM;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    site.Quad(Face(s, out[i], band, Facade::Ledge), Face(s, out[j], band, Facade::Ledge),
              Face(s, out[j], lo, Facade::Ledge), Face(s, out[i], lo, Facade::Ledge));
    site.Quad(Face(s, s.Ring[i], band, Facade::Soffit), Face(s, out[i], band, Facade::Soffit),
              Face(s, out[j], band, Facade::Soffit), Face(s, s.Ring[j], band, Facade::Soffit));
    site.Quad(Face(s, out[i], lo, Facade::Ledge), Face(s, out[j], lo, Facade::Ledge),
              Face(s, s.Ring[j], lo, Facade::Ledge), Face(s, s.Ring[i], lo, Facade::Ledge));

    site.Quad(Face(s, s.Ring[i], lo, Facade::Parapet), Face(s, s.Ring[j], lo, Facade::Parapet),
              Face(s, s.Ring[j], hi, Facade::Parapet), Face(s, s.Ring[i], hi, Facade::Parapet));
    site.Quad(Face(s, inner[j], lo, Facade::Parapet), Face(s, inner[i], lo, Facade::Parapet),
              Face(s, inner[i], hi, Facade::Parapet), Face(s, inner[j], hi, Facade::Parapet));
    site.Quad(Face(s, inner[i], hi, Facade::Ledge), Face(s, inner[j], hi, Facade::Ledge),
              Face(s, s.Ring[j], hi, Facade::Ledge), Face(s, s.Ring[i], hi, Facade::Ledge));
  }
}

void Box(Site &site, const BuildingShape &s, const En &centre, double halfU, double halfV,
         double lowZ, double highZ, Facade side) {
  En c[4];
  const En u{s.AxisU.E * halfU, s.AxisU.N * halfU};
  const En v{-s.AxisU.N * halfV, s.AxisU.E * halfV};
  c[0] = {centre.E - u.E - v.E, centre.N - u.N - v.N};
  c[1] = {centre.E + u.E - v.E, centre.N + u.N - v.N};
  c[2] = {centre.E + u.E + v.E, centre.N + u.N + v.N};
  c[3] = {centre.E - u.E + v.E, centre.N - u.N + v.N};
  for (int i = 0; i < 4; i++) {
    const int j = (i + 1) % 4;
    site.Quad(Face(s, c[i], lowZ, side), Face(s, c[j], lowZ, side), Face(s, c[j], highZ, side),
              Face(s, c[i], highZ, side));
  }
  site.Quad(Face(s, c[0], highZ, Facade::Ledge), Face(s, c[1], highZ, Facade::Ledge),
            Face(s, c[2], highZ, Facade::Ledge), Face(s, c[3], highZ, Facade::Ledge));
  // A BOX IS CLOSED AT BOTH ENDS. A chimney and a roof plant stand THROUGH the surface they rise
  // from, so their underside is never seen -- and an unseen face is still a boundary edge to a walk
  // over the triangles, which is what left four holes on every gabled house. Its limit, stated here:
  // the box and the roof interpenetrate rather than being cut against each other, which closedness
  // cannot see and no test in this tree yet can.
  site.Quad(Face(s, c[3], lowZ, Facade::Ledge), Face(s, c[2], lowZ, Facade::Ledge),
            Face(s, c[1], lowZ, Facade::Ledge), Face(s, c[0], lowZ, Facade::Ledge));
}

[[nodiscard]] bool WantsChimney(const BuildingShape &s) {
  if (s.Roof != RoofKind::Gable && s.Roof != RoofKind::Hip && s.Roof != RoofKind::Mansard)
    return false;
  return s.Use == BuildingUse::House || s.Use == BuildingUse::Terrace ||
         s.Use == BuildingUse::Block;
}

void Chimney(const BuildingShape &s, const RoofSurface &roof, Site &site) {
  const double along = (((double)(s.Seed >> 9 & 0xffu) / 255.0) - 0.5) * 1.30 * s.HalfUm;
  const En foot = s.FromBox(along, 0.0);
  const double eaves = EavesZ(s);
  const double stack = eaves + roof.HeightAt(foot) + kChimneyOverRidgeM;
  Box(site, s, foot, 0.5 * kChimneyWideM, 0.4 * kChimneyWideM, eaves, stack, Facade::Trim);
}

void RoofPlant(const BuildingShape &s, double deckZ, Site &site) {
  const double halfU = std::min(2.6, 0.30 * s.HalfUm), halfV = std::min(1.9, 0.30 * s.HalfVm);
  if (halfU < 0.9 || halfV < 0.7) return;
  const double along = (((double)(s.Seed >> 13 & 0xffu) / 255.0) - 0.5) * 0.9 * s.HalfUm;
  const En foot = s.FromBox(along, 0.0);
  Box(site, s, foot, halfU, halfV, deckZ, deckZ + 2.1, Facade::Metal);
}

double PlinthTopZ(const BuildingShape &s, const Site2Ground &ground) {
  double lowest = 0.0, highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  const double seat = highest + kPlinthM;
  // THE MEASURE HAS TO ASK SOMEWHERE THE SEATING DID NOT LOOK, or it cannot fire. A first version
  // compared the ring's own corners against a maximum taken over those same corners: zero by
  // construction, and a count that always reads zero says nothing about a building being buried.
  // These points are INSIDE the footprint, which is exactly the ground `SampleGround` cannot see.
  double deepest = 0.0;
  for (int step = 0; step < 5; ++step) {
    const double u = ((double)(step % 3) - 1.0) * 0.5 * s.HalfUm;
    const double v = ((double)(step / 3) - 0.5) * 0.5 * s.HalfVm;
    deepest = std::max(deepest, ground.At(s.FromBox(u, v)) - seat);
  }
  if (deepest > 0.0) {
    gBuried.fetch_add(1u, std::memory_order_relaxed);
    const size_t mm = (size_t)(deepest * 1000.0);
    size_t was = gDeepestMm.load(std::memory_order_relaxed);
    while (mm > was && !gDeepestMm.compare_exchange_weak(was, mm)) {}
  }
  return seat;
}

// HOW FAR A BUILDING KEEPS ITS ARCHITECTURE, derived from the lens rather than chosen. At 720 px
// over 55 degrees a pixel is 0.076 deg, so a feature of size `w` covers one pixel at
// `w / tan(0.076 deg)` = w / 1.33e-3. A gable, a chimney or an eaves band is of the order of a metre,
// so all of them are inside ONE pixel beyond about 750 m; a whole 27 m building still covers 20 px
// at 20 km. Past the near bound a footprint therefore becomes a plain extruded prism -- the mass a
// skyline is read by -- and everything the eye could not resolve stops being meshed.
//
// This is Unreal's HLOD and RAGE's distant-building proxy, and it is the answer the owner named. It
// is not a cut: the building is still there, still closed, still the right height. What it loses is
// detail no pixel was carrying. At Shibuya the full path meshed 12.9 M triangles and 313 MB of
// vertices from ONE vector tile, and the terrain never got a core to build on.
constexpr double kArchitectureReachM = 1200.0;

void Prism(const BuildingShape &s, const RoofSurface &roof, const Site2Ground &ground, Site &site) {
  const size_t n = s.Ring.size();
  const double lowZ = PlinthFootZ(s, ground);
  const double topZ = s.TopM();
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    site.Quad(Face(s, s.Ring[i], lowZ, Facade::Wall), Face(s, s.Ring[j], lowZ, Facade::Wall),
              Face(s, s.Ring[j], topZ, Facade::Wall), Face(s, s.Ring[i], topZ, Facade::Wall));
  }
  std::vector<En> tris;
  roof.Cover(s.Ring, tris);
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    site.Tri(Face(s, tris[i], topZ, Facade::RoofFlat), Face(s, tris[i + 1], topZ, Facade::RoofFlat),
             Face(s, tris[i + 2], topZ, Facade::RoofFlat));
  }
  Floor(s, s.Ring, lowZ, site);
}

void RaisePart(const BuildingShape &s, const Site2Ground &ground, Site &site) {
  const RoofSurface roof(s);
  if (site.ReachM() > kArchitectureReachM) {
    Prism(s, roof, ground, site);
    return;
  }
  const double plinthZ = PlinthTopZ(s, ground);
  const double lowZ = s.OnGround() ? plinthZ : s.FootM - kSinkM;
  const std::vector<En> overhang = RoofSurface::Widened(s.Ring, s.OverhangM);
  // WHERE THE WALL STOPS IS THE ROOF'S TO SAY. A parapet's cornice juts out from the wall head, so
  // the wall ends at the cornice's UNDERSIDE and the ledge above it belongs to the crown -- ending
  // it level with the deck instead put three surfaces on one edge, which is not a surface at all.
  // A flat roof with no crown carries its deck `RiseM` above the eaves, and a wall stopping at the
  // eaves leaves exactly that much open.
  const std::vector<En> crownInner = RoofSurface::Widened(s.Ring, -kParapetThickM);
  const std::vector<En> crownOut = RoofSurface::Widened(s.Ring, kCorniceM);
  const bool crowned = s.Roof == RoofKind::Flat && crownInner.size() == s.Ring.size() &&
                       crownOut.size() == s.Ring.size() && s.HalfVm > 2.2 && s.RiseM > 0.0;
  const double wallTopZ = s.Roof != RoofKind::Flat ? EavesZ(s)
                          : crowned                ? EavesZ(s) - 0.34
                                                   : EavesZ(s) + s.RiseM;
  if (s.OnGround()) {
    Plinth(s, roof, overhang, ground, plinthZ, site);
    const std::vector<En> proud = RoofSurface::Widened(s.Ring, kPlinthProudM);
    const std::vector<En> foot = RefinedLike(s.Ring, overhang, proud, roof);
    Floor(s, foot.empty() ? s.Ring : foot, PlinthFootZ(s, ground), site);
  } else {
    Floor(s, s.Ring, lowZ, site);
  }
  Walls(s, roof, overhang, lowZ, wallTopZ, site);

  if (s.Roof == RoofKind::Flat) {
    const double deckZ = crowned ? EavesZ(s) : EavesZ(s) + s.RiseM;
    Covering(s, roof, crowned ? crownInner : s.Ring, deckZ, site);
    if (crowned) Crown(s, crownInner, crownOut, site);
    RoofPlant(s, deckZ, site);
    return;
  }

  const std::vector<En> wide = RoofSurface::Widened(s.Ring, s.OverhangM);
  const std::vector<En> covered = Refined(s.Ring, wide, roof, true);
  Covering(s, roof, covered.empty() ? s.Ring : covered, EavesZ(s), site);
  Gables(s, roof, wide, site);
  if (!wide.empty()) Eaves(s, roof, wide, site);
  if (WantsChimney(s)) Chimney(s, roof, site);
}

double StandBack(const Frontage &street, const En &p) {
  return (p.E - street.KerbEm) * street.ToStreetE + (p.N - street.KerbNm) * street.ToStreetN;
}

En OntoKerb(const Frontage &street, const En &p, double back) {
  return {p.E - back * street.ToStreetE, p.N - back * street.ToStreetN};
}

void Pavement(const BuildingShape &s, const Frontage &street, const Site2Ground &ground,
              double plinthZ, Site &site) {
  if (!street.Known || !s.OnGround()) return;
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    if (s.Party[i]) continue;
    const En &p = s.Ring[i], &q = s.Ring[(i + 1) % n];
    const double e = q.E - p.E, nn = q.N - p.N, len = std::hypot(e, nn);
    if (len < 1.2) continue;
    if ((nn / len) * street.ToStreetE - (e / len) * street.ToStreetN < 0.35) continue;
    double bp = StandBack(street, p), bq = StandBack(street, q);
    if (bp > -kPavementLeastM || bq > -kPavementLeastM) continue;
    if (bp < -kPavementMostM && bq < -kPavementMostM) continue;
    bp = std::max(bp, -kFootwayMostM);
    bq = std::max(bq, -kFootwayMostM);

    const En pk = OntoKerb(street, p, bp + kKerbTopM), qk = OntoKerb(street, q, bq + kKerbTopM);
    const En pe = OntoKerb(street, p, bp), qe = OntoKerb(street, q, bq);
    const auto walk = [&](const En &at) {
      return std::min(ground.At(at) + kKerbUpM, plinthZ - 0.05);
    };
    const double zp = walk(p), zq = walk(q), zpk = walk(pk), zqk = walk(qk);
    site.Quad(Face(s, p, zp, Facade::Pavement), Face(s, pk, zpk, Facade::Pavement),
              Face(s, qk, zqk, Facade::Pavement), Face(s, q, zq, Facade::Pavement));
    site.Quad(Face(s, pk, zpk, Facade::Kerb), Face(s, pe, zpk, Facade::Kerb),
              Face(s, qe, zqk, Facade::Kerb), Face(s, qk, zqk, Facade::Kerb));
    site.Quad(Face(s, pe, zpk - kKerbUpM - kKerbSkirtM, Facade::Kerb),
              Face(s, qe, zqk - kKerbUpM - kKerbSkirtM, Facade::Kerb),
              Face(s, qe, zqk, Facade::Kerb), Face(s, pe, zpk, Facade::Kerb));
  }
}

}

void BuildingMesh::Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept {
  if (plan.RingLatLon.Size() < 6 || !plan.AnchorEcef) return;
  const Massing mass = MassOf(plan.RingLatLon, plan.HeightM, plan.HeightMeasured, plan.Street);
  if (mass.Parts.empty()) return;

  Site site(plan, soup);
  const Site2Ground ground(plan.RingLatLon, plan.CornerAslM, plan.BaseAslM);
  for (const BuildingShape &part : mass.Parts) {
    RaisePart(part, ground, site);
    Pavement(part, plan.Street, ground, PlinthTopZ(part, ground), site);
  }
}

}
