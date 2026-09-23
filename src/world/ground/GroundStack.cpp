#include <expected>
#include <array>
#include "GroundStack.h"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <memory>
#include <ratio>
#include <span>
#include <string_view>

#include <cmath>
#include <numbers>
#include <string>

#include "Sink.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

bool GroundStack::Open(const World::StoragePaths &under,
                       std::span<const Data::SourceProvider> providers,
                       LongitudeLatitude focus,
                       Data::Transport &wire,
                       Sink &say,
                       LogSink *diagnostics,
                       double patienceS) {
  const auto position = OsmField::Locate(focus, kFineZoom);
  if (!position) {
    say.Refuse(std::string(position.error()));
    return false;
  }
  const auto config = GroundPoolConfig(focus, {.PatienceS = patienceS});
  if (!config) {
    say.Refuse(std::string(config.error()));
    return false;
  }
  Close();
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = under.Cache;
  Store_ = std::make_unique<outshine::Data::ContentStore>(keeping);
  Sources_ = std::make_unique<outshine::Data::SourceSet>(*Store_);
  outshine::Data::SourceSet &sources = *Sources_;
  std::string refused;
  const bool registered =
      outshine::Data::RegisterDeclared(sources, providers, under.Shipped + "/sky", refused);
  if (!registered) {
    say.Refuse(refused);
    Close();
    return false;
  }
  say.Number("sources registered", static_cast<double>(sources.Count()), "sources");

  outshine::Ground::GroundSurface surface;
  surface.Grid = outshine::Ground::kStreamGrid;
  surface.Z = FinestZoomOf(Data::DataKind::Elevation) - 1;
  auto poolConfig = *config;
  poolConfig.Diagnostics = diagnostics;
  Pool_ = std::make_unique<outshine::Ground::TilePool>(poolConfig, sources, wire);
  Ground_ = std::make_unique<outshine::Ground::GroundStream>(*Pool_, surface);
  SurfaceZoom_ = surface.Z;
  Cls_.Open(focus.LatitudeDeg, focus.LongitudeDeg);

  const std::string &assets = under.Shipped;
  Vegetated_ = Materials_.Load((assets + "/world/ground-materials.json").c_str()) &&
               Templates_.Load((assets + "/world/vegetation.json").c_str(), Materials_);
  if (Vegetated_) {
    Cls_.SetVegetation(&Templates_);
  } else {
    say.Say(Line("REFUSED the shipped ground tables under %s did not load, so this world stands "
                 "with no vegetation and no vector features",
                 assets.c_str()));
  }
  Opened_ = true;
  return true;
}

void GroundStack::Close() {
  Ground_.reset();
  Pool_.reset();
  Sources_.reset();
  Store_.reset();
  Opened_ = false;
  WorstRestand_ = {};
}

int GroundStack::FinestZoomOf(Data::DataKind kind) const {
  int finest = 0;
  if (!Sources_) { return finest; }
  for (size_t at = 0; at < Sources_->Count(); ++at) {
    const Data::SourceDecl &decl = Sources_->At(at).Declaration();
    if (decl.Kind != kind) { continue; }
    finest = decl.MaxZoom > finest ? decl.MaxZoom : finest;
  }
  return finest;
}

std::expected<TileAt, std::string_view> GroundStack::ValidatePosition(LongitudeLatitude at) const {
  const auto fine = OsmField::Locate(at, kFineZoom);
  if (!fine) { return std::unexpected(fine.error()); }
  const auto coarse = OsmField::Locate(at, kCoarseZoom);
  if (!coarse) { return std::unexpected(coarse.error()); }
  const int vectorZoom = Vectors_ ? Vectors_->Zoom() : FinestZoomOf(Data::DataKind::VectorMap);
  return OsmField::Locate(at, vectorZoom);
}

std::expected<void, std::string_view> GroundStack::Restand(LongitudeLatitude at,
                                                           RestandBudget budget) {
  const auto restandAt = std::chrono::steady_clock::now();
  RestandMetrics metrics;
  const auto elapsedMs = [](std::chrono::steady_clock::time_point began) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
        .count();
  };
  const auto complete = [&]() -> std::expected<void, std::string_view> {
    metrics.TotalMs = elapsedMs(restandAt);
    RecordsRestand(metrics);
    return {};
  };
  const auto vectorTile = ValidatePosition(at);
  if (!vectorTile) { return std::unexpected(vectorTile.error()); }
  if (!Pool_) { return complete(); }
  const auto classificationAt = std::chrono::steady_clock::now();
  const auto classified = Cls_.Update(*Pool_, at);
  metrics.ClassificationMs = elapsedMs(classificationAt);
  if (!classified) { return std::unexpected(classified.error()); }
  Stood_ = at;
  Settled_ = false;
  if (!Vegetated_) { return complete(); }
  const auto vectorAt = std::chrono::steady_clock::now();
  if (!Vectors_) {
    const std::array<std::string, 5> layers = {{OsmLayerName(OsmLayer::Buildings),
                                                OsmLayerName(OsmLayer::WaterPolygons),
                                                OsmLayerName(OsmLayer::WaterLines),
                                                OsmLayerName(OsmLayer::Streets),
                                                OsmLayerName(OsmLayer::StreetPolygons)}};
    const int zoom = FinestZoomOf(Data::DataKind::VectorMap);
    if (zoom <= 0) { return complete(); }
    Vectors_ = std::make_unique<OsmField>(zoom, std::span<const std::string>(layers));
    Footprints_.AnchorAt(Cls_.OriginEcef());
  }
  const uint64_t previousVectorGeneration = Vectors_->Generation();
  if (Declared_.empty()) {
    const auto built = Vectors_->Build(*Pool_, at, budget.VectorRing, kVectorTiles);
    if (!built) { return std::unexpected(built.error()); }
  } else {
    Vectors_->Declare(std::span<const OsmField::Declared>(Declared_), *vectorTile);
  }
  if (Vectors_->Generation() != previousVectorGeneration) {
    Footprints_.ResetDerived();
    Ways_ = StreetField{};
    WaterBodies_ = WaterField{};
  }
  metrics.VectorsMs = elapsedMs(vectorAt);
  if (!Vectors_->SettledWithin(0)) { return complete(); }
  for (size_t pass = 0; pass < budget.IngestTilesMost; ++pass) {
    if (HeapBytes() > kHoldsBytes) {
      const auto settleAt = std::chrono::steady_clock::now();
      Settle();
      metrics.SettlementMs += elapsedMs(settleAt);
      Settled_ = true;
      if (HeapBytes() > kHoldsBytes) {
        ++Overflowed_;
        Overflowing_ = true;
        break;
      }
    }
    const size_t before =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    const auto streetsAt = std::chrono::steady_clock::now();
    (void)Ways_.Ingest(*Vectors_, Templates_);
    metrics.StreetsMs += elapsedMs(streetsAt);
    const auto waterAt = std::chrono::steady_clock::now();
    (void)WaterBodies_.Ingest(*Ground_, *Vectors_, Templates_);
    metrics.WaterMs += elapsedMs(waterAt);
    const size_t after =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    if (after == before || Drained()) { break; }
  }
  if (Drained() && !Settled_) {
    const auto settleAt = std::chrono::steady_clock::now();
    Settle();
    metrics.SettlementMs += elapsedMs(settleAt);
    Settled_ = true;
  }
  return complete();
}

void GroundStack::RecordsRestand(RestandMetrics metrics) noexcept {
  if (metrics.TotalMs > WorstRestand_.TotalMs) { WorstRestand_ = metrics; }
}

bool GroundStack::AwaitProgress(double seconds) {
  if (seconds <= 0.0 || !Pool_) { return false; }
  if (Cls_.Building()) { return Cls_.AwaitBuild(seconds); }
  return Pool_->AwaitLanding(seconds);
}

void GroundStack::Settle() {
  Cls_.Settle();
  Footprints_.Settle();
  Ways_.Settle();
  WaterBodies_.Settle();
  if (Vectors_) { Vectors_->Settle(); }
}

bool GroundStack::Drained() const {
  if (!Vegetated_ || !Vectors_) { return true; }
  return Ways_.Ingested(*Vectors_) && WaterBodies_.Ingested(*Vectors_);
}

bool GroundStack::Ingested() const {
  if (!Vegetated_ || !Vectors_) { return !Vegetated_; }
  return Vectors_->PendingTiles() <= 0 && Cls_.Complete() && Drained();
}

bool GroundStack::IngestedWithin(int rings) const {
  if (!Vegetated_ || !Vectors_) { return !Vegetated_; }
  return Vectors_->SettledWithin(rings) && Cls_.Complete() &&
         Ways_.IngestedWithin(*Vectors_, rings) && WaterBodies_.IngestedWithin(*Vectors_, rings);
}

std::string GroundStack::IngestionStatus() const {
  if (!Vectors_) { return "vectors=absent"; }
  return "streets=" + std::to_string(Ways_.IngestedTiles()) + "/" +
         std::to_string(static_cast<int>(Ways_.Ingested(*Vectors_))) +
         ", water=" + std::to_string(WaterBodies_.IngestedTiles()) + "/" +
         std::to_string(static_cast<int>(WaterBodies_.Ingested(*Vectors_))) +
         ", waterDeferrals=" + std::to_string(WaterBodies_.Deferrals()) +
         ", classes=" + std::to_string(static_cast<int>(Cls_.Complete()));
}

}
