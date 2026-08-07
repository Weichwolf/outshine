#include "ClassField.h"

#include "Geodesy.h"
#include "Log.h"
#include "VegetationTemplates.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace outshine::World {

namespace {

double Clock() {
  using namespace std::chrono;
  return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() * 1e-3;
}

double TexelM(int level) { return ClassField::kTexel0M * (double)(1 << level); }

int FloorDiv(double v, double d) { return (int)std::floor(v / d); }

}  // namespace

void ClassField::Open(double lat, double lon) {
  GeoToEcef(lat, lon, 0.0, O_);
  double up[3];
  EnuAxesEcef(lat, lon, East_, North_, up);
  for (Level &l : Levels_) {
    l.Ids.assign((size_t)kSide * kSide * kChannels, 0);
    l.Wts.assign((size_t)kSide * kSide * kChannels, 0);
  }
  Opened_ = true;
}

/* The SAME projection the ground fragment does: the plane spanned by the ENU axes at the field
 * origin, dotted with the ECEF offset. Not a formula that resembles it — the shader is handed these
 * very axes, so a point cannot land in two places. */
void ClassField::Project(double lat, double lon, double *e, double *n) const {
  double p[3];
  GeoToEcef(lat, lon, 0.0, p);
  const double d[3] = {p[0] - O_[0], p[1] - O_[1], p[2] - O_[2]};
  *e = d[0] * East_[0] + d[1] * East_[1] + d[2] * East_[2];
  *n = d[0] * North_[0] + d[1] * North_[1] + d[2] * North_[2];
}

void ClassField::Ingest(Source &src) {
  const std::vector<double> &pts = src.Field.Points();
  const size_t havePts = pts.size() / 2;
  if (havePts > src.PtsDone) {
    src.Pts.resize(havePts * 2);
    for (size_t i = src.PtsDone; i < havePts; i++) {
      double e = 0, n = 0;
      Project(pts[i * 2], pts[i * 2 + 1], &e, &n);
      src.Pts[i * 2] = (float)e;
      src.Pts[i * 2 + 1] = (float)n;
    }
    src.PtsDone = havePts;
  }

  const std::vector<OsmField::Feature> &feats = src.Field.Features();
  if (feats.size() <= src.FeatsDone) return;

  double dMinE = 1e30, dMinN = 1e30, dMaxE = -1e30, dMaxN = -1e30;
  bool any = false;
  for (size_t i = src.FeatsDone; i < feats.size(); i++) {
    const OsmField::Feature &f = feats[i];
    const std::string_view kind = src.Field.Str(f, "kind");
    const int li = (int)f.Layer;
    const std::string_view layer = src.Field.LayerName(li);
    const VegetationTemplates::Rule *rule = Veg_->Find(layer, kind);
    if (!rule) {
      std::string key(layer);
      key.append("/").append(kind);
      if (Unknown_.insert(key).second)
        Log::Error("world", "class_unknown_kind", {{"layer", std::string(layer)},
                                                   {"kind", std::string(kind)}});
      UnknownFeats_++;
      continue;
    }
    if (f.Type != 2 && f.Type != 3) continue;
    if (f.Type == 2 && rule->WidthM <= 0.0f) {
      std::string key(layer);
      key.append("/").append(kind).append("/width");
      if (Unknown_.insert(key).second)
        Log::Error("world", "class_line_without_width", {{"layer", std::string(layer)},
                                                         {"kind", std::string(kind)}});
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
      const OsmField::Ring &ring = src.Field.Rings()[f.FirstRing + r];
      for (uint32_t k = 0; k < ring.Count; k++) {
        const float e = src.Pts[((size_t)ring.First + k) * 2];
        const float n = src.Pts[((size_t)ring.First + k) * 2 + 1];
        rec.MinE = std::min(rec.MinE, e);
        rec.MaxE = std::max(rec.MaxE, e);
        rec.MinN = std::min(rec.MinN, n);
        rec.MaxN = std::max(rec.MaxN, n);
      }
    }
    if (rec.MaxE < rec.MinE) continue;   /* a feature with no point at all */
    const float pad = rec.WidthM * 0.5f + 1.0f;
    rec.MinE -= pad; rec.MinN -= pad; rec.MaxE += pad; rec.MaxN += pad;
    src.Feats.push_back(rec);
    dMinE = std::min(dMinE, (double)rec.MinE); dMaxE = std::max(dMaxE, (double)rec.MaxE);
    dMinN = std::min(dMinN, (double)rec.MinN); dMaxN = std::max(dMaxN, (double)rec.MaxN);
    any = true;
  }
  src.FeatsDone = feats.size();
  std::stable_sort(src.Feats.begin(), src.Feats.end(),
                   [](const Feat &a, const Feat &b) { return a.Rank < b.Rank; });

  if (!any) return;
  const bool near = &src == &Near_;
  for (int lv = 0; lv < kLevels; lv++)
    if ((lv < kNearLevels) == near) PushDirty(lv, dMinE, dMinN, dMaxE, dMaxN);
}

void ClassField::PushDirty(int level, double minE, double minN, double maxE, double maxN) {
  Level &L = Levels_[level];
  if (!L.Have) return;   /* the whole window is queued already */
  const double t = TexelM(level);
  Rect r{FloorDiv(minE, t), FloorDiv(minN, t), FloorDiv(maxE, t) + 1, FloorDiv(maxN, t) + 1};
  r.I0 = std::max(r.I0, L.OriginI);
  r.J0 = std::max(r.J0, L.OriginJ);
  r.I1 = std::min(r.I1, L.OriginI + kSide);
  r.J1 = std::min(r.J1, L.OriginJ + kSide);
  if (r.I0 >= r.I1 || r.J0 >= r.J1) return;
  L.Dirty.push_back(r);
}

void ClassField::Scroll(int level, double camE, double camN) {
  Level &L = Levels_[level];
  const double t = TexelM(level);
  const int ni = FloorDiv(camE, t) - kSide / 2;
  const int nj = FloorDiv(camN, t) - kSide / 2;
  if (!L.Have) {
    L.OriginI = ni;
    L.OriginJ = nj;
    L.Have = true;
    L.Dirty.clear();
    L.Dirty.push_back(Rect{ni, nj, ni + kSide, nj + kSide});
    return;
  }
  const int oi = L.OriginI, oj = L.OriginJ;
  if (ni == oi && nj == oj) return;
  L.OriginI = ni;
  L.OriginJ = nj;
  if (std::abs(ni - oi) >= kSide || std::abs(nj - oj) >= kSide) {
    L.Dirty.clear();
    L.Dirty.push_back(Rect{ni, nj, ni + kSide, nj + kSide});
    return;
  }
  if (ni > oi) L.Dirty.push_back(Rect{oi + kSide, nj, ni + kSide, nj + kSide});
  else if (ni < oi) L.Dirty.push_back(Rect{ni, nj, oi, nj + kSide});
  const int xo0 = std::max(ni, oi), xo1 = std::min(ni, oi) + kSide;
  if (xo0 < xo1) {
    if (nj > oj) L.Dirty.push_back(Rect{xo0, oj + kSide, xo1, nj + kSide});
    else if (nj < oj) L.Dirty.push_back(Rect{xo0, nj, xo1, oj});
  }
}

/* Nonzero winding over EVERY ring of the feature at once, which is what makes a multipolygon hole a
 * hole: MVT declares interior rings by their winding, so counting the crossings signed is the whole
 * rule and no ring has to be classified first. */
void ClassField::Flush(uint8_t v) {
  if (Edges_.empty()) return;
  const size_t ne = Edges_.size() / 5;
  static thread_local std::vector<uint32_t> order;
  order.resize(ne);
  for (uint32_t i = 0; i < ne; i++) order[i] = i;
  std::sort(order.begin(), order.end(), [this](uint32_t a, uint32_t b) {
    return Edges_[(size_t)a * 5 + 2] < Edges_[(size_t)b * 5 + 2];
  });

  float yMin = 1e30f, yMax = -1e30f;
  for (size_t i = 0; i < ne; i++) {
    yMin = std::min(yMin, Edges_[i * 5 + 2]);
    yMax = std::max(yMax, Edges_[i * 5 + 3]);
  }
  int row = std::max(0, (int)std::floor(yMin - 0.5f) + 1);
  const int rowEnd = std::min(SubH_, (int)std::floor(yMax - 0.5f) + 1);

  static thread_local std::vector<uint32_t> active;
  active.clear();
  size_t next = 0;
  while (next < ne && Edges_[(size_t)order[next] * 5 + 2] < (float)row + 0.5f) {
    if (Edges_[(size_t)order[next] * 5 + 3] > (float)row + 0.5f) active.push_back(order[next]);
    next++;
  }

  for (; row < rowEnd; row++) {
    const float yc = (float)row + 0.5f;
    while (next < ne && Edges_[(size_t)order[next] * 5 + 2] < yc) {
      active.push_back(order[next]);
      next++;
    }
    size_t w = 0;
    for (size_t i = 0; i < active.size(); i++)
      if (Edges_[(size_t)active[i] * 5 + 3] > yc) active[w++] = active[i];
    active.resize(w);
    if (active.empty()) continue;

    Xs_.clear();
    Dirs_.clear();
    for (uint32_t ei : active) {
      const float *e = &Edges_[(size_t)ei * 5];
      Xs_.push_back(e[0] + (yc - e[2]) * e[1]);
      Dirs_.push_back((int)e[4]);
    }
    static thread_local std::vector<uint32_t> xo;
    xo.resize(Xs_.size());
    for (uint32_t i = 0; i < (uint32_t)Xs_.size(); i++) xo[i] = i;
    std::sort(xo.begin(), xo.end(), [this](uint32_t a, uint32_t b) { return Xs_[a] < Xs_[b]; });

    int wind = 0;
    uint8_t *dst = &Sub_[(size_t)row * (size_t)SubW_];
    for (size_t i = 0; i + 1 < xo.size(); i++) {
      wind += Dirs_[xo[i]];
      if (wind == 0) continue;
      int x0 = (int)std::floor(Xs_[xo[i]] - 0.5f) + 1;
      int x1 = (int)std::floor(Xs_[xo[i + 1]] - 0.5f) + 1;
      x0 = std::max(x0, 0);
      x1 = std::min(x1, SubW_);
      if (x1 > x0) std::memset(dst + x0, v, (size_t)(x1 - x0));
    }
  }
  Edges_.clear();
}

void ClassField::PaintRing(const float *pts, uint32_t first, uint32_t count, uint8_t) {
  if (count < 3) return;
  const double inv = 1.0 / SubStep_;
  float px = 0, py = 0, fx = 0, fy = 0;
  for (uint32_t k = 0; k <= count; k++) {
    const uint32_t idx = first + (k == count ? 0 : k);
    const float x = (float)(((double)pts[(size_t)idx * 2] - SubOrgE_) * inv);
    const float y = (float)(((double)pts[(size_t)idx * 2 + 1] - SubOrgN_) * inv);
    if (k == 0) { px = fx = x; py = fy = y; continue; }
    if (y != py) {
      const float y0 = std::min(py, y), y1 = std::max(py, y);
      const float dxdy = (x - px) / (y - py);
      Edges_.push_back(py < y ? px : x);
      Edges_.push_back(dxdy);
      Edges_.push_back(y0);
      Edges_.push_back(y1);
      Edges_.push_back(y > py ? 1.0f : -1.0f);
    }
    px = x; py = y;
  }
  (void)fx; (void)fy;
}

void ClassField::PaintQuad(const float q[8], uint8_t) {
  const double inv = 1.0 / SubStep_;
  for (int k = 0; k < 4; k++) {
    const int a = k, b = (k + 1) & 3;
    const float x0 = (float)(((double)q[a * 2] - SubOrgE_) * inv);
    const float y0 = (float)(((double)q[a * 2 + 1] - SubOrgN_) * inv);
    const float x1 = (float)(((double)q[b * 2] - SubOrgE_) * inv);
    const float y1 = (float)(((double)q[b * 2 + 1] - SubOrgN_) * inv);
    if (y0 == y1) continue;
    Edges_.push_back(y0 < y1 ? x0 : x1);
    Edges_.push_back((x1 - x0) / (y1 - y0));
    Edges_.push_back(std::min(y0, y1));
    Edges_.push_back(std::max(y0, y1));
    Edges_.push_back(y1 > y0 ? 1.0f : -1.0f);
  }
}

void ClassField::Raster(int level, const Rect &r) {
  const Source &src = level < kNearLevels ? Near_ : Far_;
  const double t = TexelM(level);
  const int tw = r.I1 - r.I0, th = r.J1 - r.J0;
  SubW_ = tw * kSub;
  SubH_ = th * kSub;
  SubStep_ = t / (double)kSub;
  SubOrgE_ = (double)r.I0 * t;
  SubOrgN_ = (double)r.J0 * t;
  Sub_.assign((size_t)SubW_ * (size_t)SubH_, 0);

  const double e0 = SubOrgE_, n0 = SubOrgN_, e1 = e0 + tw * t, n1 = n0 + th * t;
  for (const Feat &f : src.Feats) {
    if (f.MaxE < e0 || f.MinE > e1 || f.MaxN < n0 || f.MinN > n1) continue;
    const OsmField::Feature &of = src.Field.Features()[f.Idx];
    const uint8_t v = (uint8_t)(f.Tpl + 1);
    if (f.Type == 3) {
      for (uint32_t k = 0; k < of.RingCount; k++) {
        const OsmField::Ring &ring = src.Field.Rings()[of.FirstRing + k];
        PaintRing(src.Pts.data(), ring.First, ring.Count, v);
      }
      Flush(v);
    } else {
      const float hw = f.WidthM * 0.5f;
      for (uint32_t k = 0; k < of.RingCount; k++) {
        const OsmField::Ring &ring = src.Field.Rings()[of.FirstRing + k];
        for (uint32_t s = 0; s + 1 < ring.Count; s++) {
          const float ax = src.Pts[((size_t)ring.First + s) * 2];
          const float ay = src.Pts[((size_t)ring.First + s) * 2 + 1];
          const float bx = src.Pts[((size_t)ring.First + s + 1) * 2];
          const float by = src.Pts[((size_t)ring.First + s + 1) * 2 + 1];
          float dx = bx - ax, dy = by - ay;
          const float len = std::sqrt(dx * dx + dy * dy);
          if (len < 1.0e-6f) continue;
          dx /= len; dy /= len;
          /* The cap is extended by half the width so a bend closes: a mitre would need the joint
           * geometry and a notch at every corner is what it costs not to. */
          const float ex = ax - dx * hw, ey = ay - dy * hw;
          const float gx = bx + dx * hw, gy = by + dy * hw;
          const float px = -dy * hw, py = dx * hw;
          const float q[8] = {ex + px, ey + py, gx + px, gy + py, gx - px, gy - py, ex - px, ey - py};
          PaintQuad(q, v);
          Flush(v);
        }
      }
    }
  }

  /* THE REDUCTION: the coverage of every class in the texel, the four largest kept and renormalised.
   * A subcell no way covers is the DECLARED default and is counted — it is the truth about OSM and
   * not a fault, so it never produces a log line of its own. */
  const int def = Veg_->DefaultTemplate();
  Level &L = Levels_[level];
  uint16_t id[kSub * kSub];
  uint16_t cnt[kSub * kSub];
  for (int tj = 0; tj < th; tj++) {
    for (int ti = 0; ti < tw; ti++) {
      int n = 0, nodata = 0;
      for (int sy = 0; sy < kSub; sy++) {
        const uint8_t *row = &Sub_[(size_t)(tj * kSub + sy) * (size_t)SubW_ + (size_t)(ti * kSub)];
        for (int sx = 0; sx < kSub; sx++) {
          const int v = row[sx];
          if (v == 0) { nodata++; continue; }
          int k = 0;
          for (; k < n; k++) if (id[k] == v - 1) break;
          if (k == n) { id[n] = (uint16_t)(v - 1); cnt[n] = 0; n++; }
          cnt[k]++;
        }
      }
      SubNoData_ += nodata;
      SubTotal_ += kSub * kSub;
      if (nodata > 0) {
        int k = 0;
        for (; k < n; k++) if (id[k] == (uint16_t)def) break;
        if (k == n) { id[n] = (uint16_t)def; cnt[n] = 0; n++; }
        cnt[k] = (uint16_t)(cnt[k] + nodata);
      }
      for (int a = 0; a < n && a < kChannels; a++) {
        int best = a;
        for (int b = a + 1; b < n; b++) if (cnt[b] > cnt[best]) best = b;
        std::swap(id[a], id[best]);
        std::swap(cnt[a], cnt[best]);
      }
      const int keep = n < kChannels ? n : kChannels;
      int total = 0;
      for (int a = 0; a < keep; a++) total += cnt[a];
      const size_t o = ((size_t)((r.J0 + tj) & (kSide - 1)) * kSide +
                        (size_t)((r.I0 + ti) & (kSide - 1))) * kChannels;
      int acc = 0;
      for (int a = 0; a < kChannels; a++) {
        const int w = a < keep ? (int)((cnt[a] * 255 + total / 2) / total) : 0;
        L.Ids[o + (size_t)a] = a < keep ? (uint8_t)id[a] : 0;
        L.Wts[o + (size_t)a] = (uint8_t)w;
        acc += w;
      }
      L.Wts[o] = (uint8_t)std::max(0, std::min(255, (int)L.Wts[o] + 255 - acc));
    }
  }
  Texels_ += (long)tw * th;
  Written_.push_back(Chunk{level, r.I0 & (kSide - 1), r.J0 & (kSide - 1), tw, th});
}

void ClassField::Update(double camLat, double camLon) {
  Written_.clear();
  if (!Opened_ || !Veg_ || !Veg_->Ready()) return;

  Near_.Field.Build(camLat, camLon, Near_.Ring);
  Far_.Field.Build(camLat, camLon, Far_.Ring);
  Project(camLat, camLon, &CamE_, &CamN_);
  Ingest(Near_);
  Ingest(Far_);
  for (int lv = 0; lv < kLevels; lv++) Scroll(lv, CamE_, CamN_);

  const double t0 = Clock();
  int budget = kTexelBudget;
  /* Finest level first: what is under the walker's feet must not wait behind sixteen kilometres of
   * far field. */
  for (int lv = 0; lv < kLevels && budget > 0; lv++) {
    Level &L = Levels_[lv];
    while (budget > 0 && !L.Dirty.empty()) {
      Rect r = L.Dirty.back();
      L.Dirty.pop_back();
      r.I0 = std::max(r.I0, L.OriginI);
      r.J0 = std::max(r.J0, L.OriginJ);
      r.I1 = std::min(r.I1, L.OriginI + kSide);
      r.J1 = std::min(r.J1, L.OriginJ + kSide);
      if (r.I0 >= r.I1 || r.J0 >= r.J1) continue;
      /* Split so the piece never crosses the toroidal seam and never exceeds one scratch grid. */
      const int wrapI = ((r.I0 & (kSide - 1)) == 0) ? kSide : kSide - (r.I0 & (kSide - 1));
      int w = std::min(std::min(r.I1 - r.I0, wrapI), kChunkSide);
      if (w < r.I1 - r.I0) { L.Dirty.push_back(Rect{r.I0 + w, r.J0, r.I1, r.J1}); r.I1 = r.I0 + w; }
      const int wrapJ = ((r.J0 & (kSide - 1)) == 0) ? kSide : kSide - (r.J0 & (kSide - 1));
      int h = std::min(std::min(r.J1 - r.J0, wrapJ), std::min(kChunkSide, std::max(1, kChunkTexels / w)));
      if (h < r.J1 - r.J0) { L.Dirty.push_back(Rect{r.I0, r.J0 + h, r.I1, r.J1}); r.J1 = r.J0 + h; }
      Raster(lv, r);
      budget -= w * h;
    }
  }
  RasterMs_ = Clock() - t0;
}

bool ClassField::Complete() const {
  if (!Opened_) return false;
  if (Near_.Field.PendingTiles() != 0 || Far_.Field.PendingTiles() != 0) return false;
  for (const Level &l : Levels_)
    if (!l.Have || !l.Dirty.empty()) return false;
  return true;
}

bool ClassField::WeightsAt(int level, double lat, double lon, uint8_t ids[kChannels],
                           uint8_t wts[kChannels]) const {
  if (level < 0 || level >= kLevels || !Levels_[level].Have) return false;
  double e = 0, n = 0;
  Project(lat, lon, &e, &n);
  const double t = TexelM(level);
  const int gi = FloorDiv(e, t), gj = FloorDiv(n, t);
  const Level &L = Levels_[level];
  if (gi < L.OriginI || gi >= L.OriginI + kSide || gj < L.OriginJ || gj >= L.OriginJ + kSide)
    return false;
  const size_t o = ((size_t)(gj & (kSide - 1)) * kSide + (size_t)(gi & (kSide - 1))) * kChannels;
  for (int a = 0; a < kChannels; a++) { ids[a] = L.Ids[o + (size_t)a]; wts[a] = L.Wts[o + (size_t)a]; }
  return true;
}

int ClassField::DominantAt(int level, double lat, double lon) const {
  uint8_t ids[kChannels], wts[kChannels];
  if (!WeightsAt(level, lat, lon, ids, wts)) return -1;
  int best = 0;
  for (int a = 1; a < kChannels; a++) if (wts[a] > wts[best]) best = a;
  return (int)ids[best];
}

void ClassField::WindowFrac(float out[kLevels * 2]) const {
  for (int lv = 0; lv < kLevels; lv++) {
    const double t = TexelM(lv);
    out[lv * 2] = (float)(CamE_ / t - (double)Levels_[lv].OriginI);
    out[lv * 2 + 1] = (float)(CamN_ / t - (double)Levels_[lv].OriginJ);
  }
}

} // namespace outshine::World
