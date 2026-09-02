#include <array>
#include <algorithm>
#include <atomic>
#include "Units.h"
#include "RoofSurface.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include <cmath>
#include <vector>
#include <utility>

namespace outshine::Generators {

constexpr double kRidgeInset = 0.85;
constexpr double kSameLineM = 1.0e-3;
constexpr double kHipShare = 0.85;
constexpr double kHipTailShare = 0.15;

namespace {

struct Line {
  double A = 0.0, B = 0.0, C = 0.0;
};

std::atomic<size_t> gBreaksKept{0};
std::atomic<size_t> gBreaksDropped{0};
std::atomic<size_t> gBreaksMerged{0};

constexpr double kSamePointM = 0.01;
constexpr double kWeldM = 0.02;
constexpr double kOnLineM = kWeldM;
constexpr double kOverhangM = 0.60;
constexpr double kCorniceM = 0.16;
constexpr double kSliverM2 = 1.0e-4;
constexpr int kMaxCreases = 14;

void PushTri(std::vector<En> &out, const En &a, const En &b, const En &c) {
  const double ab =
      (a.EastM - b.EastM) * (a.EastM - b.EastM) + (a.NorthM - b.NorthM) * (a.NorthM - b.NorthM);
  const double bc =
      (b.EastM - c.EastM) * (b.EastM - c.EastM) + (b.NorthM - c.NorthM) * (b.NorthM - c.NorthM);
  const double ca =
      (c.EastM - a.EastM) * (c.EastM - a.EastM) + (c.NorthM - a.NorthM) * (c.NorthM - a.NorthM);
  if (ab < kSliverM2 || bc < kSliverM2 || ca < kSliverM2) { return; }
  out.push_back(a);
  out.push_back(b);
  out.push_back(c);
}

[[nodiscard]] bool EarClip(std::span<const En> ring, std::vector<En> &tris) {
  const size_t n = ring.size();
  if (n < 3) { return false; }
  std::vector<uint32_t> poly(n);
  for (size_t i = 0; i < n; i++) { poly[i] = static_cast<uint32_t>(i); }
  const auto cross = [&](uint32_t a, uint32_t b, uint32_t c) {
    return (ring[b].EastM - ring[a].EastM) * (ring[c].NorthM - ring[a].NorthM) -
           (ring[c].EastM - ring[a].EastM) * (ring[b].NorthM - ring[a].NorthM);
  };
  int guard = static_cast<int>(n * n) + 8;
  while (poly.size() > 2 && guard-- > 0) {
    bool cut = false;
    for (size_t i = 0; i < poly.size(); i++) {
      const uint32_t a = poly[(i + poly.size() - 1) % poly.size()];
      const uint32_t b = poly[i];
      const uint32_t c = poly[(i + 1) % poly.size()];
      if (cross(a, b, c) <= 0.0) { continue; }
      bool clean = true;
      for (const uint32_t o : poly) {
        if (o == a || o == b || o == c) { continue; }
        if (cross(a, b, o) >= 0.0 && cross(b, c, o) >= 0.0 && cross(c, a, o) >= 0.0) {
          clean = false;
          break;
        }
      }
      if (!clean) { continue; }
      PushTri(tris, ring[a], ring[b], ring[c]);
      poly.erase(poly.begin() + static_cast<long>(i));
      cut = true;
      break;
    }
    if (!cut) { return false; }
  }
  return poly.size() <= 2;
}

[[nodiscard]] bool Inside(const std::vector<En> &ring, const En &p, double marginM) {
  const size_t n = ring.size();
  if (n < 3) { return true; }
  bool in = false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const bool straddles = (ring[i].NorthM > p.NorthM) != (ring[j].NorthM > p.NorthM);
    if (!straddles) { continue; }
    const double at = (ring[j].EastM - ring[i].EastM) * (p.NorthM - ring[i].NorthM) /
                          (ring[j].NorthM - ring[i].NorthM) +
                      ring[i].EastM;
    if (p.EastM < at) { in = !in; }
  }
  if (in) { return true; }
  double nearest = kBeyondAnyCoordinate;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const double ex = ring[j].EastM - ring[i].EastM;
    const double ny = ring[j].NorthM - ring[i].NorthM;
    const double run = ex * ex + ny * ny;
    double along =
        run > 0.0 ? ((p.EastM - ring[i].EastM) * ex + (p.NorthM - ring[i].NorthM) * ny) / run : 0.0;
    along = std::clamp(along, 0.0, 1.0);
    const double dx = p.EastM - (ring[i].EastM + along * ex);
    const double dy = p.NorthM - (ring[i].NorthM + along * ny);
    nearest = std::min(nearest, dx * dx + dy * dy);
  }
  return nearest <= marginM * marginM;
}

int Deduped(std::span<Line> lines, int n) {
  int kept = 0;
  for (int i = 0; i < n; i++) {
    const double reach = std::hypot(lines[i].A, lines[i].B);
    if (reach < kLeastTurnRad) { continue; }
    const Line unit{.A = lines[i].A / reach, .B = lines[i].B / reach, .C = lines[i].C / reach};
    bool seen = false;
    for (int j = 0; j < kept && !seen; j++) {
      const double same = std::fabs(unit.A - lines[j].A) + std::fabs(unit.B - lines[j].B) +
                          std::fabs(unit.C - lines[j].C);
      const double flipped = std::fabs(unit.A + lines[j].A) + std::fabs(unit.B + lines[j].B) +
                             std::fabs(unit.C + lines[j].C);
      seen = same < kSamePointM || flipped < kSamePointM;
    }
    if (!seen) { lines[kept++] = unit; }
  }
  return kept;
}

int CreasesUncounted(const BuildingShape &s, std::span<Line> lines);

int CreasesOf(const BuildingShape &s, std::span<Line> lines) {
  return Deduped(lines, CreasesUncounted(s, lines));
}

int CreasesUncounted(const BuildingShape &s, std::span<Line> lines) {
  const double d = s.HalfUm - s.HalfVm;
  switch (s.Roof) {
    case RoofKind::Flat:
    case RoofKind::Shed:
    case RoofKind::Dome: return 0;
    case RoofKind::Gable: lines[0] = {.A = 0.0, .B = 1.0, .C = 0.0}; return 1;
    case RoofKind::Hip:
      lines[0] = {.A = 0.0, .B = 1.0, .C = 0.0};
      lines[1] = {.A = 1.0, .B = -1.0, .C = d};
      lines[2] = {.A = 1.0, .B = 1.0, .C = d};
      lines[3] = {.A = 1.0, .B = 1.0, .C = -d};
      lines[4] = {.A = 1.0, .B = -1.0, .C = -d};
      return 5;
    case RoofKind::Mansard:
      lines[0] = {.A = 0.0, .B = 1.0, .C = 0.0};
      lines[1] = {.A = 0.0, .B = 1.0, .C = s.BreakFracV * s.HalfVm};
      lines[2] = {.A = 0.0, .B = 1.0, .C = -s.BreakFracV * s.HalfVm};
      return 3;
    case RoofKind::Sawtooth: {
      int n = 0;
      for (double u = -s.HalfUm; u < s.HalfUm && n + 1 < kMaxCreases; u += s.PeriodM) {
        lines[n++] = {.A = 1.0, .B = 0.0, .C = u};
        lines[n++] = {.A = 1.0, .B = 0.0, .C = u + kRidgeInset * s.PeriodM};
      }
      return n;
    }
  }
  return 0;
}

void Refine(std::vector<En> &tris, int passes) {
  for (int p = 0; p < passes; p++) {
    std::vector<En> out;
    out.reserve(tris.size() * 4);
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
      const En a = tris[i];
      const En b = tris[i + 1];
      const En c = tris[i + 2];
      const En ab{.EastM = 0.5 * (a.EastM + b.EastM), .NorthM = 0.5 * (a.NorthM + b.NorthM)};
      const En bc{.EastM = 0.5 * (b.EastM + c.EastM), .NorthM = 0.5 * (b.NorthM + c.NorthM)};
      const En ca{.EastM = 0.5 * (c.EastM + a.EastM), .NorthM = 0.5 * (c.NorthM + a.NorthM)};
      PushTri(out, a, ab, ca);
      PushTri(out, ab, b, bc);
      PushTri(out, ca, bc, c);
      PushTri(out, ab, bc, ca);
    }
    tris.swap(out);
  }
}

} // namespace

RoofSurface::RoofSurface(const BuildingShape &shape) : Shape_(shape) {}

double RoofSurface::HeightAt(const En &enu) const noexcept {
  double u = 0.0;
  double v = 0.0;
  Shape_.ToBox(enu, &u, &v);
  const double hu = Shape_.HalfUm;
  const double hv = Shape_.HalfVm;
  const double rise = Shape_.RiseM;
  double f = 0.0;
  switch (Shape_.Roof) {
    case RoofKind::Flat: return 0.0;
    case RoofKind::Gable: f = 1.0 - std::fabs(v) / hv; break;
    case RoofKind::Hip: f = std::min(hv - std::fabs(v), hu - std::fabs(u)) / hv; break;
    case RoofKind::Shed: f = 0.5 * (v / hv) + 0.5; break;
    case RoofKind::Mansard: {
      const double b = Shape_.BreakFracV * hv;
      const double a = std::fabs(v);
      const double br = Shape_.BreakRiseM;
      return a >= b ? br * (hv - a) / std::max(hv - b, kSameLineM)
                    : br + (rise - br) * (b - a) / std::max(b, kSameLineM);
    }
    case RoofKind::Sawtooth: {
      const double p = std::max(Shape_.PeriodM, 1.0);
      double x = std::fmod(u + hu, p);
      if (x < 0.0) { x += p; }
      f = std::min(x / (kHipShare * p), (p - x) / (kHipTailShare * p));
      break;
    }
    case RoofKind::Dome: {
      const double r = std::hypot(u / hu, v / hv);
      f = r >= 1.0 ? 0.0 : std::sqrt(1.0 - r * r);
      break;
    }
  }

  return f * rise;
}

bool RoofSurface::Fill(std::span<const En> plan, std::vector<En> &tris) {
  const size_t first = tris.size();
  if (!EarClip(plan, tris)) {
    tris.resize(first);
    RoofSurface::Unclipped_.fetch_add(1u, std::memory_order_relaxed);
    return false;
  }
  return true;
}

namespace {

std::vector<En>
ClipHalf(const BuildingShape &shape, std::span<const En> poly, const Line &line, double sign) {
  std::vector<En> out;
  const size_t n = poly.size();
  out.reserve(n + 2);
  for (size_t i = 0; i < n; i++) {
    const En &a = poly[i];
    const En &b = poly[(i + 1) % n];
    double ua = 0.0;
    double va = 0.0;
    double ub = 0.0;
    double vb = 0.0;
    shape.ToBox(a, &ua, &va);
    shape.ToBox(b, &ub, &vb);
    const double da = (line.A * ua + line.B * va - line.C) * sign;
    const double db = (line.A * ub + line.B * vb - line.C) * sign;
    if (da >= -kOnLineM) { out.push_back(a); }
    if ((da > kOnLineM && db < -kOnLineM) || (da < -kOnLineM && db > kOnLineM)) {
      const double f = da / (da - db);
      En cut{.EastM = a.EastM + (b.EastM - a.EastM) * f,
             .NorthM = a.NorthM + (b.NorthM - a.NorthM) * f};
      if (std::hypot(cut.EastM - a.EastM, cut.NorthM - a.NorthM) < kWeldM) {
        cut = a;
      } else if (std::hypot(cut.EastM - b.EastM, cut.NorthM - b.NorthM) < kWeldM) {
        cut = b;
      }
      out.push_back(cut);
    }
  }
  return out;
}
} // namespace

size_t RoofSurface::BreaksKeptTaken() {
  return gBreaksKept.exchange(0u);
}

size_t RoofSurface::BreaksDroppedTaken() {
  return gBreaksDropped.exchange(0u);
}

size_t RoofSurface::BreaksMergedTaken() {
  return gBreaksMerged.exchange(0u);
}

void RoofSurface::BreaksAlong(const En &from, const En &to, std::vector<double> &at) const {
  at.clear();
  std::array<Line, kMaxCreases> lines{};
  const int n = CreasesOf(Shape_, lines);
  double u0 = 0.0;
  double v0 = 0.0;
  double u1 = 0.0;
  double v1 = 0.0;
  Shape_.ToBox(from, &u0, &v0);
  Shape_.ToBox(to, &u1, &v1);
  for (int i = 0; i < n; i++) {
    const double d0 = lines[i].A * u0 + lines[i].B * v0 - lines[i].C;
    const double d1 = lines[i].A * u1 + lines[i].B * v1 - lines[i].C;
    if ((d0 > kOnLineM && d1 > kOnLineM) || (d0 < -kOnLineM && d1 < -kOnLineM)) { continue; }
    const double span = d0 - d1;
    if (std::fabs(span) < kOnLineM) { continue; }
    const double t = d0 / span;
    const double reach = std::hypot(to.EastM - from.EastM, to.NorthM - from.NorthM);
    const double keepAway = reach > kLeastRunM ? kWeldM / reach : 1.0;
    if (t > keepAway && t < 1.0 - keepAway) {
      at.push_back(t);
      gBreaksKept.fetch_add(1u, std::memory_order_relaxed);
    } else {
      gBreaksDropped.fetch_add(1u, std::memory_order_relaxed);
    }
  }
  std::ranges::sort(at);
  const size_t before = at.size();
  at.erase(std::ranges::unique(at, [](double a, double b) { return std::fabs(a - b) < kSameLineM; })
               .begin(),
           at.end());
  gBreaksMerged.fetch_add(before - at.size(), std::memory_order_relaxed);
}

void RoofSurface::Cover(std::span<const En> plan, std::vector<En> &tris) const {
  const size_t first = tris.size();

  std::array<Line, kMaxCreases> lines{};
  const int n = CreasesOf(Shape_, lines);
  std::vector<std::vector<En>> cells;
  cells.emplace_back(plan.begin(), plan.end());
  for (int i = 0; i < n; i++) {
    std::vector<std::vector<En>> next;
    for (const std::vector<En> &cell : cells) {
      std::vector<En> above = ClipHalf(Shape_, cell, lines[i], 1.0);
      std::vector<En> below = ClipHalf(Shape_, cell, lines[i], -1.0);
      if (above.size() >= 3 && below.size() >= 3) {
        next.push_back(std::move(above));
        next.push_back(std::move(below));
        continue;
      }
      next.push_back(cell);
    }
    cells.swap(next);
  }

  std::vector<En> mine;
  for (const std::vector<En> &cell : cells) {
    if (!EarClip(cell, mine)) {
      tris.resize(first);
      RoofSurface::Unclipped_.fetch_add(1u, std::memory_order_relaxed);
      return;
    }
  }
  if (mine.empty()) { return; }
  if (Shape_.Roof == RoofKind::Dome) { Refine(mine, 4); }
  for (size_t at = 0; at + 2 < mine.size(); at += 3) {
    for (size_t corner = 0; corner < 3; ++corner) {
      const double reach = 4.0 * std::max({Shape_.OverhangM, kCorniceM, kOverhangM});
      if (!Inside(Shape_.Ring, mine[at + corner], reach)) {
        Outside_.fetch_add(1u, std::memory_order_relaxed);
        break;
      }
    }
  }
  tris.insert(tris.end(), mine.begin(), mine.end());
}

std::vector<En>
RoofSurface::Widened(std::span<const En> ring, double byM, std::span<const uint8_t> held) {
  const size_t n = ring.size();
  if (n < 3 || std::fabs(byM) < kSameLineM) { return {}; }
  std::vector<En> out;
  out.reserve(n);
  for (size_t i = 0; i < n; i++) {
    const size_t before = (i + n - 1) % n;
    const En &p = ring[i];
    const En &a = ring[before];
    const En &b = ring[(i + 1) % n];
    const double e0 = p.EastM - a.EastM;
    const double n0 = p.NorthM - a.NorthM;
    const double l0 = std::hypot(e0, n0);
    const double e1 = b.EastM - p.EastM;
    const double n1 = b.NorthM - p.NorthM;
    const double l1 = std::hypot(e1, n1);
    if (l0 < kLeastRunM || l1 < kLeastRunM) { return {}; }

    const double a0 = n0 / l0;
    const double b0 = -e0 / l0;
    const double a1 = n1 / l1;
    const double b1 = -e1 / l1;
    const double d0 = before < held.size() && (held[before] != 0u) ? 0.0 : byM;
    const double d1 = i < held.size() && (held[i] != 0u) ? 0.0 : byM;
    const double det = a0 * b1 - b0 * a1;
    if (std::fabs(det) < kLeastRunM) {
      if (std::fabs(d0 - d1) > kLeastTurnRad) { return {}; }
      out.push_back({.EastM = p.EastM + a0 * d0, .NorthM = p.NorthM + b0 * d0});
      continue;
    }
    const double yE = (d0 * b1 - d1 * b0) / det;
    const double yN = (a0 * d1 - a1 * d0) / det;
    if (std::hypot(yE, yN) > 4.0 * std::fabs(byM)) { return {}; }
    out.push_back({.EastM = p.EastM + yE, .NorthM = p.NorthM + yN});
  }
  return out;
}

} // namespace outshine::Generators
