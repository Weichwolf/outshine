#include "WaterField.h"

#include "Geodesy.h"
#include "TerrainLoader.h"

#include <algorithm>
#include <cmath>

namespace outshine::World {

namespace {

/* The shoreline IS the water level, so the level is read off the ring — but not as its minimum. The
 * minimum is one texel: measured on the Weser at Hameln over 26 292 water pixels, min 62.97 m against
 * p5 63.00 m against max 129.93 m, so an outlier at either end exists and only one of them is cheap to
 * be wrong about. The 5th percentile keeps the shore and drops the stray. */
constexpr double kLevelPercentile = 0.05;

/* Beyond this above the level a ring point is not shore but hillside, and the ring is a water body
 * whose outline OSM and the DEM disagree about. [SET] 5 m: an order over the 1.12 m the Weser's own
 * fall contributes across a whole z14 tile, well under the 67 m the disagreement produced. */
constexpr double kShoreToleranceM = 5.0;

/* [SET] 0.15 m of lift. The DEM already carries a flat surface under a water body — measured on the
 * Weser at Hameln, p5..p95 = 1.12 m over 1.5 km — so the water mesh and the terrain mesh land
 * COPLANAR and fight for every pixel. Every engine lifts its water plane for the same reason it lifts
 * a decal; 0.15 m is under the terrain's own 47 m support spacing and over any float error at ECEF
 * magnitudes. */
constexpr double kLiftM = 0.15;

}  // namespace

uint32_t WaterField::Ingest(const OsmField &field, const VegetationTemplates &veg) {
  const std::vector<OsmField::Feature> &feats = field.Features();
  if (Consumed_ >= feats.size()) return (uint32_t)Surfaces_.size();

  const int poly = field.Layer("water_polygons");
  const int line = field.Layer("water_lines");
  const uint32_t tile = feats[Consumed_].Tile;
  const std::vector<double> &pts = field.Points();
  std::vector<double> hs;

  for (; Consumed_ < feats.size() && feats[Consumed_].Tile == tile; Consumed_++) {
    const OsmField::Feature &f = feats[Consumed_];
    /* A culvert carries no water at the surface. The bake draws one anyway (tiles/src/raster.c has
     * no tunnel test), so chasing its coverage would be rebuilding its artefact. */
    if (field.Num(f, "tunnel", 0.0) > 0.5) continue;
    if (f.Type == 2 && (int)f.Layer == line) {
      for (uint32_t r = 0; r < f.RingCount; r++) {
        const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
        if (ring.Count < 2 || ring.Count > 512) continue;
        hs.clear();
        bool ok = true;
        for (uint32_t k = 0; k < ring.Count; k++) {
          const double e = fb_stream_ground(pts[((size_t)ring.First + k) * 2],
                                            pts[((size_t)ring.First + k) * 2 + 1]);
          if (e <= -1e7) { ok = false; break; }
          hs.push_back(e);
        }
        if (!ok) { NoGround_++; continue; }
        /* Water runs downhill, so the sequence along the way is monotone — which end is downstream is
         * whichever end sits lower. Enforcing it turns the terrain's own noise into a surface that
         * cannot run uphill, and that is the whole claim being made here. */
        if (hs.front() >= hs.back())
          for (size_t k = 1; k < hs.size(); k++) hs[k] = std::min(hs[k], hs[k - 1]);
        else
          for (size_t k = hs.size() - 1; k-- > 0;) hs[k] = std::min(hs[k], hs[k + 1]);
        Course c{};
        c.FirstPoint = ring.First;
        c.PointCount = ring.Count;
        c.FirstLevel = (uint32_t)Levels_.size();
        const VegetationTemplates::Rule *rule = veg.Find(field.LayerName((int)f.Layer), field.Str(f, "kind"));
        c.HalfWidthM = rule && rule->WidthM > 0.0f ? rule->WidthM * 0.5f : 1.0f;
        for (double h : hs) Levels_.push_back((float)h);
        if (!HaveAnchor_) {
          GeoToEcef(pts[(size_t)c.FirstPoint * 2], pts[(size_t)c.FirstPoint * 2 + 1], hs[0], Anchor_);
          HaveAnchor_ = true;
        }
        Courses_.push_back(c);
      }
      continue;
    }
    if (f.Type != 3 || (int)f.Layer != poly) continue;

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) continue;

      hs.clear();
      bool ground = true;
      for (uint32_t k = 0; k < ring.Count; k++) {
        const double e = fb_stream_ground(pts[((size_t)ring.First + k) * 2],
                                          pts[((size_t)ring.First + k) * 2 + 1]);
        if (e <= -1e7) { ground = false; break; }
        hs.push_back(e);
      }
      if (!ground) { NoGround_++; continue; }

      std::sort(hs.begin(), hs.end());
      const double level = hs[(size_t)(kLevelPercentile * (double)(hs.size() - 1))];
      for (double h : hs)
        if (h > level + kShoreToleranceM) { Outliers_++; break; }

      Surface s{};
      s.FirstPoint = ring.First;
      s.PointCount = ring.Count;
      s.LevelM = (float)level;
      if (!HaveAnchor_) {
        GeoToEcef(pts[(size_t)s.FirstPoint * 2], pts[(size_t)s.FirstPoint * 2 + 1], level, Anchor_);
        HaveAnchor_ = true;
      }
      Surfaces_.push_back(s);
    }
  }
  return (uint32_t)Surfaces_.size();
}

void WaterField::Tessellate(const OsmField &field, std::vector<float> &out) const {
  out.clear();
  if (!HaveAnchor_) return;
  const std::vector<double> &ring = field.Points();
  std::vector<double> p3;

  for (const Surface &s : Surfaces_) {
    const uint32_t n = s.PointCount;
    if (n < 3) continue;
    const double refLat = ring[(size_t)s.FirstPoint * 2], refLon = ring[(size_t)s.FirstPoint * 2 + 1];
    double up[3];
    { double e[3], nn[3]; EnuAxesEcef(refLat, refLon, e, nn, up); }

    p3.resize((size_t)n * 3);
    for (uint32_t k = 0; k < n; k++) {
      double p[3];
      GeoToEcef(ring[((size_t)s.FirstPoint + k) * 2], ring[((size_t)s.FirstPoint + k) * 2 + 1],
                s.LevelM + kLiftM, p);
      for (int c = 0; c < 3; c++) p3[(size_t)k * 3 + c] = p[c] - Anchor_[c];
    }
    /* EAR CLIPPING, because a fan folded: measured on the Hannover tile it covered 13 % of the water
     * at 52 % precision — half of what it drew lay outside the outline. A lake is concave often
     * enough that the convex shortcut is simply wrong. */
    std::vector<double> en((size_t)n * 2);
    for (uint32_t k = 0; k < n; k++)
      EnuOffsetM(refLat, refLon, ring[((size_t)s.FirstPoint + k) * 2],
                 ring[((size_t)s.FirstPoint + k) * 2 + 1], en[(size_t)k * 2], en[(size_t)k * 2 + 1]);
    double area2 = 0.0;
    for (uint32_t k = 0; k < n; k++) {
      const uint32_t j = (k + 1) % n;
      area2 += en[(size_t)k * 2] * en[(size_t)j * 2 + 1] - en[(size_t)j * 2] * en[(size_t)k * 2 + 1];
    }
    std::vector<uint32_t> poly(n);
    for (uint32_t k = 0; k < n; k++) poly[k] = area2 >= 0.0 ? k : n - 1 - k;
    auto cross = [&](uint32_t a2, uint32_t b2, uint32_t c2) {
      return (en[(size_t)b2 * 2] - en[(size_t)a2 * 2]) * (en[(size_t)c2 * 2 + 1] - en[(size_t)a2 * 2 + 1]) -
             (en[(size_t)b2 * 2 + 1] - en[(size_t)a2 * 2 + 1]) * (en[(size_t)c2 * 2] - en[(size_t)a2 * 2]);
    };
    auto inside = [&](uint32_t a2, uint32_t b2, uint32_t c2, uint32_t q) {
      return cross(a2, b2, q) >= 0.0 && cross(b2, c2, q) >= 0.0 && cross(c2, a2, q) >= 0.0;
    };
    size_t guard = (size_t)n * (size_t)n + 16;   /* a self-intersecting ring must not spin forever */
    while (poly.size() >= 3 && guard-- > 0) {
      bool clipped = false;
      for (size_t k = 0; k < poly.size(); k++) {
        const uint32_t a2 = poly[(k + poly.size() - 1) % poly.size()], b2 = poly[k],
                       c2 = poly[(k + 1) % poly.size()];
        if (cross(a2, b2, c2) <= 0.0) continue;                 /* reflex */
        bool clear = true;
        for (uint32_t q : poly)
          if (q != a2 && q != b2 && q != c2 && inside(a2, b2, c2, q)) { clear = false; break; }
        if (!clear) continue;
        for (uint32_t idx : {a2, b2, c2}) {
          const double *v = &p3[(size_t)idx * 3];
          out.push_back((float)v[0]); out.push_back((float)v[1]); out.push_back((float)v[2]);
          out.push_back((float)up[0]); out.push_back((float)up[1]); out.push_back((float)up[2]);
        }
        poly.erase(poly.begin() + (long)k);
        clipped = true;
        break;
      }
      if (!clipped) break;
    }
  }

  for (const Course &c : Courses_) {
    if (c.PointCount < 2) continue;
    const double refLat = ring[(size_t)c.FirstPoint * 2], refLon = ring[(size_t)c.FirstPoint * 2 + 1];
    double up[3], ea[3], no[3];
    EnuAxesEcef(refLat, refLon, ea, no, up);
    std::vector<double> L((size_t)c.PointCount * 3), R((size_t)c.PointCount * 3);
    for (uint32_t k = 0; k < c.PointCount; k++) {
      double e0 = 0.0, n0 = 0.0, e1 = 0.0, n1 = 0.0;
      const uint32_t a = k > 0 ? k - 1 : k, b = k + 1 < c.PointCount ? k + 1 : k;
      EnuOffsetM(refLat, refLon, ring[((size_t)c.FirstPoint + a) * 2],
                 ring[((size_t)c.FirstPoint + a) * 2 + 1], e0, n0);
      EnuOffsetM(refLat, refLon, ring[((size_t)c.FirstPoint + b) * 2],
                 ring[((size_t)c.FirstPoint + b) * 2 + 1], e1, n1);
      double tx = e1 - e0, ty = n1 - n0;
      const double tl = std::sqrt(tx * tx + ty * ty);
      if (tl < 1e-9) { tx = 1.0; ty = 0.0; } else { tx /= tl; ty /= tl; }
      const double px = -ty * c.HalfWidthM, py = tx * c.HalfWidthM;
      const double lat = ring[((size_t)c.FirstPoint + k) * 2], lon = ring[((size_t)c.FirstPoint + k) * 2 + 1];
      const double lev = (double)Levels_[c.FirstLevel + k] + kLiftM;
      double base[3];
      GeoToEcef(lat, lon, lev, base);
      for (int cc = 0; cc < 3; cc++) {
        L[(size_t)k * 3 + cc] = base[cc] - Anchor_[cc] + ea[cc] * px + no[cc] * py;
        R[(size_t)k * 3 + cc] = base[cc] - Anchor_[cc] - ea[cc] * px - no[cc] * py;
      }
    }
    for (uint32_t k = 0; k + 1 < c.PointCount; k++) {
      const double *q[6] = {&L[(size_t)k * 3], &R[(size_t)k * 3], &R[(size_t)(k + 1) * 3],
                            &L[(size_t)k * 3], &R[(size_t)(k + 1) * 3], &L[(size_t)(k + 1) * 3]};
      for (int t = 0; t < 6; t++) {
        out.push_back((float)q[t][0]); out.push_back((float)q[t][1]); out.push_back((float)q[t][2]);
        out.push_back((float)up[0]); out.push_back((float)up[1]); out.push_back((float)up[2]);
      }
    }
  }
}

} // namespace outshine::World
