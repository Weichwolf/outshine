#include "ClassField.h"

#include "Geodesy.h"
#include "Log.h"
#include "VegetationTemplates.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>

namespace outshine::World {

namespace {

double Clock() {
  using namespace std::chrono;
  return (double)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count() * 1e-3;
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
  /* The local tangent plane: over the 900 m a scatter reaches, its error stays under a metre, and the
   * height it asks for has a 47 m posting of its own. */
  constexpr double kMPerDeg = 111320.0;
  *lat = Lat0_ + n / kMPerDeg;
  *lon = Lon0_ + e / (kMPerDeg * std::cos(Lat0_ * 3.14159265358979 / 180.0));
}

void ClassField::Ingest(Grain &g) {
  /* The builder has this grain's arrays. Deferring costs a pass; OsmField keeps what arrived and
   * PtsDone/FeatsDone make the catch-up exact. */
  if (g.Lent) return;
  const std::vector<double> &pts = g.Field->Points();
  const size_t havePts = pts.size() / 2;
  if (havePts > g.PtsDone) {
    g.Pts.resize(havePts * 2);
    for (size_t i = g.PtsDone; i < havePts; i++) {
      double e = 0, n = 0;
      Project(pts[i * 2], pts[i * 2 + 1], &e, &n);
      g.Pts[i * 2] = (float)e;
      g.Pts[i * 2 + 1] = (float)n;
    }
    g.PtsDone = havePts;
  }

  const std::vector<OsmField::Ring> &rings = g.Field->Rings();
  if (rings.size() > g.RingsDone) {
    g.Rings.resize(rings.size());
    for (size_t i = g.RingsDone; i < rings.size(); i++)
      g.Rings[i] = ClassBuilder::Ring{rings[i].First, rings[i].Count};
    g.RingsDone = rings.size();
  }

  const std::vector<OsmField::Feature> &feats = g.Field->Features();
  if (feats.size() <= g.FeatsDone) return;

  for (size_t i = g.FeatsDone; i < feats.size(); i++) {
    const OsmField::Feature &f = feats[i];
    const std::string_view layer = g.Field->LayerName((int)f.Layer);
    const std::string_view kind = g.Field->Str(f, "kind");
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
    if (g.Field->Num(f, "tunnel", 0.0) > 0.5) continue;
    if (f.Type != 2 && f.Type != 3) continue;
    if (f.Type == 2 && rule->WidthM <= 0.0f) {   /* the source's line code */
      std::string key(layer);
      key.append("/").append(kind).append("#width");
      if (Unknown_.insert(key).second)
        Log::Error("world", "class_line_without_width",
                   {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
      UnknownFeats_++;
      continue;
    }

    ClassBuilder::Feature rec{};
    rec.FirstRing = f.FirstRing;
    rec.RingCount = f.RingCount;
    rec.Rank = rule->Rank;
    rec.Tpl = (uint16_t)rule->Tpl;
    rec.Form = (ClassBuilder::Shape)f.Type;
    rec.WidthM = rule->WidthM;
    rec.MinE = rec.MinN = 1e30f;
    rec.MaxE = rec.MaxN = -1e30f;
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const ClassBuilder::Ring &ring = g.Rings[f.FirstRing + r];
      for (uint32_t k = 0; k < ring.Count; k++) {
        const float e = g.Pts[((size_t)ring.First + k) * 2];
        const float n = g.Pts[((size_t)ring.First + k) * 2 + 1];
        rec.MinE = std::min(rec.MinE, e); rec.MaxE = std::max(rec.MaxE, e);
        rec.MinN = std::min(rec.MinN, n); rec.MaxN = std::max(rec.MaxN, n);
      }
    }
    if (rec.MaxE < rec.MinE) continue;
    const float pad = rec.WidthM * 0.5f;
    rec.MinE -= pad; rec.MinN -= pad; rec.MaxE += pad; rec.MaxN += pad;
    g.Feats.push_back(rec);
  }
  g.FeatsDone = feats.size();
  std::stable_sort(g.Feats.begin(), g.Feats.end(),
                   [](const ClassBuilder::Feature &a, const ClassBuilder::Feature &b) {
                     return a.Rank < b.Rank;
                   });
  g.Stale = true;
}

/* LENT, NOT COPIED. The three arrays move into the job and come back with the result, so nothing is
 * duplicated and nothing is allocated: the tier simply does not have them while the grid is laid
 * down, which is why no rule is needed against reading them. */
ClassBuilder::Job ClassField::LendTo(Grain &g, ClassBuilder::Resolution res, double camE,
                                    double camN) {
  assert(!g.Lent);
  ClassBuilder::Job job;
  job.Grain = res;
  job.CamE = camE;
  job.CamN = camN;
  job.CellM = g.CellM;
  job.HalfCells = g.HalfCells;
  job.DefaultTemplate = Veg_->DefaultTemplate();
  job.Pts = std::move(g.Pts);
  job.Rings = std::move(g.Rings);
  job.Feats = std::move(g.Feats);
  g.Lent = true;
  return job;
}

bool ClassField::SubmitDue(double camE, double camN) {
  const ClassBuilder::Resolution order[2] = {ClassBuilder::Resolution::Fine,
                                             ClassBuilder::Resolution::Coarse};
  for (ClassBuilder::Resolution res : order) {
    Grain &g = GrainOf(res);
    const double cx = g.OrgE + g.HalfCells * g.CellM, cy = g.OrgN + g.HalfCells * g.CellM;
    const bool drifted = g.Have && (std::fabs(camE - cx) > g.SlackM || std::fabs(camN - cy) > g.SlackM);
    if (g.Have && !g.Stale && !drifted) continue;
    Builder_.Submit(LendTo(g, res, camE, camN));
    Submitted_ = res;
    Submits_[res == ClassBuilder::Resolution::Fine ? 0 : 1]++;
    /* The anchor is settled here rather than when the grid lands, so the drift test cannot ask for
     * the same grid again while the one it already asked for is still being laid down. */
    g.OrgE = std::floor(camE / g.CellM - g.HalfCells) * g.CellM;
    g.OrgN = std::floor(camN / g.CellM - g.HalfCells) * g.CellM;
    g.Stale = false;
    return true;
  }
  return false;
}

void ClassField::Update(double camLat, double camLon) {
  if (!Opened_ || !Veg_ || !Veg_->Ready()) return;
  /* THE LAYER SET IS THE DECLARATION'S, so it cannot be known before the table is loaded. */
  if (!Fine_.Field) {
    Fine_.Field = std::make_unique<OsmField>(Fine_.Zoom, Veg_->Layers());
    Coarse_.Field = std::make_unique<OsmField>(Coarse_.Zoom, Veg_->AreaLayers());
  }
  const double t0 = Clock();
  Fine_.Field->Build(camLat, camLon, Fine_.Ring);
  Coarse_.Field->Build(camLat, camLon, Coarse_.Ring);
  const double t1 = Clock();
  Ingest(Fine_);
  Ingest(Coarse_);
  const double t2 = Clock();
  StreamMs_ = t1 - t0;
  IngestMs_ = t2 - t1;

  double camE = 0, camN = 0;
  Project(camLat, camLon, &camE, &camN);
  Cam_[0] = camE;
  Cam_[1] = camN;

  if (std::optional<ClassBuilder::Handback> done = Builder_.Collect()) {
    Grain &g = GrainOf(done->Done.Grain);
    g.Pts = std::move(done->Done.Pts);
    g.Rings = std::move(done->Done.Rings);
    g.Feats = std::move(done->Done.Feats);
    g.Lent = false;
    g.Have = true;
    Submitted_.reset();
    const double buildMs = done->Structure->Measured().BuildMs;
    if (buildMs > BuildMsMax_) BuildMsMax_ = buildMs;
    Log::Debug("world", "class_built", {{"version", (double)done->Structure->Version()},
        {"buildMs", buildMs}, {"packMs", done->Structure->Measured().PackMs},
        {"bufferKB", done->Structure->Bytes() / 1024.0},
        {"lentKB", (double)(g.Pts.capacity() * sizeof(float) +
                            g.Rings.capacity() * sizeof(ClassBuilder::Ring) +
                            g.Feats.capacity() * sizeof(ClassBuilder::Feature)) / 1024.0}});
    std::lock_guard<std::mutex> lk(Mu_);
    Published_ = std::move(done->Structure);
  }
  /* NOT "is the builder busy": that answer is already in the past by the time it is acted on, and a
   * worker finishing in that window would let the next submit lend a tier its own uncollected,
   * already moved-out arrays. This one is render-thread-local and cannot race. */
  if (!Submitted_) SubmitDue(camE, camN);
}

bool ClassField::Complete() const {
  return Opened_ && Fine_.Field && Coarse_.Field &&
         Fine_.Field->PendingTiles() == 0 && Coarse_.Field->PendingTiles() == 0 &&
         Fine_.Have && Coarse_.Have && !Fine_.Stale && !Coarse_.Stale && !Submitted_;
}

} // namespace outshine::World
