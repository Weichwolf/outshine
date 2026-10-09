#include <expected>
#include "math/Units.h"
#include "ClassificationPreparation.h"

#include "OsmLayer.h"

#include <array>
#include <algorithm>
#include <tuple>
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

namespace {

double Clock() {
  using namespace std::chrono;
  return static_cast<double>(
             duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) *
         kMsPerMicrosecond;
}

}

std::unique_ptr<Generators::Osm::OsmField> ClassificationPreparation::CreateField(int zoom) const {
  if (Veg_ == nullptr || (zoom != Fine_.Zoom && zoom != Coarse_.Zoom)) { return {}; }
  return std::make_unique<Generators::Osm::OsmField>(
      zoom, zoom == Fine_.Zoom ? Veg_->Layers() : Veg_->AreaLayers(), VectorSchema_, Prepared_);
}

void ClassificationPreparation::SetPreparedTiles(
    std::shared_ptr<Generators::Osm::PreparedOsmTiles> prepared) {
  Prepared_ = std::move(prepared);
}

void ClassificationPreparation::Open(double lat, double lon, Tasks &compute) {
  Close();
  Builder_ = std::make_unique<ClassificationBuild>(compute);
  ++FrameRevision_;
  Frame_ = TangentFrame::At({.LongitudeDeg = lon, .LatitudeDeg = lat});
  Opened_ = true;
}

void ClassificationPreparation::Close() {
  Builder_.reset();
  Submitted_.reset();
  Prepared_.reset();
  for (Tier *tier : {&Fine_, &Coarse_}) {
    tier->Field.reset();
    tier->Pts.clear();
    tier->Rings.clear();
    tier->Feats.clear();
    tier->Generation = 0;
    tier->PtsDone = tier->RingsDone = tier->FeatsDone = 0;
    tier->Have = false;
    tier->Stale = true;
    tier->ArraysLent = false;
  }
  const std::scoped_lock lock(Mu_);
  Published_ = {};
  Restored_.reset();
  Opened_ = false;
}

bool ClassificationPreparation::Restore(std::shared_ptr<const ClassStructure> classes) {
  if (!Opened_ || !classes || Submitted_ || Fine_.Field || Coarse_.Field ||
      classes->Frame().OriginEcef().Axis != Frame_.OriginEcef().Axis) {
    return false;
  }
  const auto fits = [](const ClassStructure::Grid &grid, const Tier &tier) {
    return grid.W == 2 * tier.HalfCells && grid.H == grid.W && grid.CellM == tier.CellM;
  };
  if (!fits(classes->Fine(), Fine_) || !fits(classes->Coarse(), Coarse_)) { return false; }
  const std::scoped_lock lock(Mu_);
  Restored_ = std::move(classes);
  Published_ = {.Classes = Restored_,
                .Upload = std::make_shared<const Render::GroundClassBuffer>(*Restored_)};
  return true;
}

void ClassificationPreparation::Tier::Settle() {
  if (Field) { Field->Settle(); }
  Pts.shrink_to_fit();
  Rings.shrink_to_fit();
  Feats.shrink_to_fit();
}

size_t ClassificationPreparation::Tier::HeapBytes() const {
  return (Field ? Field->HeapBytes() : 0) + CapacityBytes(Pts) + CapacityBytes(Rings) +
         CapacityBytes(Feats);
}

void ClassificationPreparation::Settle() {
  const std::scoped_lock lk(Mu_);
  if (Fine_.ArraysLent || Coarse_.ArraysLent) { return; }
  Fine_.Settle();
  Coarse_.Settle();
}

size_t ClassificationPreparation::HeapBytes() const {
  const std::scoped_lock lk(Mu_);
  return Fine_.HeapBytes() + Coarse_.HeapBytes() + (Builder_ ? Builder_->HeapBytes() : 0) +
         (Published_.Upload ? Published_.Upload->HeapBytes() : 0);
}

void ClassificationPreparation::Ingest(Tier &t) {
  if (t.ArraysLent) { return; }
  if (t.Field && t.Field->Generation() != t.Generation) {
    t.Generation = t.Field->Generation();
    t.PtsDone = 0;
    t.RingsDone = 0;
    t.FeatsDone = 0;
    t.Stale = true;
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

  const std::span<const ::outshine::Generators::Osm::OsmField::Ring> rings = t.Field->Rings();
  if (rings.size() > t.RingsDone) {
    t.Rings.resize(rings.size());
    for (size_t i = t.RingsDone; i < rings.size(); i++) {
      t.Rings[i] = Generators::ClassificationRasterizer::Ring{.First = rings[i].First,
                                                              .Count = rings[i].Count};
    }
    t.RingsDone = rings.size();
  }

  const std::span<const ::outshine::Generators::Osm::OsmField::Feature> feats = t.Field->Features();
  if (feats.size() <= t.FeatsDone) { return; }

  for (size_t i = t.FeatsDone; i < feats.size(); i++) { AppendFeature(t, feats[i]); }
  t.FeatsDone = feats.size();
  std::ranges::sort(t.Feats,
                    [](const Generators::ClassificationRasterizer::Feature &a,
                       const Generators::ClassificationRasterizer::Feature &b) {
                      return std::tie(a.Rank,
                                      a.MinE,
                                      a.MinN,
                                      a.MaxE,
                                      a.MaxN,
                                      a.ClassRow,
                                      a.Form,
                                      a.WidthM,
                                      a.RingCount) < std::tie(b.Rank,
                                                              b.MinE,
                                                              b.MinN,
                                                              b.MaxE,
                                                              b.MaxN,
                                                              b.ClassRow,
                                                              b.Form,
                                                              b.WidthM,
                                                              b.RingCount);
                    });
  t.Stale = true;
}

void ClassificationPreparation::AppendFeature(
    Tier &t, const ::outshine::Generators::Osm::OsmField::Feature &f) {
  const std::string_view layer = t.Field->LayerName(static_cast<int>(f.Layer));
  const std::string_view kind = t.Field->Str(f, "kind");
  const VegetationTemplates::Rule *rule = Veg_->Find(layer, kind);
  if (rule == nullptr) {
    std::string key(layer);
    key.append("/").append(kind);
    if (Unknown_.insert(key).second) {
      Log::Error(LogTag::World,
                 "class_unknown_kind",
                 {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
    }
    UnknownFeats_++;
    return;
  }

  if (t.Field->Num(f, "tunnel", 0.0) > 0.5) { return; }
  if (f.Type != 2 && f.Type != 3) { return; }
  if (f.Type == 2 && rule->WidthM <= 0.0f) {
    std::string key(layer);
    key.append("/").append(kind).append("#width");
    if (Unknown_.insert(key).second) {
      Log::Error(LogTag::World,
                 "class_line_without_width",
                 {{"layer", std::string(layer)}, {"kind", std::string(kind)}});
    }
    UnknownFeats_++;
    return;
  }

  Generators::ClassificationRasterizer::Feature rec{};
  rec.FirstRing = f.FirstRing;
  rec.RingCount = f.RingCount;
  rec.Rank = rule->Rank;
  rec.ClassRow = static_cast<uint16_t>(rule->Tpl);
  rec.Form = static_cast<Generators::ClassificationRasterizer::Shape>(f.Type);
  rec.WidthM = rule->WidthM;
  rec.MinE = rec.MinN = static_cast<float>(kBeyondAnyCoordinate);
  rec.MaxE = rec.MaxN = -static_cast<float>(kBeyondAnyCoordinate);
  for (uint32_t r = 0; r < f.RingCount; r++) {
    const Generators::ClassificationRasterizer::Ring &ring = t.Rings[f.FirstRing + r];
    for (uint32_t k = 0; k < ring.Count; k++) {
      const float e = t.Pts[(static_cast<size_t>(ring.First) + k) * 2];
      const float n = t.Pts[(static_cast<size_t>(ring.First) + k) * 2 + 1];
      rec.MinE = std::min(rec.MinE, e);
      rec.MaxE = std::max(rec.MaxE, e);
      rec.MinN = std::min(rec.MinN, n);
      rec.MaxN = std::max(rec.MaxN, n);
    }
  }
  if (rec.MaxE < rec.MinE) { return; }
  const float pad = rec.WidthM * 0.5f;
  rec.MinE -= pad;
  rec.MinN -= pad;
  rec.MaxE += pad;
  rec.MaxN += pad;
  t.Feats.push_back(rec);
}

ClassificationBuild::Job
ClassificationPreparation::LendTo(Tier &t, ClassGrain grain, double camE, double camN) {
  assert(!t.ArraysLent);
  ClassificationBuild::Job job;
  job.Grain = grain;
  job.Frame = Frame_;
  job.Source = SourceRevision();
  job.Raster.CamE = camE;
  job.Raster.CamN = camN;
  job.Raster.CellM = t.CellM;
  job.Raster.HalfCells = t.HalfCells;
  job.UnmappedRow = Veg_->UnmappedRow();
  job.Raster.Points = std::move(t.Pts);
  job.Raster.Rings = std::move(t.Rings);
  job.Raster.Features = std::move(t.Feats);
  t.ArraysLent = true;
  return job;
}

ClassificationBuild::SourceRevision ClassificationPreparation::SourceRevision() const {
  return {.Frame = FrameRevision_,
          .Fine = Fine_.Field->Generation(),
          .Coarse = Coarse_.Field->Generation()};
}

bool ClassificationPreparation::SubmitDue(double camE, double camN) {
  const std::array<ClassGrain, 2> order = {{ClassGrain::Fine, ClassGrain::Coarse}};
  for (ClassGrain grain : order) {
    Tier &t = TierOf(grain);
    const double cx = t.OrgE + t.HalfCells * t.CellM;
    const double cy = t.OrgN + t.HalfCells * t.CellM;
    const bool drifted =
        t.Have && (std::fabs(camE - cx) > t.SlackM || std::fabs(camN - cy) > t.SlackM);
    if (t.Have && !t.Stale && !drifted) { continue; }
    if (!Builder_->Submit(LendTo(t, grain, camE, camN))) { return false; }
    Submitted_ = grain;
    Submits_[grain == ClassGrain::Fine ? 0 : 1]++;

    t.OrgE = std::floor(camE / t.CellM - t.HalfCells) * t.CellM;
    t.OrgN = std::floor(camN / t.CellM - t.HalfCells) * t.CellM;
    t.Stale = false;
    return true;
  }
  return true;
}

bool ClassificationPreparation::HasSourceRequests() const noexcept {
  return Opened_ && !Restored_ && Veg_ != nullptr && Veg_->Ready() && HasVectorSource_ &&
         Declared_.empty();
}

std::expected<void, std::string_view> ClassificationPreparation::Update(TilePool &tiles,
                                                                        LongitudeLatitude at) {
  const auto fine = ::outshine::Generators::Osm::OsmField::Locate(at, Fine_.Zoom);
  if (!fine) { return std::unexpected(fine.error()); }
  const auto coarse = ::outshine::Generators::Osm::OsmField::Locate(at, Coarse_.Zoom);
  if (!coarse) { return std::unexpected(coarse.error()); }
  if (!Opened_ || (Veg_ == nullptr) || !Veg_->Ready()) { return {}; }

  if (Restored_) {
    const EastNorth cam = Project(at);
    Cam_[0] = cam.EastM;
    Cam_[1] = cam.NorthM;
    const auto covers = [cam](const ClassStructure::Grid &grid, const Tier &tier) {
      return std::fabs(cam.EastM - (grid.OrgE + tier.HalfCells * grid.CellM)) <= tier.SlackM &&
             std::fabs(cam.NorthM - (grid.OrgN + tier.HalfCells * grid.CellM)) <= tier.SlackM;
    };
    if (covers(Restored_->Fine(), Fine_) && covers(Restored_->Coarse(), Coarse_)) { return {}; }
    const std::scoped_lock lock(Mu_);
    Restored_.reset();
    Published_ = {};
  }

  if (!Fine_.Field) {
    Fine_.Field = CreateField(Fine_.Zoom);
    Coarse_.Field = CreateField(Coarse_.Zoom);
  }
  const double t0 = Clock();
  if (HasSourceRequests()) {
    const auto fineBuilt = Fine_.Field->Build(
        tiles, at, Fine_.TileRadius, (size_t{2} * kFineRings + 1) * (size_t{2} * kFineRings + 1));
    if (!fineBuilt) { return std::unexpected(fineBuilt.error()); }
    const auto coarseBuilt =
        Coarse_.Field->Build(tiles,
                             at,
                             Coarse_.TileRadius,
                             (size_t{2} * kCoarseRings + 1) * (size_t{2} * kCoarseRings + 1));
    if (!coarseBuilt) { return std::unexpected(coarseBuilt.error()); }
  } else {
    const std::span<const ::outshine::Generators::Osm::OsmField::Declared> these(Declared_);
    Fine_.Field->Declare(these, *fine);
    Coarse_.Field->Declare(these, *coarse);
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

  CollectFinished();

  if (!Submitted_ && Fine_.Field->PendingTiles() == 0 && Coarse_.Field->PendingTiles() == 0) {
    if (!SubmitDue(cam.EastM, cam.NorthM)) {
      return std::unexpected("classification compute queue is closed");
    }
  }
  return {};
}

void ClassificationPreparation::CollectFinished() {
  auto done = Builder_->Collect();
  if (!done) { return; }
  Tier &t = TierOf(done->Returned.Grain);
  const bool current =
      done->Returned.Source == SourceRevision() && done->Structure &&
      t.Generation == t.Field->Generation() && t.PtsDone == t.Field->Points().size() / 2 &&
      t.RingsDone == t.Field->Rings().size() && t.FeatsDone == t.Field->Features().size();
  t.Pts = std::move(done->Returned.Raster.Points);
  t.Rings = std::move(done->Returned.Raster.Rings);
  t.Feats = std::move(done->Returned.Raster.Features);
  t.ArraysLent = false;
  Submitted_.reset();
  BuildMsMax_ = std::max(done->BuildMs, BuildMsMax_);
  Ingest(t);
  if (!current) {
    t.Stale = true;
    return;
  }
  t.Have = true;
  if (!Fine_.Have || !Coarse_.Have || Fine_.Stale || Coarse_.Stale) { return; }
  const std::scoped_lock lk(Mu_);
  Published_ = {.Classes = std::move(done->Structure), .Upload = std::move(done->Upload)};
}

bool ClassificationPreparation::Complete() const {
  if (Opened_ && Restored_) { return true; }
  return Opened_ && Fine_.Field && Coarse_.Field && Fine_.Field->PendingTiles() == 0 &&
         Coarse_.Field->PendingTiles() == 0 && Fine_.Have && Coarse_.Have && !Fine_.Stale &&
         !Coarse_.Stale && !Submitted_;
}

}
