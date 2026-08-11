#include "BuildingField.h"

#include "Geodesy.h"
#include "Log.h"
#include "TerrainLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace outshine::World {

namespace {

/* THE PROVIDER'S FILL, not a building's height. Measured on /t/vector/14/8617/5404 (Hameln): 1634 of
 * 1706 footprints carry exactly 5 — 95.8 %, integer, and the Altstadt they cover is three and four
 * storeys of half-timber. A single value on 96 % of a town is a default, and taking it literally puts
 * the whole of Hameln under a caravan roof. */
constexpr double kFillHeightM = 5.0;

/* [SET] two full storeys plus a roof: 2 x 2.9 m floor-to-floor = 5.8 m to the eaves, plus a 3.2 m
 * pitched roof on a ~9 m span at 35 deg. The right fix is upstream — fb-tiles must carry OSM's
 * building:levels — and this number exists to be deleted when it does. */
constexpr double kDefaultHeightM = 9.0;

}  // namespace

/* The lowest corner of a ring, or why there is none. Read TWICE per tile — once to decide whether the
 * tile can be consumed at all, once to build — because the second read is a hit in the oracle's own
 * tile cache while the first is what keeps a footprint from being thrown away for arriving early. */
GroundSample BuildingField::RingBase(const OsmField &field, const OsmField::Ring &ring) {
  const std::vector<double> &pts = field.Points();
  if (ring.Count == 0) return GroundSample::Missing();
  double lowest = 1.0e9;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const GroundSample g = fb_stream_ground(pts[((size_t)ring.First + k) * 2],
                                            pts[((size_t)ring.First + k) * 2 + 1]);
    double aslM = 0.0;
    if (!g.TryAslM(&aslM)) return g;   /* the corner's own reason travels out, unre-encoded */
    lowest = std::min(lowest, aslM);
  }
  return GroundSample::At(lowest);
}

bool BuildingField::TileGroundResolved(const OsmField &field, size_t from, size_t to,
                                       int layer) const {
  const std::vector<OsmField::Feature> &feats = field.Features();
  for (size_t i = from; i < to; i++) {
    const OsmField::Feature &f = feats[i];
    if (f.Type != 3 || (int)f.Layer != layer) continue;
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) continue;
      if (RingBase(field, ring).Where() == GroundSample::State::Pending) return false;
    }
  }
  return true;
}

int BuildingField::Build(const OsmField &field) {
  AddedFirst_ = (uint32_t)Verts_.size();
  AddedCount_ = 0;

  const std::vector<OsmField::Feature> &feats = field.Features();
  if (Mark_.Done(feats)) return (int)Prints_.size();

  const int layer = field.Layer(OsmLayer::Buildings);
  const std::vector<double> &pts = field.Points();
  const uint32_t firstPrint = (uint32_t)Prints_.size();
  int added = 0;

  /* A FOOTPRINT IS CONSUMED ONCE. Whichever ring is asked first, a tile is either buildable now or
   * asked again next pass — dropping a stand because its DEM had not landed yet is indistinguishable
   * from "nothing stands here" and is never revisited. */
  const TileWatermark::Next next = Mark_.Ask(feats, [&](size_t from, size_t to) {
    return TileGroundResolved(field, from, to, layer);
  });
  if (!next.Found) return (int)Prints_.size();
  Mark_.Take(next.Tile);

  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];
    if (f.Type != 3 || (int)f.Layer != layer) continue;
    const double h = field.Num(f, "height", 0.0);

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) continue;

      double base = 0.0;
      if (!RingBase(field, ring).TryAslM(&base)) { NoGround_++; continue; }

      Footprint fp{};
      fp.FirstPoint = ring.First;
      fp.PointCount = ring.Count;
      if (h > 0.0 && std::fabs(h - kFillHeightM) > 0.01) {
        fp.HeightM = (float)h;
        fp.Source = HeightSource::Osm;
        OsmHeights_++;
      } else {
        fp.HeightM = (float)kDefaultHeightM;
        fp.Source = HeightSource::Default;
        DefaultHeights_++;
      }
      fp.BaseM = (float)base;
      if (!HaveAnchor_) {
        GeoToEcef(pts[(size_t)fp.FirstPoint * 2], pts[(size_t)fp.FirstPoint * 2 + 1], base, Anchor_);
        HaveAnchor_ = true;
      }
      Prints_.push_back(fp);
      Extrude(field, fp);
      added++;
    }
  }

  ByTile_.Set(next.Tile, firstPrint, (uint32_t)Prints_.size());
  Mark_.Advance(feats);

  AddedCount_ = (uint32_t)Verts_.size() - AddedFirst_;
  if (added == 0) return (int)Prints_.size();

  Log::Info("world", "buildings", {{"added", added}, {"total", (int)Prints_.size()},
                                     {"osmHeight", OsmHeights_}, {"defaultHeight", DefaultHeights_},
                                     {"vertsMB", (double)Verts_.size() * 4.0 / 1.0e6},
                                     {"noGround", NoGround_}, {"deferrals", Mark_.Deferrals()},
                                     {"builtAhead", (int)Mark_.AheadCount()}});
  return (int)Prints_.size();
}

void BuildingField::Extrude(const OsmField &field, const Footprint &f) {
  const std::vector<double> &ring = field.Points();
  const uint32_t n = f.PointCount;
  std::vector<double> lo((size_t)n * 3), hi((size_t)n * 3), en((size_t)n * 2);
  const double refLat = ring[(size_t)f.FirstPoint * 2], refLon = ring[(size_t)f.FirstPoint * 2 + 1];

  /* Buildings sink 0.30 m so a prism standing on a 47 m terrain facet cannot show daylight under a
   * wall where the facet dips away from the sampled corner. */
  const double sink = 0.30;
  for (uint32_t k = 0; k < n; k++) {
    const double la = ring[((size_t)f.FirstPoint + k) * 2], ln = ring[((size_t)f.FirstPoint + k) * 2 + 1];
    double p[3];
    GeoToEcef(la, ln, f.BaseM - sink, p);
    for (int c = 0; c < 3; c++) lo[(size_t)k * 3 + c] = p[c] - Anchor_[c];
    GeoToEcef(la, ln, f.BaseM + f.HeightM, p);
    for (int c = 0; c < 3; c++) hi[(size_t)k * 3 + c] = p[c] - Anchor_[c];
    EnuOffsetM(refLat, refLon, la, ln, en[(size_t)k * 2], en[(size_t)k * 2 + 1]);
  }

  double up[3];
  {
    double e[3], nn[3];
    EnuAxesEcef(refLat, refLon, e, nn, up);
  }

  auto push = [&](const double p[3], const double nrm[3], double u, double v) {
    Verts_.push_back((float)p[0]); Verts_.push_back((float)p[1]); Verts_.push_back((float)p[2]);
    Verts_.push_back((float)u); Verts_.push_back((float)v);
    Verts_.push_back((float)nrm[0]); Verts_.push_back((float)nrm[1]); Verts_.push_back((float)nrm[2]);
  };

  double run = 0.0;
  for (uint32_t k = 0; k < n; k++) {
    const uint32_t j = (k + 1) % n;
    const double dx = en[(size_t)j * 2] - en[(size_t)k * 2];
    const double dy = en[(size_t)j * 2 + 1] - en[(size_t)k * 2 + 1];
    const double seg = std::sqrt(dx * dx + dy * dy);
    if (seg < 0.05) continue;

    double t[3];
    for (int c = 0; c < 3; c++) t[c] = (lo[(size_t)j * 3 + c] - lo[(size_t)k * 3 + c]) / seg;
    double nrm[3] = {t[1] * up[2] - t[2] * up[1], t[2] * up[0] - t[0] * up[2], t[0] * up[1] - t[1] * up[0]};
    const double nl = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (nl < 1e-6) continue;
    for (int c = 0; c < 3; c++) nrm[c] /= nl;

    const double h = (double)f.HeightM + sink;
    const double *a = &lo[(size_t)k * 3], *b = &lo[(size_t)j * 3];
    const double *c2 = &hi[(size_t)j * 3], *d = &hi[(size_t)k * 3];
    push(a, nrm, run, 0.0);        push(b, nrm, run + seg, 0.0);  push(c2, nrm, run + seg, h);
    push(a, nrm, run, 0.0);        push(c2, nrm, run + seg, h);   push(d, nrm, run, h);
    run += seg;
  }

  /* A FLAT cap by ear clipping, because a fan off vertex 0 fills the convex hull and Hameln's plans
   * are L- and U-shaped. The pitched roof this town actually has is the next generator over the same
   * Footprint — it needs the ring and the eaves height, which is exactly what is kept. */
  std::vector<uint32_t> poly((size_t)n);
  double area = 0.0;
  for (uint32_t k = 0; k < n; k++) {
    const uint32_t j = (k + 1) % n;
    area += en[(size_t)k * 2] * en[(size_t)j * 2 + 1] - en[(size_t)j * 2] * en[(size_t)k * 2 + 1];
  }
  for (uint32_t k = 0; k < n; k++) poly[k] = area >= 0.0 ? k : n - 1 - k;

  auto cross2 = [&](uint32_t a, uint32_t b, uint32_t c) {
    return (en[(size_t)b * 2] - en[(size_t)a * 2]) * (en[(size_t)c * 2 + 1] - en[(size_t)a * 2 + 1]) -
           (en[(size_t)c * 2] - en[(size_t)a * 2]) * (en[(size_t)b * 2 + 1] - en[(size_t)a * 2 + 1]);
  };
  int guard = (int)n * (int)n + 8;
  while (poly.size() > 2 && guard-- > 0) {
    bool cut = false;
    for (size_t i = 0; i < poly.size(); i++) {
      const uint32_t a = poly[(i + poly.size() - 1) % poly.size()], b = poly[i],
                     c = poly[(i + 1) % poly.size()];
      if (cross2(a, b, c) <= 0.0) continue;
      bool clean = true;
      for (uint32_t o : poly) {
        if (o == a || o == b || o == c) continue;
        if (cross2(a, b, o) >= 0.0 && cross2(b, c, o) >= 0.0 && cross2(c, a, o) >= 0.0) { clean = false; break; }
      }
      if (!clean) continue;
      /* uv.x = -1 TAGS the cap; the walls' own run is >= 0 by construction, so one float carries the
       * material split without a second attribute and without the shader guessing from a normal. */
      push(&hi[(size_t)a * 3], up, -1.0, (double)f.HeightM);
      push(&hi[(size_t)b * 3], up, -1.0, (double)f.HeightM);
      push(&hi[(size_t)c * 3], up, -1.0, (double)f.HeightM);
      poly.erase(poly.begin() + (long)i);
      cut = true;
      break;
    }
    if (!cut) break;   /* self-intersecting ring: what was clipped stands, the rest is dropped */
  }
}

} // namespace outshine::World
