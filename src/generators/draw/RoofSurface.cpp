#include "RoofSurface.h"

#include <span>

#include <algorithm>
#include <cmath>

namespace outshine::Generators {

namespace {

struct Line {
  double A = 0.0, B = 0.0, C = 0.0;
};

constexpr double kOnLineM = 1.0e-4;
constexpr double kSliverM2 = 1.0e-4;
constexpr int kMaxCreases = 14;

double TriArea(const En &a, const En &b, const En &c) {
  return 0.5 * std::fabs((b.E - a.E) * (c.N - a.N) - (c.E - a.E) * (b.N - a.N));
}

void PushTri(std::vector<En> &out, const En &a, const En &b, const En &c) {
  if (TriArea(a, b, c) < kSliverM2) return;
  out.push_back(a);
  out.push_back(b);
  out.push_back(c);
}

void EarClip(std::span<const En> ring, std::vector<En> &tris) {
  const size_t n = ring.size();
  if (n < 3) return;
  std::vector<uint32_t> poly(n);
  for (size_t i = 0; i < n; i++) poly[i] = (uint32_t)i;
  const auto cross = [&](uint32_t a, uint32_t b, uint32_t c) {
    return (ring[b].E - ring[a].E) * (ring[c].N - ring[a].N) -
           (ring[c].E - ring[a].E) * (ring[b].N - ring[a].N);
  };
  int guard = (int)(n * n) + 8;
  while (poly.size() > 2 && guard-- > 0) {
    bool cut = false;
    for (size_t i = 0; i < poly.size(); i++) {
      const uint32_t a = poly[(i + poly.size() - 1) % poly.size()], b = poly[i],
                     c = poly[(i + 1) % poly.size()];
      if (cross(a, b, c) <= 0.0) continue;
      bool clean = true;
      for (uint32_t o : poly) {
        if (o == a || o == b || o == c) continue;
        if (cross(a, b, o) >= 0.0 && cross(b, c, o) >= 0.0 && cross(c, a, o) >= 0.0) {
          clean = false;
          break;
        }
      }
      if (!clean) continue;
      PushTri(tris, ring[a], ring[b], ring[c]);
      poly.erase(poly.begin() + (long)i);
      cut = true;
      break;
    }
    if (!cut) return;
  }
}

void HalfPlane(const En *t, const double *d, double sign, std::vector<En> &out) {
  En poly[4];
  int n = 0;
  for (int i = 0; i < 3; i++) {
    const int j = (i + 1) % 3;
    const double di = d[i] * sign, dj = d[j] * sign;
    if (di >= -kOnLineM) poly[n++] = t[i];
    if ((di > kOnLineM && dj < -kOnLineM) || (di < -kOnLineM && dj > kOnLineM)) {
      const double f = di / (di - dj);
      poly[n++] = {t[i].E + (t[j].E - t[i].E) * f, t[i].N + (t[j].N - t[i].N) * f};
    }
    if (n >= 4) break;
  }
  for (int i = 2; i < n; i++) PushTri(out, poly[0], poly[i - 1], poly[i]);
}

void SplitByLine(const BuildingShape &shape, const Line &line, std::vector<En> &tris) {
  std::vector<En> out;
  out.reserve(tris.size() * 2);
  for (size_t i = 0; i + 2 < tris.size(); i += 3) {
    const En *t = &tris[i];
    double d[3];
    int pos = 0, neg = 0;
    for (int k = 0; k < 3; k++) {
      double u = 0.0, v = 0.0;
      shape.ToBox(t[k], &u, &v);
      d[k] = line.A * u + line.B * v - line.C;
      if (d[k] > kOnLineM) pos++;
      if (d[k] < -kOnLineM) neg++;
    }
    if (pos == 0 || neg == 0) {
      PushTri(out, t[0], t[1], t[2]);
      continue;
    }
    HalfPlane(t, d, 1.0, out);
    HalfPlane(t, d, -1.0, out);
  }
  tris.swap(out);
}

int CreasesOf(const BuildingShape &s, Line *lines) {
  const double d = s.HalfUm - s.HalfVm;
  switch (s.Roof) {
    case RoofKind::Flat:
    case RoofKind::Shed:
    case RoofKind::Dome:
      return 0;
    case RoofKind::Gable:
      lines[0] = {0.0, 1.0, 0.0};
      return 1;
    case RoofKind::Hip:
      lines[0] = {0.0, 1.0, 0.0};
      lines[1] = {1.0, -1.0, d};
      lines[2] = {1.0, 1.0, d};
      lines[3] = {1.0, 1.0, -d};
      lines[4] = {1.0, -1.0, -d};
      return 5;
    case RoofKind::Mansard:
      lines[0] = {0.0, 1.0, 0.0};
      lines[1] = {0.0, 1.0, s.BreakFracV * s.HalfVm};
      lines[2] = {0.0, 1.0, -s.BreakFracV * s.HalfVm};
      return 3;
    case RoofKind::Sawtooth: {
      int n = 0;
      for (double u = -s.HalfUm; u < s.HalfUm && n + 1 < kMaxCreases; u += s.PeriodM) {
        lines[n++] = {1.0, 0.0, u};
        lines[n++] = {1.0, 0.0, u + 0.85 * s.PeriodM};
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
      const En a = tris[i], b = tris[i + 1], c = tris[i + 2];
      const En ab{0.5 * (a.E + b.E), 0.5 * (a.N + b.N)};
      const En bc{0.5 * (b.E + c.E), 0.5 * (b.N + c.N)};
      const En ca{0.5 * (c.E + a.E), 0.5 * (c.N + a.N)};
      PushTri(out, a, ab, ca);
      PushTri(out, ab, b, bc);
      PushTri(out, ca, bc, c);
      PushTri(out, ab, bc, ca);
    }
    tris.swap(out);
  }
}

}

RoofSurface::RoofSurface(const BuildingShape &shape) : Shape_(shape) {}

double RoofSurface::HeightAt(const En &enu) const noexcept {
  double u = 0.0, v = 0.0;
  Shape_.ToBox(enu, &u, &v);
  const double hu = Shape_.HalfUm, hv = Shape_.HalfVm, rise = Shape_.RiseM;
  double f = 0.0;
  switch (Shape_.Roof) {
    case RoofKind::Flat:
      return 0.0;
    case RoofKind::Gable:
      f = 1.0 - std::fabs(v) / hv;
      break;
    case RoofKind::Hip:
      f = std::min(hv - std::fabs(v), hu - std::fabs(u)) / hv;
      break;
    case RoofKind::Shed:
      f = 0.5 * (v / hv) + 0.5;
      break;
    case RoofKind::Mansard: {
      const double b = Shape_.BreakFracV * hv, a = std::fabs(v), br = Shape_.BreakRiseM;
      return a >= b ? br * (hv - a) / std::max(hv - b, 1.0e-3)
                    : br + (rise - br) * (b - a) / std::max(b, 1.0e-3);
    }
    case RoofKind::Sawtooth: {
      const double p = std::max(Shape_.PeriodM, 1.0);
      double x = std::fmod(u + hu, p);
      if (x < 0.0) x += p;
      f = std::min(x / (0.85 * p), (p - x) / (0.15 * p));
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

void RoofSurface::Cover(std::span<const En> plan, std::vector<En> &tris) const {
  const size_t first = tris.size();
  EarClip(plan, tris);
  if (tris.size() == first) return;
  std::vector<En> mine(tris.begin() + (long)first, tris.end());
  tris.resize(first);

  Line lines[kMaxCreases];
  const int n = CreasesOf(Shape_, lines);
  for (int i = 0; i < n; i++) SplitByLine(Shape_, lines[i], mine);
  if (Shape_.Roof == RoofKind::Dome) Refine(mine, 4);
  tris.insert(tris.end(), mine.begin(), mine.end());
}

std::vector<En> RoofSurface::Widened(std::span<const En> ring, double byM) {
  const size_t n = ring.size();
  if (n < 3 || std::fabs(byM) < 1.0e-3) return {};
  std::vector<En> out;
  out.reserve(n);
  for (size_t i = 0; i < n; i++) {
    const En &p = ring[i];
    const En &a = ring[(i + n - 1) % n];
    const En &b = ring[(i + 1) % n];
    const double e0 = p.E - a.E, n0 = p.N - a.N, l0 = std::hypot(e0, n0);
    const double e1 = b.E - p.E, n1 = b.N - p.N, l1 = std::hypot(e1, n1);
    if (l0 < 1.0e-6 || l1 < 1.0e-6) return {};

    const double ox = (n0 / l0 + n1 / l1), oy = -(e0 / l0 + e1 / l1);
    const double len = std::hypot(ox, oy);
    if (len < 0.35) return {};
    const double miter = byM / (0.5 * len * len);
    if (std::fabs(miter) > 4.0 * std::fabs(byM)) return {};
    out.push_back({p.E + ox * miter, p.N + oy * miter});
  }
  return out;
}

}
