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
std::atomic<size_t> gFootless{0};
std::atomic<size_t> gPlinthSteps{0};
std::atomic<size_t> gFloorRim{0};
std::atomic<size_t> gOverBudget{0};
} // namespace

size_t BuildingMesh::BuriedTaken() {
  return gBuried.exchange(0u);
}

size_t BuildingMesh::DeepestBuriedMmTaken() {
  return gDeepestMm.exchange(0u);
}

size_t BuildingMesh::RaisedTaken() {
  return gRaised.exchange(0u);
}

size_t BuildingMesh::FarthestMTaken() {
  return gFarthestM.exchange(0u);
}

size_t BuildingMesh::BoxesTaken() {
  return gBoxes.exchange(0u);
}

size_t BuildingMesh::UnscaledTaken() {
  return gUnscaled.exchange(0u);
}

size_t BuildingMesh::FootlessTaken() {
  return gFootless.exchange(0u);
}

size_t BuildingMesh::PlinthStepsTaken() {
  return gPlinthSteps.exchange(0u);
}

size_t BuildingMesh::FloorRimTaken() {
  return gFloorRim.exchange(0u);
}

size_t BuildingMesh::OverBudgetTaken() {
  return gOverBudget.exchange(0u);
}

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
  Site(const StructurePlan &plan, Raised &into) : Out_(into) {
    const double lat = plan.RingLatLon[0];
    const double lon = plan.RingLatLon[1];
    double origin[3];
    GeoToEcef(lat, lon, plan.BaseAslM, origin);
    EnuAxesEcef(lat, lon, East_, North_, Up_);
    for (int c = 0; c < 3; c++) { Origin_[c] = origin[c] - plan.AnchorEcef[c]; }
    ReachM_ =
        std::sqrt(Origin_[0] * Origin_[0] + Origin_[1] * Origin_[1] + Origin_[2] * Origin_[2]);
    FocalPx_ = plan.FocalPx;
  }

  [[nodiscard]] double ReachM() const { return ReachM_; }

  [[nodiscard]] double FocalPx() const { return FocalPx_; }

  [[nodiscard]] static Vtx Snapped(const Vtx &v) {
    Vtx out = v;
    out.P.E = std::round(v.P.E * 1000.0) / 1000.0;
    out.P.N = std::round(v.P.N * 1000.0) / 1000.0;
    out.Z = std::round(v.Z * 1000.0) / 1000.0;
    return out;
  }

  [[nodiscard]] uint32_t Index(const Vtx &v) {
    const auto ce = static_cast<int64_t>(std::llround(v.P.E * 1000.0));
    const auto cn = static_cast<int64_t>(std::llround(v.P.N * 1000.0));
    const auto cz = static_cast<int64_t>(std::llround(v.Z * 1000.0));
    const uint64_t key = static_cast<uint64_t>(ce * 73856093LL) ^
                         static_cast<uint64_t>(cn * 19349663LL) ^
                         static_cast<uint64_t>(cz * 83492791LL);
    const auto found = Welded_.find(key);
    if (found != Welded_.end()) { return found->second; }
    const auto made = static_cast<uint32_t>(Welded_.size());
    Welded_.emplace(key, made);
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
    const double e1 = b.P.E - a.P.E;
    const double n1 = b.P.N - a.P.N;
    const double z1 = b.Z - a.Z;
    const double e2 = c.P.E - a.P.E;
    const double n2 = c.P.N - a.P.N;
    const double z2 = c.Z - a.Z;
    double nrm[3] = {n1 * z2 - z1 * n2, z1 * e2 - e1 * z2, e1 * n2 - n1 * e2};
    const double len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (len < 1.0e-12) { return; }
    Face_.push_back(ia);
    Face_.push_back(ib);
    Face_.push_back(ic);
    for (int c2 = 0; c2 < 3; c2++) { nrm[c2] /= len; }
    const int side = nrm[2] > kSteepestRoof ? 1 : 0;
    std::vector<uint32_t> &run = side == 1 ? Out_.RoofRun : Out_.WallRun;
    run.push_back(Corner(side, a, ia, nrm));
    run.push_back(Corner(side, b, ib, nrm));
    run.push_back(Corner(side, c, ic, nrm));
  }

  void Judged(size_t *open, size_t *overused, size_t *reversed) const {
    std::map<std::pair<uint32_t, uint32_t>, int> walked;
    std::map<std::pair<uint32_t, uint32_t>, int> counted;
    for (size_t at = 0; at + 2 < Face_.size(); at += 3) {
      for (int side = 0; side < 3; ++side) {
        const uint32_t from = Face_[at + static_cast<size_t>(side)];
        const uint32_t to = Face_[at + static_cast<size_t>((side + 1) % 3)];
        const bool ahead = from < to;
        const std::pair<uint32_t, uint32_t> key{ahead ? from : to, ahead ? to : from};
        walked[key] += ahead ? 1 : -1;
        counted[key] += 1;
      }
    }
    for (const auto &edge : counted) {
      if (edge.second == 1) {
        ++*open;
      } else if (edge.second > 2) {
        ++*overused;
      } else if (walked.at(edge.first) != 0) {
        ++*reversed;
      }
    }
  }

  void Quad(const Vtx &a, const Vtx &b, const Vtx &c, const Vtx &d) {
    Tri(a, b, c);
    Tri(a, c, d);
  }

private:
  [[nodiscard]] uint32_t Corner(int side, const Vtx &v, uint32_t at, const double nrm[3]) {
    std::vector<float> &soup = side == 1 ? Out_.RoofCorners : Out_.WallCorners;
    const uint64_t facing =
        (static_cast<uint64_t>(
             static_cast<uint32_t>(static_cast<int32_t>(std::llround(nrm[0] * 4096.0))))
         << 42) ^
        (static_cast<uint64_t>(
             static_cast<uint32_t>(static_cast<int32_t>(std::llround(nrm[1] * 4096.0))))
         << 21) ^
        static_cast<uint64_t>(
            static_cast<uint32_t>(static_cast<int32_t>(std::llround(nrm[2] * 4096.0))));
    const uint64_t key = (static_cast<uint64_t>(at) * 1099511628211ull) ^ facing;
    const auto found = Corners_[side].find(key);
    if (found != Corners_[side].end()) { return found->second; }
    const auto made = static_cast<uint32_t>(soup.size() / 8u);
    Corners_[side].emplace(key, made);
    for (int c = 0; c < 3; c++) {
      soup.push_back(
          static_cast<float>(Origin_[c] + v.P.E * East_[c] + v.P.N * North_[c] + v.Z * Up_[c]));
    }
    soup.push_back(v.U);
    soup.push_back(v.V);
    for (int c = 0; c < 3; c++) {
      soup.push_back(static_cast<float>(nrm[0] * East_[c] + nrm[1] * North_[c] + nrm[2] * Up_[c]));
    }
    return made;
  }

  Raised &Out_;
  std::unordered_map<uint64_t, uint32_t> Welded_;
  std::unordered_map<uint64_t, uint32_t> Corners_[2];
  std::vector<uint32_t> Face_;
  double Origin_[3], East_[3], North_[3], Up_[3];
  double ReachM_ = 0.0;
  double FocalPx_ = 0.0;
};

class Site2Ground {
public:
  Site2Ground(Span<const double> ringLatLon,
              Span<const double> cornerAslM,
              double baseAslM,
              double seatAslM,
              double footAslM)
      : HighM_(seatAslM - baseAslM), LowM_(footAslM - baseAslM) {
    const size_t n = std::min(cornerAslM.Size(), ringLatLon.Size() / 2);
    if (n < 3) { return; }
    double m[3][4] = {};
    for (size_t k = 0; k < n; k++) {
      double e = 0.0;
      double nn = 0.0;
      EnuOffsetM(ringLatLon[0], ringLatLon[1], ringLatLon[k * 2], ringLatLon[k * 2 + 1], e, nn);
      const double z = cornerAslM[k] - baseAslM;
      const double b[3] = {1.0, e, nn};
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
      if (std::fabs(m[piv][c]) < 1.0e-6) { return; }
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

  [[nodiscard]] double At(const En &p) const { return Const_ + SlopeE_ * p.E + SlopeN_ * p.N; }

  [[nodiscard]] double High() const { return HighM_; }

  [[nodiscard]] double Low() const { return LowM_; }

private:
  double Const_ = 0.0, SlopeE_ = 0.0, SlopeN_ = 0.0;
  double HighM_ = 0.0, LowM_ = 0.0;
};

double EdgeLength(const En &p, const En &q) {
  return std::hypot(q.E - p.E, q.N - p.N);
}

En Along(const En &p, const En &q, double t) {
  return {.E = p.E + (q.E - p.E) * t, .N = p.N + (q.N - p.N) * t};
}

double BaysOn(double lengthM, double bayM) {
  if (lengthM < 1.9) { return 0.0; }
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

void BreaksBoth(const RoofSurface &roof,
                const En &p,
                const En &q,
                const En &wp,
                const En &wq,
                bool overhung,
                std::vector<double> &at) {
  std::vector<double> other;
  roof.BreaksAlong(p, q, at);
  if (overhung) {
    roof.BreaksAlong(wp, wq, other);
    at.insert(at.end(), other.begin(), other.end());
    std::sort(at.begin(), at.end());
    at.erase(std::unique(at.begin(),
                         at.end(),
                         [](double a, double b) { return std::fabs(a - b) < 1.0e-3; }),
             at.end());
  }
}

std::vector<En> RefinedLike(std::span<const En> along,
                            const std::vector<En> &wide,
                            std::span<const En> emit,
                            const RoofSurface &roof) {
  const size_t n = along.size();
  const bool overhung = wide.size() == n;
  std::vector<En> out;
  std::vector<double> at;
  if (emit.size() != n) { return out; }
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof,
               along[i],
               along[j],
               overhung ? wide[i] : along[i],
               overhung ? wide[j] : along[j],
               overhung,
               at);
    out.push_back(emit[i]);
    for (const double t : at) { out.push_back(Along(emit[i], emit[j], t)); }
  }
  return out;
}

std::vector<En> Refined(std::span<const En> ring,
                        const std::vector<En> &wide,
                        const RoofSurface &roof,
                        bool takeWide) {
  const size_t n = ring.size();
  const bool overhung = wide.size() == n;
  std::vector<En> out;
  std::vector<double> at;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof,
               ring[i],
               ring[j],
               overhung ? wide[i] : ring[i],
               overhung ? wide[j] : ring[j],
               overhung,
               at);
    const En &from = takeWide && overhung ? wide[i] : ring[i];
    const En &to = takeWide && overhung ? wide[j] : ring[j];
    out.push_back(from);
    for (const double t : at) { out.push_back(Along(from, to, t)); }
  }
  return out;
}

void Walls(const BuildingShape &s,
           const RoofSurface &roof,
           const std::vector<En> &wide,
           double lowZ,
           double topZ,
           Site &site) {
  const size_t n = s.Ring.size();
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i];
    const En &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < 0.05) { continue; }
    const double bays = (s.Party[i] != 0u) ? 0.0 : BaysOn(len, s.BayM);
    if (static_cast<int>(i) == s.FrontEdge && bays >= 2.0) {
      FrontWall(s, p, q, bays, lowZ, topZ, site);
      continue;
    }
    const bool overhung = wide.size() == n;
    BreaksBoth(
        roof, p, q, overhung ? wide[i] : p, overhung ? wide[(i + 1) % n] : q, overhung, breaks);
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
                static_cast<int>(i) == s.FrontEdge ? Fields::Entrance : Fields::Back,
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
      if (at < *lowest) { *lowest = at; }
      if (at > *highest) { *highest = at; }
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
            const std::vector<En> &wide,
            double topZ,
            Site &site) {
  const std::vector<En> out = RoofSurface::Widened(s.Ring, kPlinthProudM);
  if (out.size() != s.Ring.size()) { return; }
  const size_t n = s.Ring.size();
  const double lowZ = s.SoleM;
  const bool overhung = wide.size() == n;
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof,
               s.Ring[i],
               s.Ring[j],
               overhung ? wide[i] : s.Ring[i],
               overhung ? wide[j] : s.Ring[j],
               overhung,
               breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En oa = Along(out[i], out[j], was);
      const En ob = Along(out[i], out[j], now);
      const En ra = Along(s.Ring[i], s.Ring[j], was);
      const En rb = Along(s.Ring[i], s.Ring[j], now);
      was = now;
      gPlinthSteps.fetch_add(1u, std::memory_order_relaxed);
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

void Floor(const BuildingShape &s, const std::vector<En> &ring, double atZ, Site &site) {
  std::vector<En> tris;
  (void)RoofSurface::Fill(ring, tris);
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    site.Tri(Face(s, tris[i + 2], atZ, Facade::Plinth),
             Face(s, tris[i + 1], atZ, Facade::Plinth),
             Face(s, tris[i], atZ, Facade::Plinth));
  }
}

void Gables(const BuildingShape &s,
            const RoofSurface &roof,
            const std::vector<En> &wide,
            Site &site) {
  const size_t n = s.Ring.size();
  const double eaves = EavesZ(s);
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const En &p = s.Ring[i];
    const En &q = s.Ring[(i + 1) % n];
    const double len = EdgeLength(p, q);
    if (len < 0.05) { continue; }
    const double bays = (s.Party[i] != 0u) ? 0.0 : BaysOn(len, s.BayM);
    const bool overhung = wide.size() == n;
    BreaksBoth(
        roof, p, q, overhung ? wide[i] : p, overhung ? wide[(i + 1) % n] : q, overhung, breaks);
    double was = 0.0;
    for (size_t step = 0; step <= breaks.size(); ++step) {
      const double now = step < breaks.size() ? breaks[step] : 1.0;
      const En a = Along(p, q, was);
      const En b = Along(p, q, now);
      const double ha = std::max(roof.HeightAt(a), 0.0);
      const double hb = std::max(roof.HeightAt(b), 0.0);
      was = now;
      if (ha < 0.03 && hb < 0.03) { continue; }
      site.Quad(Wall(s, a, eaves, 0.0, Fields::Back),
                Wall(s, b, eaves, bays, Fields::Back),
                Wall(s, b, eaves + hb, bays, Fields::Back),
                Wall(s, a, eaves + ha, 0.0, Fields::Back));
    }
  }
}

void Covering(const BuildingShape &s,
              const RoofSurface &roof,
              const std::vector<En> &plan,
              double deckZ,
              Site &site) {
  std::vector<En> tris;
  roof.Cover(plan, tris);
  const Facade kind = s.Roof == RoofKind::Flat ? Facade::RoofFlat : Facade::RoofPitch;
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    Vtx v[3];
    for (int k = 0; k < 3; k++) {
      v[k] = Face(s,
                  tris[i + static_cast<size_t>(k)],
                  deckZ + roof.HeightAt(tris[i + static_cast<size_t>(k)]) + kSlabM,
                  kind);
    }
    site.Tri(v[0], v[1], v[2]);
  }
}

void Eaves(const BuildingShape &s,
           const RoofSurface &roof,
           const std::vector<En> &wide,
           Site &site) {
  const size_t n = s.Ring.size();
  if (wide.size() != n) { return; }
  const double eaves = EavesZ(s);
  std::vector<double> breaks;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    BreaksBoth(roof, s.Ring[i], s.Ring[j], wide[i], wide[j], true, breaks);
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

void Crown(const BuildingShape &s,
           const std::vector<En> &inner,
           const std::vector<En> &out,
           Site &site) {
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

void Box(Site &site,
         const BuildingShape &s,
         const En &centre,
         double halfU,
         double halfV,
         double lowZ,
         double highZ,
         Facade side) {
  En c[4];
  const En u{.E = s.AxisU.E * halfU, .N = s.AxisU.N * halfU};
  const En v{.E = -s.AxisU.N * halfV, .N = s.AxisU.E * halfV};
  c[0] = {.E = centre.E - u.E - v.E, .N = centre.N - u.N - v.N};
  c[1] = {.E = centre.E + u.E - v.E, .N = centre.N + u.N - v.N};
  c[2] = {.E = centre.E + u.E + v.E, .N = centre.N + u.N + v.N};
  c[3] = {.E = centre.E - u.E + v.E, .N = centre.N - u.N + v.N};
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
  const double along = ((static_cast<double>(s.Seed >> 9 & 0xffu) / 255.0) - 0.5) * 1.30 * s.HalfUm;
  const En foot = s.FromBox(along, 0.0);
  const double eaves = EavesZ(s);
  const double stack = eaves + roof.HeightAt(foot) + kChimneyOverRidgeM;
  Box(site, s, foot, 0.5 * kChimneyWideM, 0.4 * kChimneyWideM, eaves, stack, Facade::Trim);
}

void RoofPlant(const BuildingShape &s, double deckZ, Site &site) {
  const double halfU = std::min(2.6, 0.30 * s.HalfUm);
  const double halfV = std::min(1.9, 0.30 * s.HalfVm);
  if (halfU < 0.9 || halfV < 0.7) { return; }
  const double along = ((static_cast<double>(s.Seed >> 13 & 0xffu) / 255.0) - 0.5) * 0.9 * s.HalfUm;
  const En foot = s.FromBox(along, 0.0);
  Box(site, s, foot, halfU, halfV, deckZ, deckZ + 2.1, Facade::Metal);
}

double PlinthTopZ(const BuildingShape &s, const Site2Ground &ground) {
  double lowest = 0.0;
  double highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  const double seatZ = ground.High();
  const double seat = std::max(seatZ, highest) + kPlinthM;

  const double deepest = seatZ - seat;
  if (deepest > 0.0) {
    gBuried.fetch_add(1u, std::memory_order_relaxed);
    const auto mm = static_cast<size_t>(deepest * 1000.0);
    size_t was = gDeepestMm.load(std::memory_order_relaxed);
    while (mm > was && !gDeepestMm.compare_exchange_weak(was, mm)) {}
  }
  return seat;
}

constexpr double kRoofRiseM = 3.0;
constexpr double kResolvedPx = 2.0;

constexpr double kArchitectureTris = 262.0;
constexpr double kBoxTris = 12.0;

[[nodiscard]] double FitsInPixelsM(double focalPx, double heightM, double widthM, double tris) {
  if (heightM <= 0.0 || widthM <= 0.0 || tris <= 0.0) { return 0.0; }
  return focalPx * std::sqrt(heightM * widthM / tris);
}

[[nodiscard]] double ArchitectureReachM(double focalPx) {
  return focalPx * kRoofRiseM / kResolvedPx;
}

[[nodiscard]] std::vector<En> Hull(const std::vector<En> &ring) {
  const size_t n = ring.size();
  double bestArea = 1.0e300;
  double axE = 1.0;
  double axN = 0.0;
  double minU = 0.0;
  double maxU = 0.0;
  double minV = 0.0;
  double maxV = 0.0;
  for (size_t i = 0; i < n; i++) {
    const En &a = ring[i];
    const En &b = ring[(i + 1) % n];
    const double dE = b.E - a.E;
    const double dN = b.N - a.N;
    const double len = std::hypot(dE, dN);
    if (len < 1.0e-6) { continue; }
    const double uE = dE / len;
    const double uN = dN / len;
    double loU = 1.0e300;
    double hiU = -1.0e300;
    double loV = 1.0e300;
    double hiV = -1.0e300;
    for (const En &p : ring) {
      const double u = p.E * uE + p.N * uN;
      const double v = -p.E * uN + p.N * uE;
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
    return En{.E = u * axE - v * axN, .N = u * axN + v * axE};
  };
  return {at(minU, minV), at(maxU, minV), at(maxU, maxV), at(minU, maxV)};
}

void Box(const BuildingShape &s, const std::vector<En> &ring, Site &site) {
  const double lowZ = s.SoleM;
  const double topZ = s.TopM();
  for (size_t i = 0; i < 4; i++) {
    const size_t j = (i + 1) % 4;
    site.Quad(Face(s, ring[i], lowZ, Facade::Wall),
              Face(s, ring[j], lowZ, Facade::Wall),
              Face(s, ring[j], topZ, Facade::Wall),
              Face(s, ring[i], topZ, Facade::Wall));
  }
  site.Quad(Face(s, ring[0], topZ, Facade::RoofFlat),
            Face(s, ring[1], topZ, Facade::RoofFlat),
            Face(s, ring[2], topZ, Facade::RoofFlat),
            Face(s, ring[3], topZ, Facade::RoofFlat));
  Floor(s, ring, lowZ, site);
}

void RaisePart(const BuildingShape &s, Site &site) {
  const double outM = site.ReachM();
  const auto whole = static_cast<size_t>(outM);
  for (size_t seen = gFarthestM.load(); whole > seen;) {
    if (gFarthestM.compare_exchange_weak(seen, whole)) { break; }
  }
  const double focalPx = site.FocalPx();
  if (focalPx > 0.0) {
    double leastE = 1.0e300;
    double mostE = -1.0e300;
    double leastN = 1.0e300;
    double mostN = -1.0e300;
    for (const En &p : s.Ring) {
      leastE = std::min(leastE, p.E);
      mostE = std::max(mostE, p.E);
      leastN = std::min(leastN, p.N);
      mostN = std::max(mostN, p.N);
    }
    const double wideM = 0.5 * ((mostE - leastE) + (mostN - leastN));
    const double highM = s.TopM() - s.SoleM;
    const double asDetailed = std::min(ArchitectureReachM(focalPx),
                                       FitsInPixelsM(focalPx, highM, wideM, kArchitectureTris));
    if (outM > FitsInPixelsM(focalPx, highM, wideM, kBoxTris)) {
      gOverBudget.fetch_add(1u, std::memory_order_relaxed);
    }
    if (outM > asDetailed) {
      gBoxes.fetch_add(1u, std::memory_order_relaxed);
      Box(s, Hull(s.Ring), site);
      return;
    }
  } else {
    gUnscaled.fetch_add(1u, std::memory_order_relaxed);
  }
  gRaised.fetch_add(1u, std::memory_order_relaxed);
  const RoofSurface roof(s);
  const double lowZ = s.OnGround() ? s.SoleM : s.SeatM + s.FootM - kSinkM;
  const std::vector<En> overhang = RoofSurface::Widened(s.Ring, s.OverhangM);
  const std::vector<En> crownInner = RoofSurface::Widened(s.Ring, -kParapetThickM);
  const std::vector<En> crownOut = RoofSurface::Widened(s.Ring, kCorniceM);
  const bool crowned = s.Roof == RoofKind::Flat && crownInner.size() == s.Ring.size() &&
                       crownOut.size() == s.Ring.size() && s.HalfVm > 2.2 && s.RiseM > 0.0;
  const double wallTopZ = s.Roof != RoofKind::Flat ? EavesZ(s)
                          : crowned                ? EavesZ(s) - 0.34
                                                   : EavesZ(s) + s.RiseM;
  if (s.OnGround()) {
    Plinth(s, roof, overhang, s.SeatM, site);
    const std::vector<En> proud = RoofSurface::Widened(s.Ring, kPlinthProudM);
    const std::vector<En> foot = RefinedLike(s.Ring, overhang, proud, roof);
    if (foot.empty()) { gFootless.fetch_add(1u, std::memory_order_relaxed); }
    gFloorRim.fetch_add(foot.empty() ? s.Ring.size() : foot.size(), std::memory_order_relaxed);
    Floor(s, foot.empty() ? s.Ring : foot, s.SoleM, site);
  } else {
    Floor(s, s.Ring, lowZ, site);
  }
  Walls(s, roof, overhang, lowZ, wallTopZ, site);

  if (s.Roof == RoofKind::Flat) {
    const double deckZ = crowned ? EavesZ(s) - kSlabM : EavesZ(s) + s.RiseM;
    Covering(s, roof, crowned ? crownInner : s.Ring, deckZ, site);
    if (crowned) { Crown(s, crownInner, crownOut, site); }
    RoofPlant(s, deckZ, site);
    return;
  }

  const std::vector<En> wide = RoofSurface::Widened(s.Ring, s.OverhangM);
  const std::vector<En> covered = Refined(s.Ring, wide, roof, true);
  Covering(s, roof, covered.empty() ? s.Ring : covered, EavesZ(s), site);
  Gables(s, roof, wide, site);
  if (!wide.empty()) { Eaves(s, roof, wide, site); }
  if (WantsChimney(s)) { Chimney(s, roof, site); }
}

double StandBack(const Frontage &street, const En &p) {
  return (p.E - street.KerbEm) * street.ToStreetE + (p.N - street.KerbNm) * street.ToStreetN;
}

En OntoKerb(const Frontage &street, const En &p, double back) {
  return {.E = p.E - back * street.ToStreetE, .N = p.N - back * street.ToStreetN};
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
    const double e = q.E - p.E;
    const double nn = q.N - p.N;
    const double len = std::hypot(e, nn);
    if (len < 1.2) { continue; }
    if ((nn / len) * street.ToStreetE - (e / len) * street.ToStreetN < 0.35) { continue; }
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
      return std::min(ground.At(at) + kKerbUpM, plinthZ - 0.05);
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

void BuildingMesh::Mesh(const StructurePlan &plan, Raised &into) const noexcept {
  if (plan.RingLatLon.Size() < 6 || (plan.AnchorEcef == nullptr)) { return; }
  Massing mass = MassOf(plan.RingLatLon, plan.HeightM, plan.HeightMeasured, plan.Street);
  if (mass.Parts.empty()) { return; }

  Site site(plan, into);
  const Site2Ground ground(
      plan.RingLatLon, plan.CornerAslM, plan.BaseAslM, plan.SeatAslM, plan.FootAslM);
  for (BuildingShape &part : mass.Parts) {
    part.SeatM = PlinthTopZ(part, ground);
    part.SoleM = PlinthFootZ(part, ground);
  }
  for (const BuildingShape &part : mass.Parts) {
    RaisePart(part, site);
    Pavement(part, plan.Street, ground, part.SeatM, site);
  }
}

} // namespace outshine::Generators
