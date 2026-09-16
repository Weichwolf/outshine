#include <expected>
#include <array>
#include "GroundStack.h"

#include <cstddef>
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

bool GroundStack::Open(const Roots &under,
                       std::span<const Scenario::Provider> providers,
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
                                                           size_t ingestTilesMost) {
  const auto vectorTile = ValidatePosition(at);
  if (!vectorTile) { return std::unexpected(vectorTile.error()); }
  if (!Pool_ || StandsAt(at)) { return {}; }
  const auto classified = Cls_.Update(*Pool_, at);
  if (!classified) { return std::unexpected(classified.error()); }
  Stood_ = at;
  Settled_ = false;
  if (!Vegetated_) { return {}; }
  if (!Vectors_) {
    const std::array<std::string, 5> layers = {{OsmLayerName(OsmLayer::Buildings),
                                                OsmLayerName(OsmLayer::WaterPolygons),
                                                OsmLayerName(OsmLayer::WaterLines),
                                                OsmLayerName(OsmLayer::Streets),
                                                OsmLayerName(OsmLayer::StreetPolygons)}};
    const int zoom = FinestZoomOf(Data::DataKind::VectorMap);
    if (zoom <= 0) { return {}; }
    Vectors_ = std::make_unique<OsmField>(zoom, std::span<const std::string>(layers));
    Footprints_.AnchorAt(Cls_.OriginEcef());
  }
  if (Declared_.empty()) {
    const auto built = Vectors_->Build(*Pool_, at, kVectorRing, kVectorTiles);
    if (!built) { return std::unexpected(built.error()); }
  } else {
    Vectors_->Declare(std::span<const OsmField::Declared>(Declared_), *vectorTile);
  }
  if (Vectors_->PendingTiles() > 0) { return {}; }
  for (size_t pass = 0; pass < ingestTilesMost; ++pass) {
    if (HeapBytes() > kHoldsBytes) {
      Settle();
      Settled_ = true;
      if (HeapBytes() > kHoldsBytes) {
        ++Overflowed_;
        Overflowing_ = true;
        break;
      }
    }
    const size_t before =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    (void)Ways_.Ingest(*Vectors_, Templates_);
    (void)WaterBodies_.Ingest(*Ground_, *Vectors_, Templates_);
    const size_t after =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    if (after == before || Drained()) { break; }
  }
  if (Drained() && !Settled_) {
    Settle();
    Settled_ = true;
  }
  return {};
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
