#include "math/Units.h"
#include "ClassField.h"

#include "OsmLayer.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <mutex>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "Capacity.h"
#include "Log.h"
#include "VegetationTemplates.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <string_view>
#include <string>
#include <utility>

namespace outshine::Ground {

constexpr double kMsPerMicrosecond = 1e-3;

constexpr double kBytesPerKB = 1024.0;

namespace {

double Clock() {
  using namespace std::chrono;
  return static_cast<double>(
             duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) *
         kMsPerMicrosecond;
}

} // namespace

void ClassField::Open(double lat, double lon) {
  Frame_ = TangentFrame::At({.LongitudeDeg = lon, .LatitudeDeg = lat});
  Opened_ = true;
}

void ClassField::Tier::Settle() {
  if (Field) { Field->Settle(); }
  Pts.shrink_to_fit();
  Rings.shrink_to_fit();
  Feats.shrink_to_fit();
}

size_t ClassField::Tier::HeapBytes() const {
  return (Field ? Field->HeapBytes() : 0) + CapacityBytes(Pts) + CapacityBytes(Rings) +
         CapacityBytes(Feats);
}

void ClassField::Settle() {
  const std::scoped_lock lk(Mu_);
  if (Fine_.ArraysLent || Coarse_.ArraysLent) { return; }
  Fine_.Settle();
  Coarse_.Settle();
}

size_t ClassField::HeapBytes() const {
  const std::scoped_lock lk(Mu_);
  return Fine_.HeapBytes() + Coarse_.HeapBytes() + Builder_.HeapBytes() +
         (Published_ ? Published_->Bytes() : 0);
}

void ClassField::Ingest(Tier &t) {
  if (t.ArraysLent) { return; }
  if (t.Field && t.Field->Generation() != t.Generation) {
    t.Generation = t.Field->Generation();
    t.PtsDone = 0;
    t.RingsDone = 0;
    t.FeatsDone = 0;
    t.Pts.clear();
    t.Rings.clear();
    t.Feats.clear();
  }
  const std::span<const double> pts = t.Field->Points();
  const size_t havePts = pts.size() / 2;
  if (havePts > t.PtsDone) {
    t.Pts.resize(havePts * 2);
    for (size_t i = t.PtsDone; i < havePts; i++) {
      const EastNorth on = Project({.LongitudeDeg = pts[i * 2 + 1], .LatitudeDeg = pts[i * 2]});
      t.Pts[i * 2] = static_cast<float>(on.EastM);
      t.Pts[i * 2 + 1] = static_cast<float>(on.NorthM);
    }
    t.PtsDone = havePts;
  }

  const std::span<const OsmField::Ring> rings = t.Field->Rings();
  if (rings.size() > t.RingsDone) {
    t.Rings.resize(rings.size());
    for (size_t i = t.RingsDone; i < rings.size(); i++) {
      t.Rings[i] = ClassBuilder::Ring{.First = rings[i].First, .Count = rings[i].Count};
    }
    t.RingsDone = rings.size();
  }

  const std::span<const OsmField::Feature> feats = t.Field->Features();
  if (feats.size() <= t.FeatsDone) { return; }

  for (size_t i = t.FeatsDone; i < feats.size(); i++) {
    const OsmField::Feature &f = feats[i];
    const std::string_view layer = t.Field->LayerName(static_cast<int>(f.Layer));
    const std::string_view kind = t.Field->Str(f, "kind");
    const VegetationTemplates::Rule *rule = Veg_->Find(layer, kind);
    if (rule == nullptr) {
      std::string key(layer);
      key.append("/").append(kind);
      if (Unknown_.insert(key).second) {
        Log::Error("world",
                   "class_unknown_kind",
                   {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
      }
      UnknownFeats_++;
      continue;
    }

    if (t.Field->Num(f, "tunnel", 0.0) > 0.5) { continue; }
    if (f.Type != 2 && f.Type != 3) { continue; }
    if (f.Type == 2 && rule->WidthM <= 0.0f) {
      std::string key(layer);
      key.append("/").append(kind).append("#width");
      if (Unknown_.insert(key).second) {
        Log::Error("world",
                   "class_line_without_width",
                   {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
      }
      UnknownFeats_++;
      continue;
    }

    ClassBuilder::Feature rec{};
    rec.FirstRing = f.FirstRing;
    rec.RingCount = f.RingCount;
    rec.Rank = rule->Rank;
    rec.Tpl = static_cast<uint16_t>(rule->Tpl);
    rec.Form = static_cast<ClassBuilder::Shape>(f.Type);
    rec.WidthM = rule->WidthM;
    rec.MinE = rec.MinN = static_cast<float>(kBeyondAnyCoordinate);
    rec.MaxE = rec.MaxN = -static_cast<float>(kBeyondAnyCoordinate);
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const ClassBuilder::Ring &ring = t.Rings[f.FirstRing + r];
      for (uint32_t k = 0; k < ring.Count; k++) {
        const float e = t.Pts[(static_cast<size_t>(ring.First) + k) * 2];
        const float n = t.Pts[(static_cast<size_t>(ring.First) + k) * 2 + 1];
        rec.MinE = std::min(rec.MinE, e);
        rec.MaxE = std::max(rec.MaxE, e);
        rec.MinN = std::min(rec.MinN, n);
        rec.MaxN = std::max(rec.MaxN, n);
      }
    }
    if (rec.MaxE < rec.MinE) { continue; }
    const float pad = rec.WidthM * 0.5f;
    rec.MinE -= pad;
    rec.MinN -= pad;
    rec.MaxE += pad;
    rec.MaxN += pad;
    t.Feats.push_back(rec);
  }
  t.FeatsDone = feats.size();
  std::ranges::stable_sort(t.Feats,

                           [](const ClassBuilder::Feature &a, const ClassBuilder::Feature &b) {
                             return a.Rank < b.Rank;
                           });
  t.Stale = true;
}

ClassBuilder::Job ClassField::LendTo(Tier &t, ClassGrain grain, double camE, double camN) {
  assert(!t.ArraysLent);
  ClassBuilder::Job job;
  job.Grain = grain;
  job.Frame = Frame_;
  job.CamE = camE;
  job.CamN = camN;
  job.CellM = t.CellM;
  job.HalfCells = t.HalfCells;
  job.UnmappedRow = Veg_->UnmappedRow();
  job.Pts = std::move(t.Pts);
  job.Rings = std::move(t.Rings);
  job.Feats = std::move(t.Feats);
  t.ArraysLent = true;
  return job;
}

void ClassField::SubmitDue(double camE, double camN) {
  const std::array<ClassGrain, 2> order = {{ClassGrain::Fine, ClassGrain::Coarse}};
  for (ClassGrain grain : order) {
    Tier &t = TierOf(grain);
    const double cx = t.OrgE + t.HalfCells * t.CellM;
    const double cy = t.OrgN + t.HalfCells * t.CellM;
    const bool drifted =
        t.Have && (std::fabs(camE - cx) > t.SlackM || std::fabs(camN - cy) > t.SlackM);
    if (t.Have && !t.Stale && !drifted) { continue; }
    Builder_.Submit(LendTo(t, grain, camE, camN));
    Submitted_ = grain;
    Submits_[grain == ClassGrain::Fine ? 0 : 1]++;

    t.OrgE = std::floor(camE / t.CellM - t.HalfCells) * t.CellM;
    t.OrgN = std::floor(camN / t.CellM - t.HalfCells) * t.CellM;
    t.Stale = false;
    return;
  }
}

void ClassField::Update(TilePool &tiles, LongitudeLatitude at) {
  if (!Opened_ || (Veg_ == nullptr) || !Veg_->Ready()) { return; }

  if (!Fine_.Field) {
    Fine_.Field = std::make_unique<OsmField>(Fine_.Zoom, Veg_->Layers());
    Coarse_.Field = std::make_unique<OsmField>(Coarse_.Zoom, Veg_->AreaLayers());
  }
  const double t0 = Clock();
  if (Declared_.empty()) {
    (void)Fine_.Field->Build(tiles, at, Fine_.TileRadius);
    (void)Coarse_.Field->Build(tiles, at, Coarse_.TileRadius);
  } else {
    const std::span<const OsmField::Declared> these(Declared_);
    Fine_.Field->Declare(these, at.LongitudeDeg, at.LatitudeDeg);
    Coarse_.Field->Declare(these, at.LongitudeDeg, at.LatitudeDeg);
  }
  const double t1 = Clock();
  Ingest(Fine_);
  Ingest(Coarse_);
  const double t2 = Clock();
  StreamMs_ = t1 - t0;
  IngestMs_ = t2 - t1;

  const EastNorth cam = Project(at);
  Cam_[0] = cam.EastM;
  Cam_[1] = cam.NorthM;

  if (std::optional<ClassBuilder::Handback> done = Builder_.Collect()) {
    Tier &t = TierOf(done->Returned.Grain);
    t.Pts = std::move(done->Returned.Pts);
    t.Rings = std::move(done->Returned.Rings);
    t.Feats = std::move(done->Returned.Feats);
    t.ArraysLent = false;
    t.Have = true;
    Submitted_.reset();
    const double buildMs = done->Structure->Measured().BuildMs;
    BuildMsMax_ = std::max(buildMs, BuildMsMax_);
    Log::Debug("world",
               "class_built",
               {{"version", static_cast<double>(done->Structure->Version())},
                {"buildMs", buildMs},
                {"packMs", done->Structure->Measured().PackMs},
                {"bufferKB", static_cast<double>(done->Structure->Bytes()) / kBytesPerKB},
                {"lentKB",
                 static_cast<double>(CapacityBytes(t.Pts) + CapacityBytes(t.Rings) +
                                     CapacityBytes(t.Feats)) /
                     kBytesPerKB}});
    const std::scoped_lock lk(Mu_);
    Published_ = std::move(done->Structure);
  }

  if (!Submitted_) { SubmitDue(cam.EastM, cam.NorthM); }
}

bool ClassField::Complete() const {
  return Opened_ && Fine_.Field && Coarse_.Field && Fine_.Field->PendingTiles() == 0 &&
         Coarse_.Field->PendingTiles() == 0 && Fine_.Have && Coarse_.Have && !Fine_.Stale &&
         !Coarse_.Stale && !Submitted_;
}

} // namespace outshine::Ground
