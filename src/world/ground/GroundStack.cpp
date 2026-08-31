#include <chrono>
#include "GroundStack.h"

#include <memory>
#include <ratio>
#include <span>
#include <string_view>

#include <cmath>
#include <string>

#include "Sink.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

constexpr int kVectorRing = 3;

bool GroundStack::Open(std::string_view cacheDir,
                       std::string_view assetsDir,
                       std::span<const Provider> providers,
                       double focusLat,
                       double focusLon,
                       Data::Transport &wire,
                       Sink &say,
                       double patienceS) {
  Close();
  const bool onTheBand = std::fabs(focusLat) <= kMercatorLatMaxDeg;
  say.Number("the focus latitude the stack was asked for", focusLat, "deg");
  say.Number("the furthest the tiling reaches", kMercatorLatMaxDeg, "deg");
  if (!onTheBand) {
    say.Say("REFUSED the focus is off the tiling band");
    return false;
  }
  outshine::Data::ContentStore::Config keeping;
  keeping.Directory = std::string(cacheDir);
  Store_ = std::make_unique<outshine::Data::ContentStore>(keeping);
  Sources_ = std::make_unique<outshine::Data::SourceSet>(*Store_);
  outshine::Data::SourceSet &sources = *Sources_;
  std::string refused;
  const bool registered = outshine::Data::RegisterDeclared(
      sources, providers, std::string(assetsDir) + "/sky", refused);
  if (!registered) {
    say.Say(Line("REFUSED %s", refused.c_str()));
    Close();
    return false;
  }
  say.Number("sources registered", (double)sources.Count(), "sources");

  outshine::Ground::GroundSurface surface;
  surface.Grid = outshine::Ground::kStreamGrid;
  surface.Z = FinestZoomOf(Data::DataKind::Elevation) - 1;
  Pool_ = std::make_unique<outshine::Ground::TilePool>(
      outshine::Ground::GroundPoolConfig(focusLat, focusLon, 0, patienceS), sources, wire);
  Ground_ = std::make_unique<outshine::Ground::GroundStream>(*Pool_, surface);
  SurfaceZoom_ = surface.Z;
  Cls_.Open(focusLat, focusLon);

  const std::string assets(assetsDir);
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

void GroundStack::Restand(double lat, double lon, double budgetMs) {
  if (!Pool_) { return; }
  Cls_.Update(*Pool_, lat, lon, budgetMs);
  if (!Vegetated_) { return; }
  if (!Vectors_) {
    const std::string layers[] = {OsmLayerName(OsmLayer::Buildings),
                                  OsmLayerName(OsmLayer::WaterPolygons),
                                  OsmLayerName(OsmLayer::WaterLines),
                                  OsmLayerName(OsmLayer::Streets),
                                  OsmLayerName(OsmLayer::StreetPolygons)};
    const int zoom = FinestZoomOf(Data::DataKind::VectorMap);
    if (zoom <= 0) { return; }
    Vectors_ = std::make_unique<OsmField>(zoom, std::span<const std::string>(layers, 5));
    WaterBodies_.AnchorAt(Cls_.OriginEcef());
    Footprints_.AnchorAt(Cls_.OriginEcef());
  }
  (void)Vectors_->Build(*Pool_, lat, lon, kVectorRing, budgetMs);
  const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
  for (;;) {
    if (HeapBytes() > kHoldsBytes) {
      ++Overflowed_;
      break;
    }
    const size_t before =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    (void)Ways_.Ingest(*Vectors_, Templates_);
    (void)WaterBodies_.Ingest(*Ground_, *Vectors_, Templates_);
    (void)Footprints_.Build(*Ground_, *Vectors_, Span<const WayLine>());
    const size_t after =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    if (after == before || Drained()) { break; }
    if (budgetMs > 0.0 &&
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
                .count() >= budgetMs) {
      break;
    }
  }
}

bool GroundStack::Drained() const {
  if (!Vegetated_ || !Vectors_) { return true; }
  return Ways_.Ingested(*Vectors_) && WaterBodies_.Ingested(*Vectors_) &&
         Footprints_.Ingested(*Vectors_);
}

bool GroundStack::Ingested() const {
  if (!Vegetated_ || !Vectors_) { return !Vegetated_; }
  return Vectors_->PendingTiles() <= 0 && Cls_.Complete() && Drained();
}

} // namespace outshine::Ground
