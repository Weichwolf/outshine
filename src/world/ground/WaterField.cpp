#include "WaterField.h"

#include "Geodesy.h"
#include "TerrainLoader.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace outshine::Ground {

namespace {

constexpr double kLevelPercentile = 0.05;

constexpr double kShoreToleranceM = 5.0;

constexpr double kLiftM = 0.15;

} // namespace

bool WaterField::TileGroundResolved(const GroundQuery &ground,
                                    const OsmField &field,
                                    size_t from,
                                    size_t to,
                                    int poly,
                                    int line) const {
  const std::span<const double> pts = field.Points();
  const std::span<const OsmField::Feature> feats = field.Features();
  for (size_t i = from; i < to; i++) {
    const OsmField::Feature &f = feats[i];
    if (field.Num(f, "tunnel", 0.0) > 0.5) { continue; }
    const bool isLine = f.Type == 2 && (int)f.Layer == line;
    const bool isPoly = f.Type == 3 && (int)f.Layer == poly;
    if (!isLine && !isPoly) { continue; }
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (isPoly && (!ring.Exterior || ring.Count < 3 || ring.Count > 512)) { continue; }
      if (isLine && (ring.Count < 2 || ring.Count > 512)) { continue; }
      for (uint32_t k = 0; k < ring.Count; k++) {
        if (ground.At(pts[((size_t)ring.First + k) * 2], pts[((size_t)ring.First + k) * 2 + 1])
                .Where() == GroundSample::State::Pending) {
          return false;
        }
      }
    }
  }
  return true;
}

void WaterField::AnchorAt(const double ecef[3]) {
  assert(Surfaces_.empty() && Courses_.empty());
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
}

uint32_t WaterField::Ingest(const GroundQuery &ground,
                            const OsmField &field,
                            const VegetationTemplates &veg) {
  assert(Anchored_);
  const std::span<const OsmField::Feature> feats = field.Features();
  if (Mark_.Done(feats)) { return (uint32_t)Surfaces_.size(); }

  const int poly = field.Layer(OsmLayer::WaterPolygons);
  const int line = field.Layer(OsmLayer::WaterLines);
  const TileWatermark::Next next = Mark_.Ask(feats, [&](size_t from, size_t to) {
    return TileGroundResolved(ground, field, from, to, poly, line);
  });
  if (!next.Found) { return (uint32_t)Surfaces_.size(); }
  Mark_.Take(next.Tile);
  Mark_.Advance(feats);

  const std::span<const double> pts = field.Points();
  const uint32_t firstSurface = (uint32_t)Surfaces_.size();
  std::vector<double> hs;

  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];

    if (field.Num(f, "tunnel", 0.0) > 0.5) { continue; }
    if (f.Type == 2 && (int)f.Layer == line) {
      for (uint32_t r = 0; r < f.RingCount; r++) {
        const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
        if (ring.Count < 2 || ring.Count > 512) { continue; }
        hs.clear();
        bool ok = true;
        for (uint32_t k = 0; k < ring.Count; k++) {
          double aslM = 0.0;
          if (!ground.At(pts[((size_t)ring.First + k) * 2], pts[((size_t)ring.First + k) * 2 + 1])
                   .TryAslM(&aslM)) {
            ok = false;
            break;
          }
          hs.push_back(aslM);
        }
        if (!ok) {
          NoGround_++;
          continue;
        }

        if (hs.front() >= hs.back()) {
          for (size_t k = 1; k < hs.size(); k++) { hs[k] = std::min(hs[k], hs[k - 1]); }
        } else {
          for (size_t k = hs.size() - 1; k-- > 0;) { hs[k] = std::min(hs[k], hs[k + 1]); }
        }
        Course course{};
        course.FirstPoint = ring.First;
        course.PointCount = ring.Count;
        course.FirstLevel = (uint32_t)Levels_.size();
        const VegetationTemplates::Rule *rule =
            veg.Find(field.LayerName((int)f.Layer), field.Str(f, "kind"));
        course.HalfWidthM = rule && rule->WidthM > 0.0f ? rule->WidthM * 0.5f : 1.0f;
        for (double h : hs) { Levels_.push_back((float)h); }
        Courses_.push_back(course);
      }
      continue;
    }
    if (f.Type != 3 || (int)f.Layer != poly) { continue; }

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) { continue; }

      hs.clear();
      bool resolved = true;
      for (uint32_t k = 0; k < ring.Count; k++) {
        double aslM = 0.0;
        if (!ground.At(pts[((size_t)ring.First + k) * 2], pts[((size_t)ring.First + k) * 2 + 1])
                 .TryAslM(&aslM)) {
          resolved = false;
          break;
        }
        hs.push_back(aslM);
      }
      if (!resolved) {
        NoGround_++;
        continue;
      }

      std::sort(hs.begin(), hs.end());
      const double level = hs[(size_t)(kLevelPercentile * (double)(hs.size() - 1))];
      for (double h : hs) {
        if (h > level + kShoreToleranceM) {
          Outliers_++;
          break;
        }
      }

      Surface s{};
      s.FirstPoint = ring.First;
      s.PointCount = ring.Count;
      s.LevelM = (float)level;
      Surfaces_.push_back(s);
    }
  }
  ByTile_.Set(next.Tile, firstSurface, (uint32_t)Surfaces_.size());
  return (uint32_t)Surfaces_.size();
}

void WaterField::Tessellate(const OsmField &field, std::vector<float> &out) const {
  out.clear();
  const std::span<const double> ring = field.Points();
  std::vector<double> p3;

  for (const Surface &s : Surfaces_) {
    const uint32_t n = s.PointCount;
    if (n < 3) { continue; }
    const double refLat = ring[(size_t)s.FirstPoint * 2],
                 refLon = ring[(size_t)s.FirstPoint * 2 + 1];
    double up[3];
    {
      double e[3], nn[3];
      EnuAxesEcef(refLat, refLon, e, nn, up);
    }

    p3.resize((size_t)n * 3);
    for (uint32_t k = 0; k < n; k++) {
      double p[3];
      GeoToEcef(ring[((size_t)s.FirstPoint + k) * 2],
                ring[((size_t)s.FirstPoint + k) * 2 + 1],
                s.LevelM + kLiftM,
                p);
      for (int c = 0; c < 3; c++) { p3[(size_t)k * 3 + c] = p[c] - Anchor_[c]; }
    }

    std::vector<double> en((size_t)n * 2);
    for (uint32_t k = 0; k < n; k++) {
      EnuOffsetM(refLat,
                 refLon,
                 ring[((size_t)s.FirstPoint + k) * 2],
                 ring[((size_t)s.FirstPoint + k) * 2 + 1],
                 en[(size_t)k * 2],
                 en[(size_t)k * 2 + 1]);
    }
    double area2 = 0.0;
    for (uint32_t k = 0; k < n; k++) {
      const uint32_t j = (k + 1) % n;
      area2 +=
          en[(size_t)k * 2] * en[(size_t)j * 2 + 1] - en[(size_t)j * 2] * en[(size_t)k * 2 + 1];
    }
    std::vector<uint32_t> poly(n);
    for (uint32_t k = 0; k < n; k++) { poly[k] = area2 >= 0.0 ? k : n - 1 - k; }
    auto cross = [&](uint32_t a2, uint32_t b2, uint32_t c2) {
      return (en[(size_t)b2 * 2] - en[(size_t)a2 * 2]) *
                 (en[(size_t)c2 * 2 + 1] - en[(size_t)a2 * 2 + 1]) -
             (en[(size_t)b2 * 2 + 1] - en[(size_t)a2 * 2 + 1]) *
                 (en[(size_t)c2 * 2] - en[(size_t)a2 * 2]);
    };
    auto inside = [&](uint32_t a2, uint32_t b2, uint32_t c2, uint32_t q) {
      return cross(a2, b2, q) >= 0.0 && cross(b2, c2, q) >= 0.0 && cross(c2, a2, q) >= 0.0;
    };
    size_t guard = (size_t)n * (size_t)n + 16;
    while (poly.size() >= 3 && guard-- > 0) {
      bool clipped = false;
      for (size_t k = 0; k < poly.size(); k++) {
        const uint32_t a2 = poly[(k + poly.size() - 1) % poly.size()], b2 = poly[k],
                       c2 = poly[(k + 1) % poly.size()];
        if (cross(a2, b2, c2) <= 0.0) { continue; }
        bool clear = true;
        for (uint32_t q : poly) {
          if (q != a2 && q != b2 && q != c2 && inside(a2, b2, c2, q)) {
            clear = false;
            break;
          }
        }
        if (!clear) { continue; }
        for (uint32_t idx : {a2, b2, c2}) {
          const double *v = &p3[(size_t)idx * 3];
          out.push_back((float)v[0]);
          out.push_back((float)v[1]);
          out.push_back((float)v[2]);
          out.push_back((float)up[0]);
          out.push_back((float)up[1]);
          out.push_back((float)up[2]);
        }
        poly.erase(poly.begin() + (long)k);
        clipped = true;
        break;
      }
      if (!clipped) { break; }
    }
  }

  for (const Course &c : Courses_) {
    if (c.PointCount < 2) { continue; }
    const double refLat = ring[(size_t)c.FirstPoint * 2],
                 refLon = ring[(size_t)c.FirstPoint * 2 + 1];
    double up[3], ea[3], no[3];
    EnuAxesEcef(refLat, refLon, ea, no, up);
    std::vector<double> L((size_t)c.PointCount * 3), R((size_t)c.PointCount * 3);
    for (uint32_t k = 0; k < c.PointCount; k++) {
      double e0 = 0.0, n0 = 0.0, e1 = 0.0, n1 = 0.0;
      const uint32_t a = k > 0 ? k - 1 : k, b = k + 1 < c.PointCount ? k + 1 : k;
      EnuOffsetM(refLat,
                 refLon,
                 ring[((size_t)c.FirstPoint + a) * 2],
                 ring[((size_t)c.FirstPoint + a) * 2 + 1],
                 e0,
                 n0);
      EnuOffsetM(refLat,
                 refLon,
                 ring[((size_t)c.FirstPoint + b) * 2],
                 ring[((size_t)c.FirstPoint + b) * 2 + 1],
                 e1,
                 n1);
      double tx = e1 - e0, ty = n1 - n0;
      const double tl = std::sqrt(tx * tx + ty * ty);
      if (tl < 1e-9) {
        tx = 1.0;
        ty = 0.0;
      } else {
        tx /= tl;
        ty /= tl;
      }
      const double px = -ty * c.HalfWidthM, py = tx * c.HalfWidthM;
      const double lat = ring[((size_t)c.FirstPoint + k) * 2],
                   lon = ring[((size_t)c.FirstPoint + k) * 2 + 1];
      const double lev = (double)Levels_[c.FirstLevel + k] + kLiftM;
      double base[3];
      GeoToEcef(lat, lon, lev, base);
      for (int cc = 0; cc < 3; cc++) {
        L[(size_t)k * 3 + cc] = base[cc] - Anchor_[cc] + ea[cc] * px + no[cc] * py;
        R[(size_t)k * 3 + cc] = base[cc] - Anchor_[cc] - ea[cc] * px - no[cc] * py;
      }
    }
    for (uint32_t k = 0; k + 1 < c.PointCount; k++) {
      const double *q[6] = {&L[(size_t)k * 3],
                            &R[(size_t)k * 3],
                            &R[(size_t)(k + 1) * 3],
                            &L[(size_t)k * 3],
                            &R[(size_t)(k + 1) * 3],
                            &L[(size_t)(k + 1) * 3]};
      for (int t = 0; t < 6; t++) {
        out.push_back((float)q[t][0]);
        out.push_back((float)q[t][1]);
        out.push_back((float)q[t][2]);
        out.push_back((float)up[0]);
        out.push_back((float)up[1]);
        out.push_back((float)up[2]);
      }
    }
  }
}

} // namespace outshine::Ground
