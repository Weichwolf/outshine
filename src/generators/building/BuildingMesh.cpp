#include "math/Units.h"
#include "Digest.h"
#include <generate/Generate.h>

#include "BuildingMesh.h"

#include "ground/TileMeshes.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Geodesy.h"
#include "RoofSurface.h"

namespace outshine::Generators {

constexpr double kPlinthClearM = 0.05;

constexpr double kFrontLeastLook = 0.35;

constexpr double kChimneyHalfWide = 0.5;
constexpr double kChimneyHalfDeep = 0.4;
constexpr double kDormerLeastHalfUm = 0.9;
constexpr double kDormerLeastHalfVm = 0.7;
constexpr double kDormerRiseM = 2.1;
constexpr double kFrontLeastEdgeM = 1.2;

namespace {

constexpr double kWeldPerM = 1000.0;
constexpr double kLeastWallM = 1.9;
constexpr double kSameHeightM = 1.0e-3;
constexpr double kLeastEdgeM = 0.05;
constexpr double kLeastRiseM = 0.03;

} // namespace

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
    case BuildingUse::Terrace: return FacadeStyle::Terrace;
    case BuildingUse::Block: return FacadeStyle::Block;
    case BuildingUse::Hall: return FacadeStyle::Hall;
    case BuildingUse::Tower: return FacadeStyle::Tower;
    case BuildingUse::Spire: return FacadeStyle::Spire;
    case BuildingUse::House: break;
  }
  return FacadeStyle::House;
}

double EavesZ(const BuildingShape &s) {
  return s.SeatM + s.FootM + s.EavesM;
}

Vtx Wall(const BuildingShape &s, const En &p, double z, double bays, Fields stand) {
  return {.P = p,
          .Z = z,
          .U = FacadeUvX(StyleOf(s.Use), stand, static_cast<float>(bays)),
          .V = FacadeUvY(s.Ident, static_cast<float>((z - s.SeatM - s.FootM) / s.FloorM))};
}

Vtx Face(const BuildingShape &s, const En &p, double z, Facade kind) {
  return {.P = p, .Z = z, .U = FaceUvX(kind, s.Ident), .V = static_cast<float>(z)};
}

class Site {
public:
  Site(const StructurePlan &plan, BuildingScratch &scratch, Raised &into)
      : Out_(into), Scratch_(scratch) {
    Scratch_.ClearWelds();
    const double lat = plan.RingLatLon[0];
    const double lon = plan.RingLatLon[1];
    Vec3 origin;
    GeoToEcef({.LongitudeDeg = lon, .LatitudeDeg = lat, .HeightM = plan.BaseAslM}, origin);
    const EnuAxes axes = EnuAxesEcef({.LongitudeDeg = lon, .LatitudeDeg = lat});
    East_ = axes.East;
    North_ = axes.North;
    Up_ = axes.Up;
    for (int c = 0; c < 3; c++) { Origin_[c] = origin[c] - plan.AnchorEcef[c]; }
    ReachM_ =
        std::sqrt(Origin_[0] * Origin_[0] + Origin_[1] * Origin_[1] + Origin_[2] * Origin_[2]);
    FocalPx_ = plan.FocalPx;
    Coarseness_ = plan.Coarseness;
  }

  [[nodiscard]] double ReachM() const { return ReachM_; }

  [[nodiscard]] double FocalPx() const { return FocalPx_; }

  [[nodiscard]] Detail Coarseness() const { return Coarseness_; }

  [[nodiscard]] BuildingScratch &Scratch() { return Scratch_; }

  [[nodiscard]] static Vtx Snapped(const Vtx &v) {
    Vtx out = v;
    out.P.EastM = std::round(v.P.EastM * kWeldPerM) / kWeldPerM;
    out.P.NorthM = std::round(v.P.NorthM * kWeldPerM) / kWeldPerM;
    out.Z = std::round(v.Z * kWeldPerM) / kWeldPerM;
    return out;
  }

  [[nodiscard]] uint32_t Index(const Vtx &v) {
    const auto ce = static_cast<int64_t>(std::llround(v.P.EastM * 1000.0));
    const auto cn = static_cast<int64_t>(std::llround(v.P.NorthM * 1000.0));
    const auto cz = static_cast<int64_t>(std::llround(v.Z * 1000.0));
    const uint64_t key = static_cast<uint64_t>(ce * 73856093LL) ^
                         static_cast<uint64_t>(cn * 19349663LL) ^
                         static_cast<uint64_t>(cz * 83492791LL);
    if (const uint32_t *found = Scratch_.Welded.Find(key)) { return *found; }
    const auto made = static_cast<uint32_t>(Scratch_.Welded.Size());
    (void)Scratch_.Welded.Emplace(key, made);
    return made;
  }

  void Tri(const Vtx &given0, const Vtx &given1, const Vtx &given2) {
    const Vtx a = Snapped(given0);
    const Vtx b = Snapped(given1);
    const Vtx c = Snapped(given2);
    const uint32_t ia = Index(a);
    const uint32_t ib = Index(b);
    const uint32_t ic = Index(c);
    if (ia == ib || ib == ic || ic == ia) { return; }
    const double e1 = b.P.EastM - a.P.EastM;
    const double n1 = b.P.NorthM - a.P.NorthM;
    const double z1 = b.Z - a.Z;
    const double e2 = c.P.EastM - a.P.EastM;
    const double n2 = c.P.NorthM - a.P.NorthM;
    const double z2 = c.Z - a.Z;
    Vec3 nrm = {{n1 * z2 - z1 * n2, z1 * e2 - e1 * z2, e1 * n2 - n1 * e2}};
    const double len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < kParallelCross) { return; }
    for (double &c2 : nrm) { c2 /= len; }
    const int side = nrm[2] > kSteepestRoof ? 1 : 0;
    std::vector<uint32_t> &run = side == 1 ? Out_.RoofRun : Out_.WallRun;
    run.push_back(Corner(side, a, ia, nrm));
    run.push_back(Corner(side, b, ib, nrm));
    run.push_back(Corner(side, c, ic, nrm));
  }

  void Quad(const Vtx &a, const Vtx &b, const Vtx &c, const Vtx &d) {
    Tri(a, b, c);
    Tri(a, c, d);
  }

private:
  [[nodiscard]] uint32_t Corner(int side, const Vtx &v, uint32_t at, const Vec3 &nrm) {
    std::vector<StoredVertex> &soup = side == 1 ? Out_.RoofCorners : Out_.WallCorners;
    const uint64_t facing =
        (static_cast<uint64_t>(
             static_cast<uint32_t>(static_cast<int32_t>(std::llround(nrm[0] * 4096.0))))
         << 42u) ^
        (static_cast<uint64_t>(
             static_cast<uint32_t>(static_cast<int32_t>(std::llround(nrm[1] * 4096.0))))
         << 21u) ^
        static_cast<uint64_t>(
            static_cast<uint32_t>(static_cast<int32_t>(std::llround(nrm[2] * 4096.0))));
    const uint64_t key = (static_cast<uint64_t>(at) * kDigestPrime) ^ facing;
    FlatMap<uint32_t> &corners = Scratch_.Corners[static_cast<size_t>(side)];
    if (const uint32_t *found = corners.Find(key)) { return *found; }
    const auto made = static_cast<uint32_t>(soup.size());
    (void)corners.Emplace(key, made);
    Vec3f placeM{};
    Vec3f turned{};
    for (int c = 0; c < 3; c++) {
      placeM[static_cast<size_t>(c)] = static_cast<float>(Origin_[c] + v.P.EastM * East_[c] +
                                                          v.P.NorthM * North_[c] + v.Z * Up_[c]);
      turned[static_cast<size_t>(c)] =
          static_cast<float>(nrm[0] * East_[c] + nrm[1] * North_[c] + nrm[2] * Up_[c]);
    }
    soup.push_back(StoredVertex::Of(placeM, Vec2f{{v.U, v.V}}, turned));
    return made;
  }

  Raised &Out_;
  BuildingScratch &Scratch_;
  Vec3 Origin_, East_, North_, Up_;
  double ReachM_ = 0.0;
  double FocalPx_ = 0.0;
  Detail Coarseness_ = Detail::Fine;
};

struct Levels {
  double BaseAslM = 0.0;
  double SeatAslM = 0.0;
  double FootAslM = 0.0;
};

class Site2Ground {
public:
  Site2Ground(std::span<const double> ringLatLon, std::span<const double> cornerAslM, Levels at)
      : HighM_(at.SeatAslM - at.BaseAslM), LowM_(at.FootAslM - at.BaseAslM) {
    const double baseAslM = at.BaseAslM;
    const size_t n = std::min(cornerAslM.size(), ringLatLon.size() / 2);
    if (n < 3) { return; }
    std::array<std::array<double, 4>, 3> m = {};
    for (size_t k = 0; k < n; k++) {
      const EastNorth away =
          EnuOffsetM({.LongitudeDeg = ringLatLon[1], .LatitudeDeg = ringLatLon[0]},
                     {.LongitudeDeg = ringLatLon[k * 2 + 1], .LatitudeDeg = ringLatLon[k * 2]});
      const double z = cornerAslM[k] - baseAslM;
      const Vec3 b = {{1.0, away.EastM, away.NorthM}};
      for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) { m[r][c] += b[r] * b[c]; }
        m[r][3] += b[r] * z;
      }
    }
    for (int c = 0; c < 3; c++) {
      int piv = c;
      for (int r = c + 1; r < 3; r++) {
        if (std::fabs(m[r][c]) > std::fabs(m[piv][c])) { piv = r; }
      }
      if (std::fabs(m[piv][c]) < kLeastRunM) { return; }
      for (int k = 0; k < 4; k++) { std::swap(m[c][k], m[piv][k]); }
      for (int r = 0; r < 3; r++) {
        if (r == c) { continue; }
        const double f = m[r][c] / m[c][c];
        for (int k = c; k < 4; k++) { m[r][k] -= f * m[c][k]; }
      }
    }
    Const_ = m[0][3] / m[0][0];
    SlopeE_ = m[1][3] / m[1][1];
    SlopeN_ = m[2][3] / m[2][2];
  }

  [[nodiscard]] double At(const En &p) const {
    return Const_ + SlopeE_ * p.EastM + SlopeN_ * p.NorthM;
  }

  [[nodiscard]] double High() const { return HighM_; }

  [[nodiscard]] double Low() const { return LowM_; }

private:
  double Const_ = 0.0, SlopeE_ = 0.0, SlopeN_ = 0.0;
  double HighM_ = 0.0, LowM_ = 0.0;
};

double EdgeLength(const En &p, const En &q) {
  return std::hypot(q.EastM - p.EastM, q.NorthM - p.NorthM);
}

En Along(const En &p, const En &q, double t) {
  return {.EastM = p.EastM + (q.EastM - p.EastM) * t,
          .NorthM = p.NorthM + (q.NorthM - p.NorthM) * t};
}

double BaysOn(double lengthM, double bayM) {
  if (lengthM < kLeastWallM) { return 0.0; }
  return std::max(1.0, std::round(lengthM / bayM));
}

void WallPanel(const BuildingShape &s,
               const En &p,
               const En &q,
               double bay0,
               double bay1,
               double lowZ,
               double highZ,
               Fields stand,
               Site &site) {
  site.Quad(Wall(s, p, lowZ, bay0, stand),
            Wall(s, q, lowZ, bay1, stand),
            Wall(s, q, highZ, bay1, stand),
            Wall(s, p, highZ, bay0, stand));
}

void FrontWall(const BuildingShape &s,
               const En &p,
               const En &q,
               double bays,
               double lowZ,
               double highZ,
               Site &site) {
  const double door = std::floor(0.5 * bays);
  const double t0 = door / bays;
  const double t1 = (door + 1.0) / bays;
  const En a = Along(p, q, t0);
  const En b = Along(p, q, t1);
  if (door > 0.0) { WallPanel(s, p, a, 0.0, door, lowZ, highZ, Fields::Front, site); }
  WallPanel(s, a, b, door, door + 1.0, lowZ, highZ, Fields::Entrance, site);
  if (door + 1.0 < bays) { WallPanel(s, b, q, door + 1.0, bays, lowZ, highZ, Fields::Front, site); }
}

struct Stretch {
  En From;
  En To;
};

struct Breaking {
  Stretch Face;
  Stretch Eave;
  bool Overhung = false;
};

void BreaksBoth(const RoofSurface &roof,
                Breaking along,
                BuildingScratch &scratch,
                std::vector<double> &at) {
  const Stretch &face = along.Face;
  const Stretch &eave = along.Eave;
  const bool overhung = along.Overhung;
  std::vector<double> &other = scratch.Other;
  roof.BreaksAlong(face.From, face.To, at);
  if (overhung) {
    roof.BreaksAlong(eave.From, eave.To, other);
    at.insert(at.end(), other.begin(), other.end());
    std::ranges::sort(at);
    at.erase(std::ranges::unique(at,

                                 [](double a, double b) { return std::fabs(a - b) < kSameHeightM; })
                 .begin(),
             at.end());
  }
}

void RefinedLike(std::span<const En> along,
                 std::span<const En> wide,
                 std::span<const En> emit,
                 const RoofSurface &roof,
                 BuildingScratch &scratch,
                 std::vector<En> &out) {
  const size_t n = along.size();
  const bool overhung = wide.size() == n;
  out.clear();
  std::vector<double> &at = scratch.At;
  if (emit.size() != n) { return; }
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(
        roof,
        {.Face = {.From = along[i], .To = along[j]},
         .Eave = {.From = overhung ? wide[i] : along[i], .To = overhung ? wide[j] : along[j]},
         .Overhung = overhung},
        scratch,
        at);
    out.push_back(emit[i]);
    for (const double t : at) { out.push_back(Along(emit[i], emit[j], t)); }
  }
}

void Refined(std::span<const En> ring,
             std::span<const En> wide,
             const RoofSurface &roof,
             bool takeWide,
             BuildingScratch &scratch,
             std::vector<En> &out) {
  const size_t n = ring.size();
  const bool overhung = wide.size() == n;
  out.clear();
  std::vector<double> &at = scratch.At;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof,
               {.Face = {.From = ring[i], .To = ring[j]},
                .Eave = {.From = overhung ? wide[i] : ring[i], .To = overhung ? wide[j] : ring[j]},
                .Overhung = overhung},
               scratch,
               at);
    const En &from = takeWide && overhung ? wide[i] : ring[i];
    const En &to = takeWide && overhung ? wide[j] : ring[j];
    out.push_back(from);
    for (const double t : at) { out.push_back(Along(from, to, t)); }
  }
}

void Walls(const BuildingShape &s,
           const RoofSurface &roof,
           std::span<const En> wide,
           double lowZ,
           double topZ,
           Site &site) {
  const size_t n = s.Ring.size();
  std::vector<double> &breaks = site.Scratch().Breaks;
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i];
    const En &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < kLeastEdgeM) { continue; }
    const double bays = (s.Party[i] != 0u) ? 0.0 : BaysOn(len, s.BayM);
    if (std::cmp_equal(i, s.FrontEdge) && bays >= 2.0) {
      FrontWall(s, p, q, bays, lowZ, topZ, site);
      continue;
    }
    const bool overhung = wide.size() == n;
    BreaksBoth(roof,
               {.Face = {.From = p, .To = q},
                .Eave = {.From = overhung ? wide[i] : p, .To = overhung ? wide[(i + 1) % n] : q},
                .Overhung = overhung},
               site.Scratch(),
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      WallPanel(s,
                Along(p, q, was),
                Along(p, q, now),
                bays * was,
                bays * now,
                lowZ,
                topZ,
                std::cmp_equal(i, s.FrontEdge) ? Fields::Entrance : Fields::Back,
                site);
      was = now;
    }
  }
}

constexpr double kGroundStepM = 2.0;

void SampleGround(const BuildingShape &s,
                  const Site2Ground &ground,
                  double *lowest,
                  double *highest) {
  bool first = true;
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i];
    const En &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    const int steps = 1 + static_cast<int>(len / kGroundStepM);
    for (int step = 0; step < steps; ++step) {
      const double at =
          ground.At(Along(p, q, static_cast<double>(step) / static_cast<double>(steps)));
      if (first) {
        *lowest = *highest = at;
        first = false;
        continue;
      }
      *lowest = std::min(at, *lowest);
      *highest = std::max(at, *highest);
    }
  }
  if (first) { *lowest = *highest = 0.0; }
}

double PlinthFootZ(const BuildingShape &s, const Site2Ground &ground) {
  double lowest = 0.0;
  double highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  lowest = std::min(lowest, ground.Low());
  highest = std::max(highest, ground.High());
  const double spread = highest - lowest;
  return lowest - (spread > kSinkM ? 2.0 * spread : kSinkM);
}

void Plinth(const BuildingShape &s,
            const RoofSurface &roof,
            std::span<const En> wide,
            double topZ,
            Site &site) {
  std::vector<En> &out = site.Scratch().Proud;
  RoofSurface::Widened(s.Ring, kPlinthProudM, {}, out);
  if (out.size() != s.Ring.size()) { return; }
  const size_t n = s.Ring.size();
  const double lowZ = s.SoleM;
  const bool overhung = wide.size() == n;
  std::vector<double> &breaks = site.Scratch().Breaks;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(
        roof,
        {.Face = {.From = s.Ring[i], .To = s.Ring[j]},
         .Eave = {.From = overhung ? wide[i] : s.Ring[i], .To = overhung ? wide[j] : s.Ring[j]},
         .Overhung = overhung},
        site.Scratch(),
        breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En oa = Along(out[i], out[j], was);
      const En ob = Along(out[i], out[j], now);
      const En ra = Along(s.Ring[i], s.Ring[j], was);
      const En rb = Along(s.Ring[i], s.Ring[j], now);
      was = now;
      site.Quad(Face(s, oa, lowZ, Facade::Plinth),
                Face(s, ob, lowZ, Facade::Plinth),
                Face(s, ob, topZ, Facade::Plinth),
                Face(s, oa, topZ, Facade::Plinth));
      site.Quad(Face(s, oa, topZ, Facade::Ledge),
                Face(s, ob, topZ, Facade::Ledge),
                Face(s, rb, topZ, Facade::Ledge),
                Face(s, ra, topZ, Facade::Ledge));
    }
  }
}

void Floor(const BuildingShape &s, std::span<const En> ring, double atZ, Site &site) {
  std::vector<En> &tris = site.Scratch().Tris;
  tris.clear();
  (void)RoofSurface::Fill(ring, site.Scratch(), tris);
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    site.Tri(Face(s, tris[i + 2], atZ, Facade::Plinth),
             Face(s, tris[i + 1], atZ, Facade::Plinth),
             Face(s, tris[i], atZ, Facade::Plinth));
  }
}

void Gables(const BuildingShape &s, const RoofSurface &roof, std::span<const En> wide, Site &site) {
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  std::vector<double> &breaks = site.Scratch().Breaks;
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i];
    const En &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < kLeastEdgeM) { continue; }
    const double bays = (s.Party[i] != 0u) ? 0.0 : BaysOn(len, s.BayM);
    const bool overhung = wide.size() == n;
    BreaksBoth(roof,
               {.Face = {.From = p, .To = q},
                .Eave = {.From = overhung ? wide[i] : p, .To = overhung ? wide[(i + 1) % n] : q},
                .Overhung = overhung},
               site.Scratch(),
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En a = Along(p, q, was);
      const En b = Along(p, q, now);
      const double ha = std::max(roof.HeightAt(a), 0.0);
      const double hb = std::max(roof.HeightAt(b), 0.0);
      was = now;
      if (ha < kLeastRiseM && hb < kLeastRiseM) { continue; }
      site.Quad(Wall(s, a, eaves, 0.0, Fields::Back),
                Wall(s, b, eaves, bays, Fields::Back),
                Wall(s, b, eaves + hb, bays, Fields::Back),
                Wall(s, a, eaves + ha, 0.0, Fields::Back));
    }
  }
}

void Covering(const BuildingShape &s,
              const RoofSurface &roof,
              std::span<const En> plan,
              double deckZ,
              Site &site) {
  std::vector<En> &tris = site.Scratch().Tris;
  tris.clear();
  roof.Cover(plan, site.Scratch(), tris);
  const Facade kind = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    std::array<Vtx, 3> v{};
    for (int k = 0; k < 3; k++) {
      v[k] = Face(s,
                  tris[i + static_cast<size_t>(k)],
                  deckZ + roof.HeightAt(tris[i + static_cast<size_t>(k)]) + kSlabM,
                  kind);
    }
    site.Tri(v[0], v[1], v[2]);
  }
}

void Eaves(const BuildingShape &s, const RoofSurface &roof, std::span<const En> wide, Site &site) {
  const size_t n = s.Ring.size();
  if (wide.size() != n) { return; }
  const double eaves = EavesZ(s);
  std::vector<double> &breaks = site.Scratch().Breaks;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof,
               {.Face = {.From = s.Ring[i], .To = s.Ring[j]},
                .Eave = {.From = wide[i], .To = wide[j]},
                .Overhung = true},
               site.Scratch(),
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En wa = Along(wide[i], wide[j], was);
      const En wb = Along(wide[i], wide[j], now);
      const En ra = Along(s.Ring[i], s.Ring[j], was);
      const En rb = Along(s.Ring[i], s.Ring[j], now);
      was = now;
      const double za = eaves + roof.HeightAt(wa);
      const double zb = eaves + roof.HeightAt(wb);
      const double rza = eaves + roof.HeightAt(ra);
      const double rzb = eaves + roof.HeightAt(rb);
      site.Quad(Face(s, ra, rza, Facade::Soffit),
                Face(s, rb, rzb, Facade::Soffit),
                Face(s, wb, zb, Facade::Soffit),
                Face(s, wa, za, Facade::Soffit));
      site.Quad(Face(s, wa, za, Facade::Trim),
                Face(s, wb, zb, Facade::Trim),
                Face(s, wb, zb + kSlabM, Facade::Trim),
                Face(s, wa, za + kSlabM, Facade::Trim));
    }
  }
}

struct Crowning {
  std::span<const En> Inner;
  std::span<const En> Out;
};

void Crown(const BuildingShape &s, Crowning over, Site &site) {
  const std::span<const En> inner = over.Inner;
  const std::span<const En> out = over.Out;
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  const double band = eaves - 0.34;
  const double lo = eaves;
  const double hi = eaves + s.RiseM;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    site.Quad(Face(s, out[i], band, Facade::Ledge),
              Face(s, out[j], band, Facade::Ledge),
              Face(s, out[j], lo, Facade::Ledge),
              Face(s, out[i], lo, Facade::Ledge));
    site.Quad(Face(s, s.Ring[j], band, Facade::Soffit),
              Face(s, out[j], band, Facade::Soffit),
              Face(s, out[i], band, Facade::Soffit),
              Face(s, s.Ring[i], band, Facade::Soffit));
    site.Quad(Face(s, out[i], lo, Facade::Ledge),
              Face(s, out[j], lo, Facade::Ledge),
              Face(s, s.Ring[j], lo, Facade::Ledge),
              Face(s, s.Ring[i], lo, Facade::Ledge));

    site.Quad(Face(s, s.Ring[i], lo, Facade::Parapet),
              Face(s, s.Ring[j], lo, Facade::Parapet),
              Face(s, s.Ring[j], hi, Facade::Parapet),
              Face(s, s.Ring[i], hi, Facade::Parapet));
    site.Quad(Face(s, inner[j], lo, Facade::Parapet),
              Face(s, inner[i], lo, Facade::Parapet),
              Face(s, inner[i], hi, Facade::Parapet),
              Face(s, inner[j], hi, Facade::Parapet));
    site.Quad(Face(s, s.Ring[i], hi, Facade::Ledge),
              Face(s, s.Ring[j], hi, Facade::Ledge),
              Face(s, inner[j], hi, Facade::Ledge),
              Face(s, inner[i], hi, Facade::Ledge));
  }
}

struct Halves {
  double U = 0.0;
  double V = 0.0;
};

struct Storey {
  double LowM = 0.0;
  double HighM = 0.0;
};

void Box(
    Site &site, const BuildingShape &s, const En &centre, Halves half, Storey over, Facade side) {
  const double halfU = half.U;
  const double halfV = half.V;
  const double lowZ = over.LowM;
  const double highZ = over.HighM;
  std::array<En, 4> c{};
  const En u{.EastM = s.AxisU.EastM * halfU, .NorthM = s.AxisU.NorthM * halfU};
  const En v{.EastM = -s.AxisU.NorthM * halfV, .NorthM = s.AxisU.EastM * halfV};
  c[0] = {.EastM = centre.EastM - u.EastM - v.EastM, .NorthM = centre.NorthM - u.NorthM - v.NorthM};
  c[1] = {.EastM = centre.EastM + u.EastM - v.EastM, .NorthM = centre.NorthM + u.NorthM - v.NorthM};
  c[2] = {.EastM = centre.EastM + u.EastM + v.EastM, .NorthM = centre.NorthM + u.NorthM + v.NorthM};
  c[3] = {.EastM = centre.EastM - u.EastM + v.EastM, .NorthM = centre.NorthM - u.NorthM + v.NorthM};
  for (int i = 0; i < 4; i++) {
    const int j = (i + 1) % 4;
    site.Quad(Face(s, c[i], lowZ, side),
              Face(s, c[j], lowZ, side),
              Face(s, c[j], highZ, side),
              Face(s, c[i], highZ, side));
  }
  site.Quad(Face(s, c[0], highZ, Facade::Ledge),
            Face(s, c[1], highZ, Facade::Ledge),
            Face(s, c[2], highZ, Facade::Ledge),
            Face(s, c[3], highZ, Facade::Ledge));
  site.Quad(Face(s, c[3], lowZ, Facade::Ledge),
            Face(s, c[2], lowZ, Facade::Ledge),
            Face(s, c[1], lowZ, Facade::Ledge),
            Face(s, c[0], lowZ, Facade::Ledge));
}

[[nodiscard]] bool WantsChimney(const BuildingShape &s) {
  if (s.Roof != RoofKind::Gable && s.Roof != RoofKind::Hip && s.Roof != RoofKind::Mansard) {
    return false;
  }
  return s.Use == BuildingUse::House || s.Use == BuildingUse::Terrace ||
         s.Use == BuildingUse::Block;
}

void Chimney(const BuildingShape &s, const RoofSurface &roof, Site &site) {
  const double along =
      ((static_cast<double>(s.Seed >> 9u & 0xffu) / 255.0) - 0.5) * 1.30 * s.HalfUm;
  const En foot = s.FromBox({.U = along, .V = 0.0});
  const double eaves = EavesZ(s);
  const double stack = eaves + roof.HeightAt(foot) + kChimneyOverRidgeM;
  Box(site,
      s,
      foot,
      {.U = kChimneyHalfWide * kChimneyWideM, .V = kChimneyHalfDeep * kChimneyWideM},
      {.LowM = eaves, .HighM = stack},
      Facade::Trim);
}

void RoofPlant(const BuildingShape &s, double deckZ, Site &site) {
  const double halfU = std::min(2.6, 0.30 * s.HalfUm);
  const double halfV = std::min(1.9, 0.30 * s.HalfVm);
  if (halfU < kDormerLeastHalfUm || halfV < kDormerLeastHalfVm) { return; }
  const double along =
      ((static_cast<double>(s.Seed >> 13u & 0xffu) / 255.0) - 0.5) * 0.9 * s.HalfUm;
  const En foot = s.FromBox({.U = along, .V = 0.0});
  Box(site,
      s,
      foot,
      {.U = halfU, .V = halfV},
      {.LowM = deckZ, .HighM = deckZ + kDormerRiseM},
      Facade::Metal);
}

double PlinthTopZ(const BuildingShape &s, const Site2Ground &ground) {
  double lowest = 0.0;
  double highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  const double seatZ = ground.High();
  const double seat = std::max(seatZ, highest) + kPlinthM;

  return seat;
}

constexpr double kRoofRiseM = 3.0;
constexpr double kResolvedPx = 2.0;

constexpr double kArchitectureTris = 262.0;

[[nodiscard]] double FitsInPixelsM(double focalPx, double heightM, double widthM, double tris) {
  if (heightM <= 0.0 || widthM <= 0.0 || tris <= 0.0) { return 0.0; }
  return focalPx * std::sqrt(heightM * widthM / tris);
}

[[nodiscard]] double ArchitectureReachM(double focalPx) {
  return focalPx * kRoofRiseM / kResolvedPx;
}

[[nodiscard]] std::array<En, 4> Hull(std::span<const En> ring) {
  const size_t n = ring.size();
  double bestArea = kBeyondAnyCoordinate;
  double axE = 1.0;
  double axN = 0.0;
  double minU = 0.0;
  double maxU = 0.0;
  double minV = 0.0;
  double maxV = 0.0;
  for (size_t i = 0; i < n; i++) {
    const En &a = ring[i];
    const En &b = ring[(i + 1) % n];
    const double dE = b.EastM - a.EastM;
    const double dN = b.NorthM - a.NorthM;
    const double len = std::hypot(dE, dN);
    if (len < kLeastRunM) { continue; }
    const double uE = dE / len;
    const double uN = dN / len;
    double loU = kBeyondAnyCoordinate;
    double hiU = -kBeyondAnyCoordinate;
    double loV = kBeyondAnyCoordinate;
    double hiV = -kBeyondAnyCoordinate;
    for (const En &p : ring) {
      const double u = p.EastM * uE + p.NorthM * uN;
      const double v = -p.EastM * uN + p.NorthM * uE;
      loU = std::min(loU, u);
      hiU = std::max(hiU, u);
      loV = std::min(loV, v);
      hiV = std::max(hiV, v);
    }
    const double area = (hiU - loU) * (hiV - loV);
    if (area < bestArea) {
      bestArea = area;
      axE = uE;
      axN = uN;
      minU = loU;
      maxU = hiU;
      minV = loV;
      maxV = hiV;
    }
  }
  const auto at = [&](double u, double v) {
    return En{.EastM = u * axE - v * axN, .NorthM = u * axN + v * axE};
  };
  return {at(minU, minV), at(maxU, minV), at(maxU, maxV), at(minU, maxV)};
}

void Box(const BuildingShape &s, std::span<const En> ring, Site &site) {
  const double lowZ = s.SoleM;
  const double topZ = s.TopM();
  const Facade roof = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i < 4; i++) {
    const size_t j = (i + 1) % 4;
    site.Quad(Face(s, ring[i], lowZ, Facade::Wall),
              Face(s, ring[j], lowZ, Facade::Wall),
              Face(s, ring[j], topZ, Facade::Wall),
              Face(s, ring[i], topZ, Facade::Wall));
  }
  site.Quad(Face(s, ring[0], topZ, roof),
            Face(s, ring[1], topZ, roof),
            Face(s, ring[2], topZ, roof),
            Face(s, ring[3], topZ, roof));
  Floor(s, ring, lowZ, site);
}

void RaisePart(const BuildingShape &s, Site &site) {
  const double outM = site.ReachM();
  const double focalPx = site.FocalPx();
  if (focalPx > 0.0) {
    double leastE = kBeyondAnyCoordinate;
    double mostE = -kBeyondAnyCoordinate;
    double leastN = kBeyondAnyCoordinate;
    double mostN = -kBeyondAnyCoordinate;
    for (const En &p : s.Ring) {
      leastE = std::min(leastE, p.EastM);
      mostE = std::max(mostE, p.EastM);
      leastN = std::min(leastN, p.NorthM);
      mostN = std::max(mostN, p.NorthM);
    }
    const double wideM = 0.5 * ((mostE - leastE) + (mostN - leastN));
    const double highM = s.TopM() - s.SoleM;
    const double asDetailed = std::min(ArchitectureReachM(focalPx),
                                       FitsInPixelsM(focalPx, highM, wideM, kArchitectureTris));
    if (site.Coarseness() != Detail::Fine || outM > asDetailed) {
      Box(s, Hull(s.Ring), site);
      return;
    }
  }
  const RoofSurface roof(s);
  BuildingScratch &scratch = site.Scratch();
  const double lowZ = s.OnGround() ? s.SoleM : s.SeatM + s.FootM - kSinkM;
  std::vector<En> &overhang = scratch.Overhang;
  std::vector<En> &crownInner = scratch.CrownInner;
  std::vector<En> &crownOut = scratch.CrownOut;
  RoofSurface::Widened(s.Ring, s.OverhangM, {}, overhang);
  RoofSurface::Widened(s.Ring, -kParapetThickM, {}, crownInner);
  RoofSurface::Widened(s.Ring, kCorniceM, {}, crownOut);
  const bool crowned = s.Roof == RoofKind::Flat && crownInner.size() == s.Ring.size() &&
                       crownOut.size() == s.Ring.size() && s.HalfVm > 2.2 && s.RiseM > 0.0;
  const double flatTopZ = crowned ? EavesZ(s) - 0.34 : EavesZ(s) + s.RiseM;
  const double wallTopZ = s.Roof != RoofKind::Flat ? EavesZ(s) : flatTopZ;
  if (s.OnGround()) {
    Plinth(s, roof, overhang, s.SeatM, site);
    std::vector<En> &proud = scratch.Proud;
    RoofSurface::Widened(s.Ring, kPlinthProudM, {}, proud);
    std::vector<En> &foot = scratch.Foot;
    RefinedLike(s.Ring, overhang, proud, roof, scratch, foot);
    Floor(s, foot.empty() ? std::span<const En>(s.Ring) : std::span<const En>(foot), s.SoleM, site);
  } else {
    Floor(s, s.Ring, lowZ, site);
  }
  Walls(s, roof, overhang, lowZ, wallTopZ, site);

  if (s.Roof == RoofKind::Flat) {
    const double deckZ = crowned ? EavesZ(s) - kSlabM : EavesZ(s) + s.RiseM;
    Covering(s,
             roof,
             crowned ? std::span<const En>(crownInner) : std::span<const En>(s.Ring),
             deckZ,
             site);
    if (crowned) { Crown(s, {.Inner = crownInner, .Out = crownOut}, site); }
    RoofPlant(s, deckZ, site);
    return;
  }

  std::vector<En> &wide = scratch.Wide;
  RoofSurface::Widened(s.Ring, s.OverhangM, {}, wide);
  std::vector<En> &covered = scratch.Covered;
  Refined(s.Ring, wide, roof, true, scratch, covered);
  Covering(s,
           roof,
           covered.empty() ? std::span<const En>(s.Ring) : std::span<const En>(covered),
           EavesZ(s),
           site);
  Gables(s, roof, wide, site);
  if (!wide.empty()) { Eaves(s, roof, wide, site); }
  if (WantsChimney(s)) { Chimney(s, roof, site); }
}

double StandBack(const Frontage &street, const En &p) {
  return (p.EastM - street.KerbEm) * street.ToStreetE +
         (p.NorthM - street.KerbNm) * street.ToStreetN;
}

En OntoKerb(const Frontage &street, const En &p, double back) {
  return {.EastM = p.EastM - back * street.ToStreetE, .NorthM = p.NorthM - back * street.ToStreetN};
}

void Pavement(const BuildingShape &s,
              const Frontage &street,
              const Site2Ground &ground,
              double plinthZ,
              Site &site) {
  if (!street.Known || !s.OnGround()) { return; }
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    if (s.Party[i] != 0u) { continue; }
    const En &p = s.Ring[i];
    const En &q = s.Ring[(i + 1) % n];
    const double e = q.EastM - p.EastM;
    const double nn = q.NorthM - p.NorthM;
    const double len = std::hypot(e, nn);
    if (len < kFrontLeastEdgeM) { continue; }
    if ((nn / len) * street.ToStreetE - (e / len) * street.ToStreetN < kFrontLeastLook) {
      continue;
    }
    double bp = StandBack(street, p);
    double bq = StandBack(street, q);
    if (bp > -kPavementLeastM || bq > -kPavementLeastM) { continue; }
    if (bp < -kPavementMostM && bq < -kPavementMostM) { continue; }
    bp = std::max(bp, -kFootwayMostM);
    bq = std::max(bq, -kFootwayMostM);

    const En pk = OntoKerb(street, p, bp + kKerbTopM);
    const En qk = OntoKerb(street, q, bq + kKerbTopM);
    const En pe = OntoKerb(street, p, bp);
    const En qe = OntoKerb(street, q, bq);
    const auto walk = [&](const En &at) {
      return std::min(ground.At(at) + kKerbUpM, plinthZ - kPlinthClearM);
    };
    const double zp = walk(p);
    const double zq = walk(q);
    const double zpk = walk(pk);
    const double zqk = walk(qk);
    site.Quad(Face(s, p, zp, Facade::Pavement),
              Face(s, pk, zpk, Facade::Pavement),
              Face(s, qk, zqk, Facade::Pavement),
              Face(s, q, zq, Facade::Pavement));
    site.Quad(Face(s, pk, zpk, Facade::Kerb),
              Face(s, pe, zpk, Facade::Kerb),
              Face(s, qe, zqk, Facade::Kerb),
              Face(s, qk, zqk, Facade::Kerb));
    site.Quad(Face(s, pe, zpk - kKerbUpM - kKerbSkirtM, Facade::Kerb),
              Face(s, qe, zqk - kKerbUpM - kKerbSkirtM, Facade::Kerb),
              Face(s, qe, zqk, Facade::Kerb),
              Face(s, pe, zpk, Facade::Kerb));
  }
}

} // namespace

std::unique_ptr<MeshScratch> BuildingMesh::Scratch() const {
  return std::make_unique<BuildingScratch>();
}

bool BuildingMesh::Mesh(const StructurePlan &plan, MeshScratch &lent, Raised &into) const noexcept {
  if (plan.RingLatLon.size() < 6) { return false; }
  auto &scratch = static_cast<BuildingScratch &>(lent);
  try {
    const std::span<BuildingShape> parts = MassOf(plan.RingLatLon,
                                                  {.HeightM = plan.HeightM,
                                                   .HeightMeasured = plan.HeightMeasured,
                                                   .PitchedShare = plan.PitchedShare},
                                                  plan.Street,
                                                  scratch);
    if (parts.empty()) { return false; }

    Site site(plan, scratch, into);
    const Site2Ground ground(
        plan.RingLatLon,
        plan.CornerAslM,
        {.BaseAslM = plan.BaseAslM, .SeatAslM = plan.SeatAslM, .FootAslM = plan.FootAslM});
    for (BuildingShape &part : parts) {
      part.SeatM = PlinthTopZ(part, ground);
      part.SoleM = PlinthFootZ(part, ground);
    }
    for (const BuildingShape &part : parts) {
      RaisePart(part, site);
      Pavement(part, plan.Street, ground, part.SeatM, site);
    }
  } catch (...) {
    into.Clear();
    return false;
  }
  return true;
}
} // namespace outshine::Generators
