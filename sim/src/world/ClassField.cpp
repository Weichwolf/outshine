#include "ClassField.h"

#include "Geodesy.h"
#include "Log.h"
#include "VegetationTemplates.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace outshine::World {

/* An OSM way is a SAMPLE of a curve, not a description of a polygon: a surveyor sets a node where
 * the bend needs one, and the chord between two nodes is the sampling artefact, not the road. So the
 * chords are replaced by the centripetal Catmull-Rom through the same nodes — it passes through every
 * one of them, so no declared position moves, and it only adds where the curve leaves the chord.
 *
 * kCurveTolM 0.60 m [SET]: the cost is measured in doc/render/classification.md, not guessed here. */
static constexpr float kCurveTolM = 0.60f;
static constexpr int kCurveMaxSplit = 8;

/* Barry-Goldman, knots spaced by sqrt of chord length: the CENTRIPETAL parameterisation, which is
 * the one that cannot cusp or loop between two nodes however uneven the survey's spacing. */
static void CatmullPoint(const float *p0, const float *p1, const float *p2, const float *p3,
                         float u, float *out) {
  auto knot = [](float t, const float *a, const float *b) {
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    return t + std::sqrt(std::sqrt(dx * dx + dy * dy) + 1.0e-6f);
  };
  const float t0 = 0.0f, t1 = knot(t0, p0, p1), t2 = knot(t1, p1, p2), t3 = knot(t2, p2, p3);
  const float t = t1 + u * (t2 - t1);
  float a1[2], a2[2], a3[2], b1[2], b2[2];
  for (int a = 0; a < 2; a++) {
    a1[a] = ((t1 - t) * p0[a] + (t - t0) * p1[a]) / (t1 - t0);
    a2[a] = ((t2 - t) * p1[a] + (t - t1) * p2[a]) / (t2 - t1);
    a3[a] = ((t3 - t) * p2[a] + (t - t2) * p3[a]) / (t3 - t2);
  }
  for (int a = 0; a < 2; a++) {
    b1[a] = ((t2 - t) * a1[a] + (t - t0) * a2[a]) / (t2 - t0);
    b2[a] = ((t3 - t) * a2[a] + (t - t1) * a3[a]) / (t3 - t1);
  }
  for (int a = 0; a < 2; a++) out[a] = ((t2 - t) * b1[a] + (t - t1) * b2[a]) / (t2 - t1);
}

/* Ring points in, curve points out. `closed` wraps the neighbour lookup; an open way reflects its
 * end tangents so the first and last spans bend like their neighbours instead of going straight. */
static void CurveRing(const float *pts, uint32_t first, uint32_t count, bool closed,
                      std::vector<float> &out) {
  out.clear();
  if (count < 2) return;
  auto P = [&](int i) -> const float * {
    int n = (int)count;
    if (closed) { i = ((i % n) + n) % n; }
    else { i = i < 0 ? 0 : (i >= n ? n - 1 : i); }
    return pts + ((size_t)first + (size_t)i) * 2;
  };
  const int spans = closed ? (int)count : (int)count - 1;
  for (int s = 0; s < spans; s++) {
    const float *p0 = P(s - 1), *p1 = P(s), *p2 = P(s + 1), *p3 = P(s + 2);
    float mid[2];
    CatmullPoint(p0, p1, p2, p3, 0.5f, mid);
    const float cx = 0.5f * (p1[0] + p2[0]), cy = 0.5f * (p1[1] + p2[1]);
    const float dev = std::sqrt((mid[0] - cx) * (mid[0] - cx) + (mid[1] - cy) * (mid[1] - cy));
    int n = 1;
    if (dev > kCurveTolM) {
      n = (int)std::ceil(std::sqrt(dev / kCurveTolM));
      if (n > kCurveMaxSplit) n = kCurveMaxSplit;
    }
    for (int k = 0; k < n; k++) {
      float q[2];
      CatmullPoint(p0, p1, p2, p3, (float)k / (float)n, q);
      out.push_back(q[0]); out.push_back(q[1]);
    }
  }
  if (!closed) {
    const float *last = pts + ((size_t)first + count - 1) * 2;
    out.push_back(last[0]); out.push_back(last[1]);
  }
}


namespace {

double Clock() {
  using namespace std::chrono;
  return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() * 1e-3;
}

constexpr int kSeedCap = 32;    /* per cell; measured worst 14 over the reference block */
constexpr int kRefCap = 255;    /* per seed; measured worst 45 edges in one whole cell */

/* WHAT A WINDING NUMBER CHANGES BY when the point moves along an axis. Both the CPU's row scan and
 * the fragment's two legs use these and nothing else, so a disagreement between them is not
 * expressible. Moving +x across an edge: the sign of the edge's own dy. Moving +y: minus the sign of
 * its dx. */
inline int CrossX(float x0, float y0, float x1, float y1, double cy, double xa, double xb) {
  if ((y0 <= cy) == (y1 <= cy)) return 0;
  const double xi = (double)x0 + (cy - (double)y0) * ((double)x1 - (double)x0) /
                                     ((double)y1 - (double)y0);
  if (xi < xa || xi >= xb) return 0;
  return y1 > y0 ? 1 : -1;
}

inline int CrossY(float x0, float y0, float x1, float y1, double cx, double ya, double yb) {
  if ((x0 <= cx) == (x1 <= cx)) return 0;
  const double yi = (double)y0 + (cx - (double)x0) * ((double)y1 - (double)y0) /
                                     ((double)x1 - (double)x0);
  if (yi < ya || yi >= yb) return 0;
  return x1 > x0 ? -1 : 1;
}

double SegDist(double px, double py, float x0, float y0, float x1, float y1) {
  const double dx = (double)x1 - x0, dy = (double)y1 - y0;
  const double l2 = dx * dx + dy * dy;
  double t = l2 > 0.0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0.0;
  t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
  const double qx = x0 + t * dx - px, qy = y0 + t * dy - py;
  return std::sqrt(qx * qx + qy * qy);
}

}  // namespace

void ClassField::Open(double lat, double lon) {
  Lat0_ = lat; Lon0_ = lon;
  GeoToEcef(lat, lon, 0.0, O_);
  double up[3];
  EnuAxesEcef(lat, lon, East_, North_, up);
  Opened_ = true;
}

void ClassField::Project(double lat, double lon, double *e, double *n) const {
  double p[3];
  GeoToEcef(lat, lon, 0.0, p);
  const double d[3] = {p[0] - O_[0], p[1] - O_[1], p[2] - O_[2]};
  *e = d[0] * East_[0] + d[1] * East_[1] + d[2] * East_[2];
  *n = d[0] * North_[0] + d[1] * North_[1] + d[2] * North_[2];
}

void ClassField::FromEnu(double e, double n, double *lat, double *lon) const {
  const double p[3] = {O_[0] + East_[0] * e + North_[0] * n,
                       O_[1] + East_[1] * e + North_[1] * n,
                       O_[2] + East_[2] * e + North_[2] * n};
  (void)p;
  /* Die lokale Tangentialebene: ueber die 900 m, in denen gestreut wird, liegt ihr Fehler unter einem
   * Meter, und die Hoehe, die damit erfragt wird, hat selbst 47 m Stuetzstellenabstand. */
  constexpr double kMPerDeg = 111320.0;
  *lat = Lat0_ + n / kMPerDeg;
  *lon = Lon0_ + e / (kMPerDeg * std::cos(Lat0_ * 3.14159265358979 / 180.0));
}

void ClassField::Ingest(Tier &t) {
  const std::vector<double> &pts = t.Field->Points();
  const size_t havePts = pts.size() / 2;
  if (havePts > t.PtsDone) {
    t.Pts.resize(havePts * 2);
    for (size_t i = t.PtsDone; i < havePts; i++) {
      double e = 0, n = 0;
      Project(pts[i * 2], pts[i * 2 + 1], &e, &n);
      t.Pts[i * 2] = (float)e;
      t.Pts[i * 2 + 1] = (float)n;
    }
    t.PtsDone = havePts;
  }

  const std::vector<OsmField::Feature> &feats = t.Field->Features();
  if (feats.size() <= t.FeatsDone) return;

  for (size_t i = t.FeatsDone; i < feats.size(); i++) {
    const OsmField::Feature &f = feats[i];
    const std::string_view layer = t.Field->LayerName((int)f.Layer);
    const std::string_view kind = t.Field->Str(f, "kind");
    const VegetationTemplates::Rule *rule = Veg_->Find(layer, kind);
    if (!rule) {
      std::string key(layer);
      key.append("/").append(kind);
      if (Unknown_.insert(key).second)
        Log::Error("world", "class_unknown_kind",
                   {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
      UnknownFeats_++;
      continue;
    }
    /* A culvert or a tunnel carries nothing at the surface. shortbread hands us `tunnel` on every
     * layer that can have one and every OSM renderer acts on it; the bake does not, which is why its
     * water looks fuller than the ground actually is. */
    if (t.Field->Num(f, "tunnel", 0.0) > 0.5) continue;
    if (f.Type != 2 && f.Type != 3) continue;
    if (f.Type == 2 && rule->WidthM <= 0.0f) {
      std::string key(layer);
      key.append("/").append(kind).append("#width");
      if (Unknown_.insert(key).second)
        Log::Error("world", "class_line_without_width",
                   {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
      UnknownFeats_++;
      continue;
    }

    Feat rec{};
    rec.Idx = (uint32_t)i;
    rec.Rank = rule->Rank;
    rec.Tpl = (uint16_t)rule->Tpl;
    rec.Type = f.Type;
    rec.WidthM = rule->WidthM;
    rec.MinE = rec.MinN = 1e30f;
    rec.MaxE = rec.MaxN = -1e30f;
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = t.Field->Rings()[f.FirstRing + r];
      for (uint32_t k = 0; k < ring.Count; k++) {
        const float e = t.Pts[((size_t)ring.First + k) * 2];
        const float n = t.Pts[((size_t)ring.First + k) * 2 + 1];
        rec.MinE = std::min(rec.MinE, e); rec.MaxE = std::max(rec.MaxE, e);
        rec.MinN = std::min(rec.MinN, n); rec.MaxN = std::max(rec.MaxN, n);
      }
    }
    if (rec.MaxE < rec.MinE) continue;
    const float pad = rec.WidthM * 0.5f;
    rec.MinE -= pad; rec.MinN -= pad; rec.MaxE += pad; rec.MaxN += pad;
    t.Feats.push_back(rec);
  }
  t.FeatsDone = feats.size();
  std::stable_sort(t.Feats.begin(), t.Feats.end(),
                   [](const Feat &a, const Feat &b) { return a.Rank < b.Rank; });
  t.Stale = true;
}

/* One tier's whole structure, laid down feature by feature in ascending rank. The row sweep carries
 * an ACTIVE EDGE LIST, so a feature costs its own edges once per row it actually spans and not its
 * whole outline per row of its bounding box — without that, a z11 forest spanning twelve kilometres
 * would be rebuilt in tens of milliseconds and the walk would stutter every time the grid re-anchors. */
void ClassField::BuildTier(Tier &t, double camE, double camN) {
  Block &B = (&t == &Near_) ? NearB_ : FarB_;
  const int W = t.Half * 2, H = t.Half * 2;
  const double cell = t.CellM;
  B.W = W; B.H = H; B.CellM = cell;
  B.OrgE = std::floor(camE / cell - t.Half) * cell;
  B.OrgN = std::floor(camN / cell - t.Half) * cell;
  t.OrgE = B.OrgE;
  t.OrgN = B.OrgN;

  const int def = Veg_->DefaultTemplate();
  std::vector<uint8_t> base((size_t)W * H, 0xFF), baseRank((size_t)W * H, 0);
  /* Per-cell seed lists as an intrusive linked list over two flat arrays: a vector of vectors here
   * allocates once per cell and that alone was two thirds of the rebuild. */
  std::vector<int32_t> seedHead((size_t)W * H, -1);
  std::vector<int32_t> seedNext;
  std::vector<uint32_t> seedCount((size_t)W * H, 0);
  B.Seeds.clear();
  B.Refs.clear();
  B.Edges.clear();

  std::vector<float> ex;      /* this feature's edges: x0,y0,x1,y1 */
  std::vector<float> curve;   /* the way resampled along its Catmull-Rom, reused per feature */
  std::vector<float> side;    /* the stroked contour of one line feature, reused */
  std::vector<uint32_t> byY;  /* edge order by ymin */
  std::vector<uint32_t> act;
  /* The same for one feature's edges over the cells of its bounding box, with a VERSION STAMP so the
   * head array is never cleared between features. */
  std::vector<int32_t> ceHead;
  std::vector<uint32_t> ceStamp;
  std::vector<int32_t> ceNext;
  std::vector<uint32_t> ceEdge;
  std::vector<uint32_t> ceCount;
  uint32_t stamp = 0;
  struct Hit { double X; int Dir; };
  std::vector<Hit> hits;

  for (const Feat &f : t.Feats) {
    if (f.MaxE < B.OrgE || f.MinE > B.OrgE + W * cell) continue;
    if (f.MaxN < B.OrgN || f.MinN > B.OrgN + H * cell) continue;
    const OsmField::Feature &of = t.Field->Features()[f.Idx];

    ex.clear();
    if (f.Type == 3) {
      for (uint32_t k = 0; k < of.RingCount; k++) {
        const OsmField::Ring &ring = t.Field->Rings()[of.FirstRing + k];
        CurveRing(t.Pts.data(), ring.First, ring.Count, true, curve);
        const size_t nc = curve.size() / 2;
        for (size_t s = 0; s < nc; s++) {
          const size_t a = s, b = (s + 1) % nc;
          ex.push_back(curve[a * 2]); ex.push_back(curve[a * 2 + 1]);
          ex.push_back(curve[b * 2]); ex.push_back(curve[b * 2 + 1]);
        }
      }
    } else {
      /* A line is its CENTRELINE. Extruding it to a polygon makes the answer binary, and a way
       * narrower than a pixel is then hit whole or missed whole — 3073 fragments against the bake's
       * 31 (doc/render/classification.md). Half the width lives in the seed instead. */
      for (uint32_t k = 0; k < of.RingCount; k++) {
        const OsmField::Ring &ring = t.Field->Rings()[of.FirstRing + k];
        CurveRing(t.Pts.data(), ring.First, ring.Count, false, curve);
        const size_t nc = curve.size() / 2;
        for (size_t i = 0; i + 1 < nc; i++) {
          ex.push_back(curve[i * 2]);       ex.push_back(curve[i * 2 + 1]);
          ex.push_back(curve[(i + 1) * 2]); ex.push_back(curve[(i + 1) * 2 + 1]);
        }
      }
    }
    const size_t ne = ex.size() / 4;
    if (ne == 0) continue;

    const int i0 = std::max(0, (int)std::floor((f.MinE - B.OrgE) / cell));
    const int i1 = std::min(W - 1, (int)std::floor((f.MaxE - B.OrgE) / cell));
    const int j0 = std::max(0, (int)std::floor((f.MinN - B.OrgN) / cell));
    const int j1 = std::min(H - 1, (int)std::floor((f.MaxN - B.OrgN) / cell));
    if (i0 > i1 || j0 > j1) continue;
    const int bw = i1 - i0 + 1, bh = j1 - j0 + 1;

    stamp++;
    if (ceHead.size() < (size_t)bw * bh) {
      ceHead.resize((size_t)bw * bh, -1);
      ceStamp.resize((size_t)bw * bh, 0);
      ceCount.resize((size_t)bw * bh, 0);
    }
    ceNext.clear();
    ceEdge.clear();
    /* A centreline reaches half its width sideways, so the cells it must appear in reach that far too;
     * without the pad the fragment beside the axis never sees the edge it is measured against. */
    const float epad = (f.Type == 3) ? 0.0f : f.WidthM * 0.5f;
    for (size_t e = 0; e < ne; e++) {
      const float *p = &ex[e * 4];
      const int ei0 = std::max(i0, (int)std::floor((std::min(p[0], p[2]) - epad - B.OrgE) / cell));
      const int ei1 = std::min(i1, (int)std::floor((std::max(p[0], p[2]) + epad - B.OrgE) / cell));
      const int ej0 = std::max(j0, (int)std::floor((std::min(p[1], p[3]) - epad - B.OrgN) / cell));
      const int ej1 = std::min(j1, (int)std::floor((std::max(p[1], p[3]) + epad - B.OrgN) / cell));
      for (int j = ej0; j <= ej1; j++)
        for (int i = ei0; i <= ei1; i++) {
          const size_t c = (size_t)(j - j0) * bw + (size_t)(i - i0);
          if (ceStamp[c] != stamp) { ceStamp[c] = stamp; ceHead[c] = -1; ceCount[c] = 0; }
          ceNext.push_back(ceHead[c]);
          ceEdge.push_back((uint32_t)e);
          ceHead[c] = (int32_t)(ceNext.size() - 1);
          ceCount[c]++;
        }
    }

    byY.resize(ne);
    for (uint32_t e = 0; e < (uint32_t)ne; e++) byY[e] = e;
    std::sort(byY.begin(), byY.end(), [&ex](uint32_t a, uint32_t b) {
      return std::min(ex[(size_t)a * 4 + 1], ex[(size_t)a * 4 + 3]) <
             std::min(ex[(size_t)b * 4 + 1], ex[(size_t)b * 4 + 3]);
    });
    act.clear();
    size_t nextE = 0;

    for (int j = j0; j <= j1; j++) {
      const double cy = B.OrgN + (double)j * cell;
      while (nextE < ne) {
        const float *p = &ex[(size_t)byY[nextE] * 4];
        if ((double)std::min(p[1], p[3]) > cy) break;
        act.push_back(byY[nextE]);
        nextE++;
      }
      size_t keep = 0;
      for (size_t k = 0; k < act.size(); k++) {
        const float *p = &ex[(size_t)act[k] * 4];
        if ((double)std::max(p[1], p[3]) > cy) act[keep++] = act[k];
      }
      act.resize(keep);

      hits.clear();
      for (uint32_t e : act) {
        const float *p = &ex[(size_t)e * 4];
        if ((p[1] <= cy) == (p[3] <= cy)) continue;
        const double xi = (double)p[0] + (cy - (double)p[1]) * ((double)p[2] - (double)p[0]) /
                                             ((double)p[3] - (double)p[1]);
        hits.push_back(Hit{xi, p[3] > p[1] ? 1 : -1});
      }
      std::sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.X < b.X; });

      int wind = 0;
      size_t hi = 0;
      for (int i = i0; i <= i1; i++) {
        const double cx = B.OrgE + (double)i * cell;
        while (hi < hits.size() && hits[hi].X < cx) { wind += hits[hi].Dir; hi++; }
        const size_t bc = (size_t)(j - j0) * bw + (size_t)(i - i0);
        const uint32_t nce = ceStamp[bc] == stamp ? ceCount[bc] : 0u;
        const size_t ci = (size_t)j * W + (size_t)i;
        if (nce == 0 || seedCount[ci] >= (uint32_t)kSeedCap || nce > (uint32_t)kRefCap) {
          if (nce != 0) Overflow_++;
          /* An OPEN centreline has no inside, so its winding is not a coverage test — and the running
           * counter would carry it along the scanline. A line covers a cell only through its seed,
           * where the distance decides; without a seed it covers nothing. */
          if (f.Type == 3 && wind != 0) { base[ci] = (uint8_t)f.Tpl; baseRank[ci] = (uint8_t)f.Rank; }
          continue;
        }
        const uint32_t refFirst = (uint32_t)B.Refs.size();
        for (int32_t k = ceHead[bc]; k >= 0; k = ceNext[(size_t)k])
          B.Refs.push_back((uint32_t)(B.Edges.size() / 4) + ceEdge[(size_t)k]);
        seedNext.push_back(seedHead[ci]);
        seedHead[ci] = (int32_t)(B.Seeds.size() / 3);
        seedCount[ci]++;
        B.Seeds.push_back((uint32_t)f.Tpl | ((uint32_t)f.Rank << 8) | ((uint32_t)nce << 16) |
                          ((uint32_t)(uint8_t)(std::max(-128, std::min(127, wind)) + 128) << 24));
        B.Seeds.push_back(refFirst);
        {
          const float hw = (f.Type == 3) ? 0.0f : f.WidthM * 0.5f;
          uint32_t bits; std::memcpy(&bits, &hw, sizeof bits);
          B.Seeds.push_back(bits);
        }
      }
    }
    B.Edges.insert(B.Edges.end(), ex.begin(), ex.end());
  }

  /* The seeds of one cell must be contiguous, so they are re-emitted in cell order once every
   * feature has been laid down. */
  std::vector<uint32_t> seeds;
  seeds.reserve(B.Seeds.size());
  B.Cells.assign((size_t)W * H * 2, 0);
  for (size_t ci = 0; ci < (size_t)W * H; ci++) {
    const uint32_t first = (uint32_t)(seeds.size() / 3);
    for (int32_t s = seedHead[ci]; s >= 0; s = seedNext[(size_t)s]) {
      seeds.push_back(B.Seeds[(size_t)s * 3]);
      seeds.push_back(B.Seeds[(size_t)s * 3 + 1]);
      seeds.push_back(B.Seeds[(size_t)s * 3 + 2]);
    }
    B.Cells[ci * 2] = (uint32_t)base[ci] | ((uint32_t)baseRank[ci] << 8) |
                      ((uint32_t)seedCount[ci] << 16);
    B.Cells[ci * 2 + 1] = first;
  }
  B.Seeds.swap(seeds);
  (void)def;
  t.Have = true;
  t.Stale = false;

  /* THE FIRST OF THE THREE OUTCOMES, counted and never logged per occurrence: how much of this grid
   * OSM says nothing about. Sampled at the cell centres of the tier that was just built. */
  if (&t == &Near_) {
    Probe_ = 0;
    ProbeNoData_ = 0;
    for (int j = 0; j < H; j++)
      for (int i = 0; i < W; i++) {
        const double e = B.OrgE + ((double)i + 0.5) * cell, n = B.OrgN + ((double)j + 0.5) * cell;
        Probe_++;
        if (Evaluate(e, n, nullptr, nullptr) < 0) ProbeNoData_++;
      }
  }
}

void ClassField::Pack() {
  const int nb = 2;
  const uint32_t head = 4;
  const uint32_t hdrWords = 12;
  Buf_.assign(head + (uint32_t)nb * hdrWords, 0u);
  Buf_[2] = (uint32_t)Veg_->DefaultTemplate();
  const Block *blocks[2] = {&NearB_, &FarB_};
  for (int b = 0; b < nb; b++) {
    const Block &B = *blocks[b];
    const uint32_t h = head + (uint32_t)b * hdrWords;
    Buf_[(size_t)b] = B.W > 0 ? h : 0u;
    if (B.W == 0) continue;
    Buf_[h + 0] = (uint32_t)B.W;
    Buf_[h + 1] = (uint32_t)B.H;
    const float oe = (float)B.OrgE, on = (float)B.OrgN, cm = (float)B.CellM;
    std::memcpy(&Buf_[h + 2], &oe, 4);
    std::memcpy(&Buf_[h + 3], &on, 4);
    std::memcpy(&Buf_[h + 4], &cm, 4);
    Buf_[h + 5] = (uint32_t)Buf_.size();
    Buf_.insert(Buf_.end(), B.Cells.begin(), B.Cells.end());
    Buf_[h + 6] = (uint32_t)Buf_.size();
    Buf_.insert(Buf_.end(), B.Seeds.begin(), B.Seeds.end());
    Buf_[h + 7] = (uint32_t)Buf_.size();
    Buf_.insert(Buf_.end(), B.Refs.begin(), B.Refs.end());
    Buf_[h + 8] = (uint32_t)Buf_.size();
    const size_t at = Buf_.size();
    Buf_.resize(at + B.Edges.size());
    std::memcpy(&Buf_[at], B.Edges.data(), B.Edges.size() * 4);
  }
  Edges_ = (long)((NearB_.Edges.size() + FarB_.Edges.size()) / 4);
  Seeds_ = (long)((NearB_.Seeds.size() + FarB_.Seeds.size()) / 3);
  Dirty_ = true;
}

void ClassField::Update(double camLat, double camLon) {
  if (!Opened_ || !Veg_ || !Veg_->Ready()) return;
  /* THE LAYER SET IS THE DECLARATION'S, so it cannot be known before the table is loaded. */
  if (!Near_.Field) {
    Near_.Field = std::make_unique<OsmField>(Near_.Zoom, Veg_->Layers());
    Far_.Field = std::make_unique<OsmField>(Far_.Zoom, Veg_->AreaLayers());
  }
  Near_.Field->Build(camLat, camLon, Near_.Ring);
  Far_.Field->Build(camLat, camLon, Far_.Ring);
  Ingest(Near_);
  Ingest(Far_);

  double camE = 0, camN = 0;
  Project(camLat, camLon, &camE, &camN);
  Cam_[0] = camE;
  Cam_[1] = camN;

  const double t0 = Clock();
  bool rebuilt = false;
  for (Tier *t : {&Near_, &Far_}) {
    const double cx = t->OrgE + t->Half * t->CellM, cy = t->OrgN + t->Half * t->CellM;
    const bool drifted = t->Have && (std::fabs(camE - cx) > t->SlackM || std::fabs(camN - cy) > t->SlackM);
    if (!t->Have || t->Stale || drifted) {
      BuildTier(*t, camE, camN);
      rebuilt = true;
      break;   /* one tier per pass: a rebuild is the only unbudgeted work here */
    }
  }
  BuildMs_ = rebuilt ? Clock() - t0 : 0.0;
  if (BuildMs_ > BuildMsMax_) BuildMsMax_ = BuildMs_;
  if (rebuilt) Pack();
}

bool ClassField::Complete() const {
  return Opened_ && Near_.Field && Far_.Field && Near_.Field->PendingTiles() == 0 && Far_.Field->PendingTiles() == 0 &&
         Near_.Have && Far_.Have && !Near_.Stale && !Far_.Stale;
}

/* THE EVALUATOR. Identical in shape to the WGSL one: the cell's base class, then every seed whose
 * winding at the fragment says the feature contains it, highest declared rank wins. */
int ClassField::Evaluate(double e, double n, double *distM, int *runnerUp) const {
  int best = -1, bestRank = -1, second = -1, secondRank = -1;
  double bestDist = 1.0e30;
  const Block *blocks[2] = {&NearB_, &FarB_};
  for (int b = 0; b < 2; b++) {
    const Block &B = *blocks[b];
    if (B.W == 0) continue;
    const int i = (int)std::floor((e - B.OrgE) / B.CellM);
    const int j = (int)std::floor((n - B.OrgN) / B.CellM);
    if (i < 0 || j < 0 || i >= B.W || j >= B.H) continue;
    const size_t ci = ((size_t)j * B.W + (size_t)i) * 2;
    const uint32_t c0 = B.Cells[ci];
    const uint32_t nseed = (c0 >> 16) & 0xFF;
    const uint32_t seedFirst = B.Cells[ci + 1];
    if ((c0 & 0xFF) != 0xFF) { best = (int)(c0 & 0xFF); bestRank = (int)((c0 >> 8) & 0xFF); }
    const double cx = B.OrgE + (double)i * B.CellM, cy = B.OrgN + (double)j * B.CellM;
    for (uint32_t s = 0; s < nseed; s++) {
      const uint32_t w0 = B.Seeds[(seedFirst + s) * 3];
      const uint32_t refFirst = B.Seeds[(seedFirst + s) * 3 + 1];
      float halfW; std::memcpy(&halfW, &B.Seeds[(seedFirst + s) * 3 + 2], sizeof halfW);
      const int tpl = (int)(w0 & 0xFF);
      const int rank = (int)((w0 >> 8) & 0xFF);
      const uint32_t nref = (w0 >> 16) & 0xFF;
      int wind = (int)((w0 >> 24) & 0xFF) - 128;
      double d = 1.0e30;
      for (uint32_t r = 0; r < nref; r++) {
        const float *p = &B.Edges[(size_t)B.Refs[refFirst + r] * 4];
        if (halfW <= 0.0f) {
          wind += CrossX(p[0], p[1], p[2], p[3], cy, cx, e);
          wind += CrossY(p[0], p[1], p[2], p[3], e, cy, n);
        }
        d = std::min(d, SegDist(e, n, p[0], p[1], p[2], p[3]));
      }
      if (halfW > 0.0f) { if (d > halfW) continue; d = (double)halfW - d; }
      else if (wind == 0) continue;
      if (rank > bestRank) {
        second = best; secondRank = bestRank;
        best = tpl; bestRank = rank; bestDist = d;
      } else if (rank > secondRank) { second = tpl; secondRank = rank; }
    }
    if (best >= 0) break;   /* the finer tier answers alone where it has an answer */
  }
  if (distM) *distM = bestDist;
  if (runnerUp) *runnerUp = second;
  return best;
}

int ClassField::ClassAt(double lat, double lon) const { return ClassAt(lat, lon, nullptr, nullptr); }

int ClassField::ClassAt(double lat, double lon, double *distM, int *runnerUp) const {
  double e = 0, n = 0;
  Project(lat, lon, &e, &n);
  return Evaluate(e, n, distM, runnerUp);
}

} // namespace outshine::World
