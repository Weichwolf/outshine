#include <atomic>
#include "RoofSurface.h"

#include <span>

#include <algorithm>
#include <cmath>

namespace outshine::Generators {

namespace {

struct Line {
  double A = 0.0, B = 0.0, C = 0.0;
};

// A CREASE'S TOLERANCE IS THE WELD'S. At a tenth of a millimetre a split point landing just beside a
// corner still counts as a crossing, and the piece it cuts off is a sliver that `PushTri` then drops
// -- so the tidying and the splitting fight, and the surface ends up with a hole where a crease
// grazed a corner. A centimetre is what positions are welded at, so below it two points ARE one
// point and there is nothing to cut.
// ONE TOLERANCE, USED BY EVERYTHING THAT ASKS "IS THIS THE SAME POINT". It was three different
// numbers -- a millimetre where positions are snapped, a centimetre for a crease, two centimetres
// for a break -- so a point could be the same point to one step of the pipeline and a different one
// to the next, and a seam opened exactly there. A crease crossing dropped because it lay within
// 2 cm of a corner still got cut at 1 cm by the clipper, and the piece between the two answers had
// no partner.
std::atomic<size_t> gBreaksKept{0};
std::atomic<size_t> gBreaksDropped{0};
std::atomic<size_t> gBreaksMerged{0};

constexpr double kSamePointM = 0.01;
// AND THIS ONE IS NOT THE SAME NUMBER, measured rather than assumed. Setting it to kSamePointM --
// which is what tidiness wants -- took the tower from 14 holes to 8 and the SPIRE from 0 to 14. Two
// tolerances that ought to be one are being played against each other here, and matching them by
// hand is fitting rather than engineering. board:1949 is the answer: one subdivision decided ONCE
// and shared, instead of several approximations tuned to agree.
constexpr double kWeldM = 0.02;
constexpr double kOnLineM = kWeldM;
constexpr double kOverhangM = 0.60;
constexpr double kCorniceM = 0.16;
constexpr double kSliverM2 = 1.0e-4;
constexpr int kMaxCreases = 14;

// A TRIANGLE GOES ONLY IF IT IS DEGENERATE, not merely thin. Refusing on AREA drops pieces a split
// legitimately produced, and a piece missing from a split is a HOLE -- the tower's roof was open over
// seven metres because of it. Two corners within the weld tolerance ARE one corner, so such a
// triangle carries no edge anything else can pair with and its absence costs nothing. A thin
// triangle has a poor normal, which is a shading question and a smaller one than a hole.
//
// This was tried once BEFORE the crease tolerance was fixed and made things worse -- 62 holes to 96
// across the nine cases -- because back then the splits were still manufacturing slivers faster than
// keeping them could help. Order mattered.
void PushTri(std::vector<En> &out, const En &a, const En &b, const En &c) {
  const double ab = (a.E - b.E) * (a.E - b.E) + (a.N - b.N) * (a.N - b.N);
  const double bc = (b.E - c.E) * (b.E - c.E) + (b.N - c.N) * (b.N - c.N);
  const double ca = (c.E - a.E) * (c.E - a.E) + (c.N - a.N) * (c.N - a.N);
  if (ab < kSliverM2 || bc < kSliverM2 || ca < kSliverM2) { return; }
  out.push_back(a);
  out.push_back(b);
  out.push_back(c);
}

// A CLIPPER THAT CANNOT FINISH SAYS SO. It used to `return` on failing to find an ear and leave the
// polygon HALF triangulated -- some ears cut, the rest of the surface simply absent, nothing counted
// and nothing said. That is the open roof faces the owner saw. Unreal fails an asset's build when a
// polygon will not triangulate and RAGE refuses at export; neither hands back half a surface.
[[nodiscard]] bool EarClip(std::span<const En> ring, std::vector<En> &tris) {
  const size_t n = ring.size();
  if (n < 3) { return false; }
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
    if (!cut) { return false; }
  }
  return poly.size() <= 2;
}

// CROSSING NUMBER, with a margin. A cornice overhangs by design, so a vertex is outside only when it
// clears the ring by more than that.
[[nodiscard]] bool Inside(const std::vector<En> &ring, const En &p, double marginM) {
  const size_t n = ring.size();
  if (n < 3) { return true; }
  bool in = false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const bool straddles = (ring[i].N > p.N) != (ring[j].N > p.N);
    if (!straddles) { continue; }
    const double at = (ring[j].E - ring[i].E) * (p.N - ring[i].N) / (ring[j].N - ring[i].N) +
                      ring[i].E;
    if (p.E < at) { in = !in; }
  }
  if (in) { return true; }
  double nearest = 1.0e30;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const double ex = ring[j].E - ring[i].E, ny = ring[j].N - ring[i].N;
    const double run = ex * ex + ny * ny;
    double along = run > 0.0 ? ((p.E - ring[i].E) * ex + (p.N - ring[i].N) * ny) / run : 0.0;
    along = along < 0.0 ? 0.0 : (along > 1.0 ? 1.0 : along);
    const double dx = p.E - (ring[i].E + along * ex), dy = p.N - (ring[i].N + along * ny);
    nearest = std::min(nearest, dx * dx + dy * dy);
  }
  return nearest <= marginM * marginM;
}


// A CREASE STATED TWICE IS NOT TWO CREASES. A hip's four diagonals are `{1, -1, +-d}` and
// `{1, 1, +-d}` where `d` is the footprint's own asymmetry -- so on a SQUARE plan `d` is zero and
// each pair collapses onto one line. Splitting a triangulation twice along the same line hands the
// second pass triangles that already have a vertex exactly on it, and the half-plane clip then
// drops pieces: the tower and the spire were open over their whole roof and closed outright with the
// diagonals removed, which is how this was found.
int Deduped(Line *lines, int n) {
  int kept = 0;
  for (int i = 0; i < n; i++) {
    const double reach = std::hypot(lines[i].A, lines[i].B);
    if (reach < 1.0e-9) { continue; }
    const Line unit{lines[i].A / reach, lines[i].B / reach, lines[i].C / reach};
    bool seen = false;
    for (int j = 0; j < kept && !seen; j++) {
      // THE SAME TOLERANCE AS EVERYTHING ELSE THAT ASKS "IS THIS THE SAME PLACE". It was a
      // MICROMETRE, and a hip's diagonals are `{1, +-1, +-d}` where `d` is the footprint's own
      // asymmetry -- so on a plan that is square to within a fraction of a millimetre, which is what
      // an 11 m box becomes after a trip through latitude and longitude, each pair is two DISTINCT
      // lines a millimetre apart instead of one. Cutting a cell twice a millimetre apart leaves a
      // sliver between them, and a sliver between two cuts is a hole. Below the weld tolerance two
      // lines ARE one line, for the same reason two points are one point.
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

int CreasesUncounted(const BuildingShape &s, Line *lines);

int CreasesOf(const BuildingShape &s, Line *lines) {
  return Deduped(lines, CreasesUncounted(s, lines));
}

int CreasesUncounted(const BuildingShape &s, Line *lines) {
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

bool RoofSurface::Fill(std::span<const En> plan, std::vector<En> &tris) {
  const size_t first = tris.size();
  if (!EarClip(plan, tris)) {
    tris.resize(first);
    RoofSurface::Unclipped_.fetch_add(1u, std::memory_order_relaxed);
    return false;
  }
  return true;
}

// CUT THE POLYGON, THEN TRIANGULATE. Splitting an already-triangulated surface is a REPAIR: each
// triangle is clipped on its own and the pieces have to agree afterwards, which they do until two
// creases stack and one of them grazes a corner. Cutting the POLYGON first makes the crease a cell
// BOUNDARY -- the two cells either side of it are built from the same two endpoints, so they cannot
// disagree about where it runs. Every crease that opened a roof in this tree opened it at a stacked
// split, and there are no stacked splits here: a cell is cut, and its halves are cut again, and
// nothing is ever triangulated until the arrangement is finished.
//
// Sutherland-Hodgman, which is exact against a half plane. Its limit, stated where it is used: on a
// CONCAVE cell it can lay a zero-width bridge across a notch. A footprint is concave often enough
// that this has to be measured rather than assumed, and the measurement is Rothenburg's hole count.
std::vector<En> ClipHalf(const BuildingShape &shape, std::span<const En> poly, const Line &line,
                         double sign) {
  std::vector<En> out;
  const size_t n = poly.size();
  out.reserve(n + 2);
  for (size_t i = 0; i < n; i++) {
    const En &a = poly[i], &b = poly[(i + 1) % n];
    double ua = 0.0, va = 0.0, ub = 0.0, vb = 0.0;
    shape.ToBox(a, &ua, &va);
    shape.ToBox(b, &ub, &vb);
    const double da = (line.A * ua + line.B * va - line.C) * sign;
    const double db = (line.A * ub + line.B * vb - line.C) * sign;
    if (da >= -kOnLineM) { out.push_back(a); }
    if ((da > kOnLineM && db < -kOnLineM) || (da < -kOnLineM && db > kOnLineM)) {
      const double f = da / (da - db);
      En cut{a.E + (b.E - a.E) * f, a.N + (b.N - a.N) * f};
      // THE SNAP USES THE SAME NUMBER AS `BreaksAlong`, not the on-line epsilon. A crossing within
      // `kWeldM` of a corner is DROPPED there -- so if it is merely snapped here at a smaller
      // tolerance, the covering gains a vertex the soffit and trim beside it never got, and the seam
      // between them is one vertex out. Two thresholds answering the same question is how the hip's
      // eaves opened three centimetres above the trim.
      if (std::hypot(cut.E - a.E, cut.N - a.N) < kWeldM) { cut = a; }
      else if (std::hypot(cut.E - b.E, cut.N - b.N) < kWeldM) { cut = b; }
      out.push_back(cut);
    }
  }
  return out;
}

size_t RoofSurface::BreaksKeptTaken() { return gBreaksKept.exchange(0u); }
size_t RoofSurface::BreaksDroppedTaken() { return gBreaksDropped.exchange(0u); }
size_t RoofSurface::BreaksMergedTaken() { return gBreaksMerged.exchange(0u); }

void RoofSurface::BreaksAlong(const En &from, const En &to, std::vector<double> &at) const {
  at.clear();
  Line lines[kMaxCreases];
  const int n = CreasesOf(Shape_, lines);
  double u0 = 0.0, v0 = 0.0, u1 = 0.0, v1 = 0.0;
  Shape_.ToBox(from, &u0, &v0);
  Shape_.ToBox(to, &u1, &v1);
  for (int i = 0; i < n; i++) {
    const double d0 = lines[i].A * u0 + lines[i].B * v0 - lines[i].C;
    const double d1 = lines[i].A * u1 + lines[i].B * v1 - lines[i].C;
    if ((d0 > kOnLineM && d1 > kOnLineM) || (d0 < -kOnLineM && d1 < -kOnLineM)) { continue; }
    const double span = d0 - d1;
    if (std::fabs(span) < kOnLineM) { continue; }
    const double t = d0 / span;
    // A BREAK IS DROPPED IF IT LANDS WITHIN A WELD OF EITHER END. The tolerance is in METRES, not in
    // the parameter: a thousandth of a 0.1 m edge is a tenth of a millimetre, and two corners that
    // close together are ONE corner once positions are welded on a centimetre grid -- so the split
    // buys a triangle with two corners in one place instead of a seam.
    const double reach = std::hypot(to.E - from.E, to.N - from.N);
    const double keepAway = reach > 1.0e-6 ? kWeldM / reach : 1.0;
    if (t > keepAway && t < 1.0 - keepAway) {
      at.push_back(t);
      gBreaksKept.fetch_add(1u, std::memory_order_relaxed);
    } else {
      gBreaksDropped.fetch_add(1u, std::memory_order_relaxed);
    }
  }
  std::sort(at.begin(), at.end());
  const size_t before = at.size();
  at.erase(std::unique(at.begin(), at.end(),
                       [](double a, double b) { return std::fabs(a - b) < 1.0e-3; }),
           at.end());
  gBreaksMerged.fetch_add(before - at.size(), std::memory_order_relaxed);
}

void RoofSurface::Cover(std::span<const En> plan, std::vector<En> &tris) const {
  const size_t first = tris.size();

  // THE ARRANGEMENT COMES FIRST AND THE TRIANGLES COME LAST. The polygon is cut by each crease in
  // turn, so a crease is a cell BOUNDARY built from the same two endpoints on both sides; only when
  // no cut is left does anything get triangulated.
  Line lines[kMaxCreases];
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
      // WHAT CANNOT BE COVERED WHOLE IS NOT COVERED AT ALL. A partial cover is a roof with a hole in
      // it that no number reports; a refusal is one the count below can see.
      tris.resize(first);
      RoofSurface::Unclipped_.fetch_add(1u, std::memory_order_relaxed);
      return;
    }
  }
  if (mine.empty()) { return; }
  if (Shape_.Roof == RoofKind::Dome) Refine(mine, 4);
  // A ROOF VERTEX BELONGS INSIDE THE FOOTPRINT IT COVERS. Five explanations for the bright diagonals
  // across Rothenburg died in turn -- a fan, thin area, long reach, the clipper's bail-outs, and
  // aliasing, the last of them at twice the resolution where the slivers got WIDER rather than
  // vanishing. At that size they resolve into real thin roof surfaces reaching out past the building
  // they belong to, so this is the question that can only answer yes or no. The crossing number is
  // taken in the same E/N the ring is given in, and a vertex a hand's breadth outside is tolerated
  // because a cornice legitimately overhangs.
  for (size_t at = 0; at + 2 < mine.size(); at += 3) {
    for (size_t corner = 0; corner < 3; ++corner) {
      // THE MARGIN IS THE MITER LIMIT, not a hand's breadth. A pitched roof covers a ring WIDENED
      // by the building's own overhang, and `Widened` allows a corner to travel up to four times
      // that before it refuses -- so a legitimate eave corner stands `4 * OverhangM` outside the
      // footprint. A fixed 0.60 m counted those as defects: 14 331 at Rothenburg against overhangs
      // of 0.25 and 0.42 m, whose corners reach 1.00 and 1.68 m.
      const double reach = 4.0 * std::max({Shape_.OverhangM, kCorniceM, kOverhangM});
      if (!Inside(Shape_.Ring, mine[at + corner], reach)) {
        Outside_.fetch_add(1u, std::memory_order_relaxed);
        break;
      }
    }
  }
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
