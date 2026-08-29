#include "BuildingMesh.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <map>
#include <unordered_map>
#include <vector>

#include "BuildingShape.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

namespace {
std::atomic<size_t> gBuried{0};
std::atomic<size_t> gDeepestMm{0};
std::atomic<size_t> gRaised{0};
std::atomic<size_t> gFarthestM{0};
std::atomic<size_t> gBoxes{0};
std::atomic<size_t> gUnscaled{0};
std::atomic<size_t> gOverBudget{0};
}

size_t BuildingMesh::BuriedTaken() { return gBuried.exchange(0u); }
size_t BuildingMesh::DeepestBuriedMmTaken() { return gDeepestMm.exchange(0u); }
size_t BuildingMesh::RaisedTaken() { return gRaised.exchange(0u); }
size_t BuildingMesh::FarthestMTaken() { return gFarthestM.exchange(0u); }
size_t BuildingMesh::BoxesTaken() { return gBoxes.exchange(0u); }
size_t BuildingMesh::UnscaledTaken() { return gUnscaled.exchange(0u); }
size_t BuildingMesh::OverBudgetTaken() { return gOverBudget.exchange(0u); }


namespace {

constexpr double kSinkM = 0.30;

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
    FocalPx_ = plan.FocalPx;
  }

  [[nodiscard]] double ReachM() const { return ReachM_; }

  [[nodiscard]] double FocalPx() const { return FocalPx_; }

  // THE GENERATOR OWNS ITS OWN TOPOLOGY. Positions are welded on the same centimetre grid everything
  // else in this tree uses, so two corners meant to be one corner ARE one index -- and a shared edge
  // is then a shared edge rather than something a walk has to rediscover afterwards by welding a
  // soup. Unreal's `FMeshDescription` keeps vertices, edges and triangles apart for exactly this
  // reason and splits RENDER vertices by normal and UV on top; the soup below is that split, derived
  // from the topology instead of standing in for it.
  //
  // NOTHING IS DISCARDED. The only triangle that goes is one whose corners are not three distinct
  // indices, and that one costs no edge: its edges are a self-loop and a pair that cancel. A needle
  // and a sliver are BAD GEOMETRY and are the generator's to stop making, never the emitter's to
  // quietly drop -- dropping one takes three edges out of the walk and can hide the very hole it
  // sits beside.
  // SNAPPED, NOT WELDED, and the difference is the whole point. Welding merges positions that have
  // already drifted apart; snapping stops them drifting. Every corner is quantised to a MILLIMETRE
  // as it is emitted, so two corners that mean to be one corner are bit-identical -- the index table
  // below is then bookkeeping over positions that already agree, rather than a repair that has to
  // guess how far apart is still "the same".
  //
  // A millimetre is chosen against the two things that bound it: it is far below anything a frame
  // can show at any distance a building is drawn from, and far above the float noise of the
  // arithmetic that produced it -- a ring widened, split and interpolated three times lands within
  // microns of itself, never within millimetres.
  [[nodiscard]] static Vtx Snapped(const Vtx &v) {
    Vtx out = v;
    out.P.E = std::round(v.P.E * 1000.0) / 1000.0;
    out.P.N = std::round(v.P.N * 1000.0) / 1000.0;
    out.Z = std::round(v.Z * 1000.0) / 1000.0;
    return out;
  }

  [[nodiscard]] uint32_t Index(const Vtx &v) {
    const int64_t ce = (int64_t)std::llround(v.P.E * 1000.0);
    const int64_t cn = (int64_t)std::llround(v.P.N * 1000.0);
    const int64_t cz = (int64_t)std::llround(v.Z * 1000.0);
    const uint64_t key =
        (uint64_t)(ce * 73856093LL) ^ (uint64_t)(cn * 19349663LL) ^ (uint64_t)(cz * 83492791LL);
    const auto found = Welded_.find(key);
    if (found != Welded_.end()) { return found->second; }
    const uint32_t made = (uint32_t)Welded_.size();
    Welded_.emplace(key, made);
    return made;
  }

  void Tri(const Vtx &given0, const Vtx &given1, const Vtx &given2) {
    const Vtx a = Snapped(given0), b = Snapped(given1), c = Snapped(given2);
    const uint32_t ia = Index(a), ib = Index(b), ic = Index(c);
    if (ia == ib || ib == ic || ic == ia) { return; }
    const double e1 = b.P.E - a.P.E, n1 = b.P.N - a.P.N, z1 = b.Z - a.Z;
    const double e2 = c.P.E - a.P.E, n2 = c.P.N - a.P.N, z2 = c.Z - a.Z;
    double nrm[3] = {n1 * z2 - z1 * n2, z1 * e2 - e1 * z2, e1 * n2 - n1 * e2};
    const double len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < 1.0e-12) { return; }
    Face_.push_back(ia);
    Face_.push_back(ib);
    Face_.push_back(ic);
    for (int c2 = 0; c2 < 3; c2++) nrm[c2] /= len;
    Push(a, nrm);
    Push(b, nrm);
    Push(c, nrm);
  }

  // WHAT THE SHELL IT JUST BUILT IS, answered by the generator about itself rather than by a test
  // about its output. An edge on one face is a hole; on more than two it is not a surface; walked
  // twice the same way it is locally inside out.
  void Judged(size_t *open, size_t *overused, size_t *reversed) const {
    std::map<std::pair<uint32_t, uint32_t>, int> walked;
    std::map<std::pair<uint32_t, uint32_t>, int> counted;
    for (size_t at = 0; at + 2 < Face_.size(); at += 3) {
      for (int side = 0; side < 3; ++side) {
        const uint32_t from = Face_[at + (size_t)side], to = Face_[at + (size_t)((side + 1) % 3)];
        const bool ahead = from < to;
        const std::pair<uint32_t, uint32_t> key{ahead ? from : to, ahead ? to : from};
        walked[key] += ahead ? 1 : -1;
        counted[key] += 1;
      }
    }
    for (const auto &edge : counted) {
      if (edge.second == 1) { ++*open; }
      else if (edge.second > 2) { ++*overused; }
      else if (walked.at(edge.first) != 0) { ++*reversed; }
    }
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
  std::unordered_map<uint64_t, uint32_t> Welded_;
  std::vector<uint32_t> Face_;
  double Origin_[3], East_[3], North_[3], Up_[3];
  double ReachM_ = 0.0;
  double FocalPx_ = 0.0;
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
    // THE CORNICE'S UNDERSIDE FACES DOWN and its top faces up, so the two walk the same loop in
    // OPPOSITE senses. They walked it the same way -- `out[i], out[j], Ring[j], Ring[i]` above is
    // this loop rotated by one, not reversed -- which is a shell that is closed and locally inside
    // out, and no hole count can see that. The first attempt at this reversed the TOP instead and
    // the reversed-edge count went 16 to 24, which is how the right one of the two was found.
    site.Quad(Face(s, s.Ring[j], band, Facade::Soffit), Face(s, out[j], band, Facade::Soffit),
              Face(s, out[i], band, Facade::Soffit), Face(s, s.Ring[i], band, Facade::Soffit));
    site.Quad(Face(s, out[i], lo, Facade::Ledge), Face(s, out[j], lo, Facade::Ledge),
              Face(s, s.Ring[j], lo, Facade::Ledge), Face(s, s.Ring[i], lo, Facade::Ledge));

    site.Quad(Face(s, s.Ring[i], lo, Facade::Parapet), Face(s, s.Ring[j], lo, Facade::Parapet),
              Face(s, s.Ring[j], hi, Facade::Parapet), Face(s, s.Ring[i], hi, Facade::Parapet));
    site.Quad(Face(s, inner[j], lo, Facade::Parapet), Face(s, inner[i], lo, Facade::Parapet),
              Face(s, inner[i], hi, Facade::Parapet), Face(s, inner[j], hi, Facade::Parapet));
    // THE COPING SHARES ITS OUTER EDGE WITH THE PARAPET'S OUTER FACE, which reaches `hi` walking
    // `Ring[j] -> Ring[i]`. Walking it the same way here is a seam traversed twice in one direction.
    site.Quad(Face(s, s.Ring[i], hi, Facade::Ledge), Face(s, s.Ring[j], hi, Facade::Ledge),
              Face(s, inner[j], hi, Facade::Ledge), Face(s, inner[i], hi, Facade::Ledge));
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
// WHERE THE DETAIL STOPS PAYING, DERIVED FROM THE OPTICS AND NOT SET. A feature of h metres at d
// metres covers `focalPx * h / d` pixels, with focalPx = H / (2 tan(fov/2)) -- 691.5 px at 720 rows
// over 55 degrees. Below TWO pixels a feature is not merely wasted, it ALIASES, so two pixels is
// where a level stops being worth building.
//
// A LEVEL IS BOUNDED BY THE SILHOUETTE IT ADDS, not by the smallest ornament riding on it. Getting
// that wrong once cost Rothenburg every roof in the town: the cornice is 0.30 m and dies at 104 m,
// so gating ARCHITECTURE on the cornice meant no building anywhere was ever built with a roof.
//
//   what a level adds        size    holds 2 px to
//   roof SHAPE (the rise)    3.0 m       1037 m
//   footprint corners        2.0 m        692 m
//   cornice, plinth, bays    0.3 m        104 m
//
// AND THOSE ARE TWO AXES, NOT ONE -- which is the answer to "are three levels enough". Footprint
// fidelity dies at 692 m and roof shape not until 1037 m, so a FLAT roof on a TRUE footprint is
// never the right trade: there is no band in which it wins. The level that belongs in that gap is
// its mirror -- a shaped roof over a HULL footprint -- and it is not built here, so the ornament
// rides along with the roof to 1037 m and the box takes over beyond it. RAGE carries High/Med/Low/
// Vlow inside a drawable and then SLOD1..4 of merged sectors on top; Unreal carries 4 to 8 mesh LODs
// and HLOD clusters over them, and Nanite drops discrete levels entirely for a cluster cut at a
// constant screen error. More than three, and for this reason.
//
// THERE IS NO LEVEL BELOW THE BOX. A building under a pixel still darkens the pixel it is under, and
// dropping it is exactly how a town stops being a town.
constexpr double kRoofRiseM = 3.0;
constexpr double kResolvedPx = 2.0;

// AND A SECOND BOUND, WHICH IS THE ONE THAT WAS MISSING. The test above asks whether a level's
// FEATURE can be seen. It never asked whether the level's TRIANGLES fit the pixels the building
// covers -- so full architecture stood out to 1037 m, where a ten-metre building covers 6.7 pixels,
// and put 262 triangles on them. Thirty-nine triangles a pixel.
//
// A building of height h and silhouette width w at distance d covers about f^2 h w / d^2 pixels, so
// a level costing T triangles is admissible only while T <= that:
//
//     d <= f * sqrt(h * w / T)
//
// For a 10 by 15 m house: full architecture (T = 262) to 523 m, a box (T = 12) to 2446 m. This is
// Nanite's invariant stated as a rule the GENERATOR can obey -- never build more geometry than the
// screen can show -- and it is the bound that answers 31 M vertices for a 0.92 M pixel frame.
//
// The two bounds are both necessary and neither implies the other: the feature test says the detail
// would be invisible, the triangle test says it would not fit. The reach is the SMALLER of them.
constexpr double kArchitectureTris = 262.0;
constexpr double kBoxTris = 12.0;

[[nodiscard]] double FitsInPixelsM(double focalPx, double heightM, double widthM, double tris) {
  if (heightM <= 0.0 || widthM <= 0.0 || tris <= 0.0) { return 0.0; }
  return focalPx * std::sqrt(heightM * widthM / tris);
}

[[nodiscard]] double ArchitectureReachM(double focalPx) {
  return focalPx * kRoofRiseM / kResolvedPx;
}

// THE MINIMUM-AREA ENCLOSING RECTANGLE, not an axis-aligned one: a building at 40 degrees to the
// grid would otherwise gain a silhouette half again its own width, and a silhouette is the only
// thing this level still carries. A minimum-area rectangle always has a side collinear with a hull
// edge, so trying every ring edge finds it -- the ring is a superset of its hull and n is small.
[[nodiscard]] std::vector<En> Hull(const std::vector<En> &ring) {
  const size_t n = ring.size();
  double bestArea = 1.0e300;
  double axE = 1.0, axN = 0.0, minU = 0.0, maxU = 0.0, minV = 0.0, maxV = 0.0;
  for (size_t i = 0; i < n; i++) {
    const En &a = ring[i], &b = ring[(i + 1) % n];
    const double dE = b.E - a.E, dN = b.N - a.N;
    const double len = std::hypot(dE, dN);
    if (len < 1.0e-6) { continue; }
    const double uE = dE / len, uN = dN / len;
    double loU = 1.0e300, hiU = -1.0e300, loV = 1.0e300, hiV = -1.0e300;
    for (const En &p : ring) {
      const double u = p.E * uE + p.N * uN, v = -p.E * uN + p.N * uE;
      loU = std::min(loU, u); hiU = std::max(hiU, u);
      loV = std::min(loV, v); hiV = std::max(hiV, v);
    }
    const double area = (hiU - loU) * (hiV - loV);
    if (area < bestArea) {
      bestArea = area;
      axE = uE; axN = uN; minU = loU; maxU = hiU; minV = loV; maxV = hiV;
    }
  }
  const auto at = [&](double u, double v) {
    return En{u * axE - v * axN, u * axN + v * axE};
  };
  return {at(minU, minV), at(maxU, minV), at(maxU, maxV), at(minU, maxV)};
}

void Box(const BuildingShape &s, const std::vector<En> &ring, const Site2Ground &ground,
         Site &site) {
  const double lowZ = PlinthFootZ(s, ground);
  const double topZ = s.TopM();
  for (size_t i = 0; i < 4; i++) {
    const size_t j = (i + 1) % 4;
    site.Quad(Face(s, ring[i], lowZ, Facade::Wall), Face(s, ring[j], lowZ, Facade::Wall),
              Face(s, ring[j], topZ, Facade::Wall), Face(s, ring[i], topZ, Facade::Wall));
  }
  site.Quad(Face(s, ring[0], topZ, Facade::RoofFlat), Face(s, ring[1], topZ, Facade::RoofFlat),
            Face(s, ring[2], topZ, Facade::RoofFlat), Face(s, ring[3], topZ, Facade::RoofFlat));
  Floor(s, ring, lowZ, site);
}

void RaisePart(const BuildingShape &s, const Site2Ground &ground, Site &site) {
  const double outM = site.ReachM();
  const size_t whole = (size_t)outM;
  for (size_t seen = gFarthestM.load(); whole > seen;) {
    if (gFarthestM.compare_exchange_weak(seen, whole)) { break; }
  }
  const double focalPx = site.FocalPx();
  if (focalPx > 0.0) {
    double leastE = 1.0e300, mostE = -1.0e300, leastN = 1.0e300, mostN = -1.0e300;
    for (const En &p : s.Ring) {
      leastE = std::min(leastE, p.E); mostE = std::max(mostE, p.E);
      leastN = std::min(leastN, p.N); mostN = std::max(mostN, p.N);
    }
    const double wideM = 0.5 * ((mostE - leastE) + (mostN - leastN));
    const double highM = s.TopM() - PlinthFootZ(s, ground);
    const double asDetailed =
        std::min(ArchitectureReachM(focalPx), FitsInPixelsM(focalPx, highM, wideM, kArchitectureTris));
    // WHERE EVEN A BOX IS TOO MUCH, counted rather than claimed. Twelve triangles stop fitting at
    // f * sqrt(h w / 12) -- 2446 m for a ten-by-fifteen house -- and there is no level below the
    // box in this tree. Dropping the building is not it: a building under a pixel still darkens the
    // pixel it is under, and dropping it is how a town stops being a town. Nor is dropping the
    // box's FLOOR, which looks free because it is buried and is not: it is what keeps the solid
    // closed, and `geo/ScoreWhetherEveryBuildingIsASolid` walks exactly that. So what belongs here
    // is the merged level, and this counter is what says how much it would be worth.
    if (outM > FitsInPixelsM(focalPx, highM, wideM, kBoxTris)) {
      gOverBudget.fetch_add(1u, std::memory_order_relaxed);
    }
    if (outM > asDetailed) {
      gBoxes.fetch_add(1u, std::memory_order_relaxed);
      Box(s, Hull(s.Ring), ground, site);
      return;
    }
  } else {
    gUnscaled.fetch_add(1u, std::memory_order_relaxed);
  }
  gRaised.fetch_add(1u, std::memory_order_relaxed);
  const RoofSurface roof(s);
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
    // THE DECK MEETS THE PARAPET'S INNER FOOT. `Covering` lays its surface a slab's thickness above
    // the height it is handed, and the parapet's inner face starts at the eaves -- so handing it the
    // eaves left the deck floating `kSlabM` above the wall meant to hold it, all the way round.
    const double deckZ = crowned ? EavesZ(s) - kSlabM : EavesZ(s) + s.RiseM;
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
