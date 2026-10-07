#include "ShippedProviders.h"
#include <algorithm>
#include <expected>
#include <array>
#include "SurfacePreparation.h"
#include "PreparedTerrainAssets.h"
#include "PreparedBuildingAssets.h"
#include "ReadTextFile.h"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <memory>
#include <utility>
#include <ratio>
#include <span>
#include <string_view>

#include <cmath>
#include <numbers>
#include <string>

#include "Sink.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

namespace {

constexpr int kGeographicTerrainRenderZoom = 14;

double ElapsedMs(std::chrono::steady_clock::time_point began) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
      .count();
}

}

std::unique_ptr<::outshine::Generators::Osm::OsmField>
SurfacePreparation::CreateVectorField() const {
  const std::array<std::string, 5> layers = {
      {::outshine::Generators::Osm::OsmLayerName(::outshine::Generators::Osm::OsmLayer::Buildings),
       ::outshine::Generators::Osm::OsmLayerName(
           ::outshine::Generators::Osm::OsmLayer::WaterPolygons),
       ::outshine::Generators::Osm::OsmLayerName(::outshine::Generators::Osm::OsmLayer::WaterLines),
       ::outshine::Generators::Osm::OsmLayerName(::outshine::Generators::Osm::OsmLayer::Streets),
       ::outshine::Generators::Osm::OsmLayerName(
           ::outshine::Generators::Osm::OsmLayer::StreetPolygons)}};
  return std::make_unique<::outshine::Generators::Osm::OsmField>(
      VectorZoom_, std::span<const std::string>(layers), VectorSchema_);
}

std::expected<void, std::string>
SurfacePreparation::BindRegionCache(const World::StoragePaths &under,
                                    const Data::SourceSet &sources) {
  if (under.AssetCache.empty()) { return {}; }
  const auto &assets = under.Shipped;
  constexpr size_t bytesMost = size_t{1024} * 1024u;
  auto materials = ReadTextFile(assets + "/world/ground-materials.json", bytesMost);
  auto vegetation = ReadTextFile(assets + "/world/vegetation.json", bytesMost);
  if (!materials || !vegetation) {
    return std::unexpected("could not bind the prepared ground region rules");
  }
  materials->push_back('\0');
  materials->append(*vegetation);
  auto regions =
      Generators::Osm::PreparedGroundRegions::Open(under.AssetCache, sources, *materials);
  if (!regions) { return std::unexpected(std::move(regions.error())); }
  PreparedRegions_ = std::move(*regions);
  return {};
}

bool SurfacePreparation::Open(const World::StoragePaths &under,
                              std::span<const Data::SourceProvider> providers,
                              LongitudeLatitude focus,
                              Data::Transport &wire,
                              Tasks &compute,
                              Sink &say,
                              LogSink *diagnostics,
                              double patienceS,
                              const Data::ProviderRegistry *registry) {
  const auto position = ::outshine::Generators::Osm::OsmField::Locate(focus, kFineZoom);
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
  Data::ProviderRegistry defaults;
  if (registry == nullptr) { Generators::RegisterShippedProviders(defaults); }
  const bool registered = outshine::Data::RegisterSources(
      sources, providers, under.Shipped, refused, registry != nullptr ? *registry : defaults);
  if (!registered) {
    say.Refuse(refused);
    Close();
    return false;
  }
  for (size_t at = 0; at < sources.Count(); ++at) {
    const auto &decl = sources.At(at).Declaration();
    if (decl.Kind != Data::DataKind::VectorMap) { continue; }
    const auto schema = Generators::Osm::ParseMvtSchema(decl.Schema);
    if (!schema || (HasVectorSource_ && *schema != VectorSchema_)) {
      say.Refuse("vector sources require one supported semantic schema");
      Close();
      return false;
    }
    HasVectorSource_ = true;
    VectorSchema_ = *schema;
  }
  VectorZoom_ = HasVectorSource_ ? FinestZoomOf(Data::DataKind::VectorMap) : kFineZoom;
  say.Number("sources registered", static_cast<double>(sources.Count()), "sources");

  outshine::Ground::GroundSurface surface;
  surface.Grid = outshine::Ground::kStreamGrid;
  surface.Z = FinestZoomOf(Data::DataKind::Elevation) - 1;
  auto poolConfig = *config;
  poolConfig.Compute = &compute;
  poolConfig.Diagnostics = diagnostics;
  if (!under.AssetCache.empty()) {
    auto prepared = Generators::PreparedTerrainAssets::Open(under.AssetCache, sources);
    if (!prepared) {
      say.Refuse(prepared.error());
      Close();
      return false;
    }
    auto buildings = Generators::PreparedBuildingAssets::Open(under.AssetCache, sources);
    if (!buildings) {
      say.Refuse(buildings.error());
      Close();
      return false;
    }
    auto network = Generators::Osm::PreparedStreetGraph::Open(under.AssetCache, sources);
    if (!network) {
      say.Refuse(network.error());
      Close();
      return false;
    }
    PreparedNetwork_ = std::move(*network);
    PreparedBuildings_ = std::move(*buildings);
    PreparedTerrain_ = std::move(*prepared);
    poolConfig.PreparedFields = [assets =
                                     PreparedTerrain_](Data::TileId at,
                                                       const TerrainTiles::Shaped &shape,
                                                       const TerrainTiles::FieldFactory &factory) {
      return assets->Resolve(at, shape, factory);
    };
  }
  Pool_ = std::make_unique<outshine::Ground::TilePool>(poolConfig, sources, wire);
  Ground_ = std::make_unique<outshine::Ground::GroundStream>(*Pool_, surface);
  SurfaceZoom_ = surface.Z;
  Cls_.SetVectorSource(HasVectorSource_, VectorSchema_);
  Cls_.Open(focus.LatitudeDeg, focus.LongitudeDeg, compute);

  const std::string &assets = under.Shipped;
  Vegetated_ = Materials_.Load((assets + "/world/ground-materials.json").c_str()) &&
               Templates_.Load((assets + "/world/vegetation.json").c_str(), Materials_);
  if (Vegetated_) {
    Cls_.SetVegetation(&Templates_);
    auto bound = BindRegionCache(under, sources);
    if (!bound) {
      say.Refuse(bound.error());
      Close();
      return false;
    }
  } else {
    say.Say(Line("REFUSED the shipped ground tables under %s did not load, so this world stands "
                 "with no vegetation and no vector features",
                 assets.c_str()));
  }
  Opened_ = true;
  return true;
}

void SurfacePreparation::Close() {
  Cls_.Close();
  Vectors_.reset();
  Ground_.reset();
  Pool_.reset();
  PreparedTerrain_.reset();
  PreparedBuildings_.reset();
  PreparedNetwork_.reset();
  PreparedRegions_.reset();
  LandingCursor_ = {};
  Sources_.reset();
  Store_.reset();
  HasVectorSource_ = false;
  VectorZoom_ = kFineZoom;
  VectorSchema_ = Generators::Osm::MvtSchema::Shortbread;
  Opened_ = false;
  Settled_.reset();
  LastAdvance_ = {};
  WorstAdvance_ = {};
}

int SurfacePreparation::FinestZoomOf(Data::DataKind kind) const {
  int finest = 0;
  if (!Sources_) { return finest; }
  for (size_t at = 0; at < Sources_->Count(); ++at) {
    const Data::SourceDecl &decl = Sources_->At(at).Declaration();
    if (decl.Kind != kind) { continue; }
    const int zoom = kind == Data::DataKind::Elevation && decl.How == Data::Scheme::GeographicCell
                         ? kGeographicTerrainRenderZoom
                         : decl.MaxZoom;
    finest = std::max(finest, zoom);
  }
  return finest;
}

std::expected<TileAt, std::string_view>
SurfacePreparation::ValidatePosition(LongitudeLatitude at) const {
  const auto fine = ::outshine::Generators::Osm::OsmField::Locate(at, kFineZoom);
  if (!fine) { return std::unexpected(fine.error()); }
  const auto coarse = ::outshine::Generators::Osm::OsmField::Locate(at, kCoarseZoom);
  if (!coarse) { return std::unexpected(coarse.error()); }
  return ::outshine::Generators::Osm::OsmField::Locate(at, VectorZoom_);
}

std::expected<void, std::string_view>
SurfacePreparation::AdvanceAt(LongitudeLatitude at, SurfacePreparationBudget budget) {
  const auto restandAt = std::chrono::steady_clock::now();
  SurfacePreparationMetrics metrics;
  const auto complete = [&] -> std::expected<void, std::string_view> {
    metrics.TotalMs = ElapsedMs(restandAt);
    RecordAdvance(metrics);
    return {};
  };
  const auto vectorTile = ValidatePosition(at);
  if (!vectorTile) { return std::unexpected(vectorTile.error()); }
  if (!Pool_) { return complete(); }
  const auto classificationAt = std::chrono::steady_clock::now();
  const auto classified = Cls_.Update(*Pool_, at);
  metrics.ClassificationMs = ElapsedMs(classificationAt);
  if (!classified) { return std::unexpected(classified.error()); }
  Stood_ = at;
  if (!Vegetated_) { return complete(); }
  const auto vectorAt = std::chrono::steady_clock::now();
  if (!Vectors_) {
    Vectors_ = CreateVectorField();
    Footprints_.AnchorAt(Cls_.OriginEcef());
  }
  const uint64_t previousVectorGeneration = Vectors_->Generation();
  if (HasVectorSource_ && Declared_.empty()) {
    const auto built = Vectors_->Build(
        *Pool_, at, budget.VectorRing, kVectorTiles, {.TilesMost = kVectorParseTilesPerAdvanceAt});
    if (!built) { return std::unexpected(built.error()); }
    metrics.VectorBuild = Vectors_->LastBuildMetrics();
  } else {
    Vectors_->Declare(std::span<const ::outshine::Generators::Osm::OsmField::Declared>(Declared_),
                      *vectorTile);
  }
  if (Vectors_->Generation() != previousVectorGeneration) {
    Footprints_.ResetDerived();
    Ways_ = ::outshine::Generators::Osm::StreetField{};
    WaterBodies_ = ::outshine::Generators::Osm::WaterField{};
  }
  metrics.VectorsMs = ElapsedMs(vectorAt);
  if (!Vectors_->SettledWithin(0)) { return complete(); }
  const auto classes = Cls_.Read();
  const SettlementInputs inputs{.Classes = classes ? classes->Version() : 0,
                                .Vectors = Vectors_->Generation(),
                                .Footprints = Footprints_.Revision()};
  if (CanReuseSettlement(inputs, budget.VectorRing)) { return complete(); }
  IngestLayers(inputs, budget, metrics);
  return complete();
}

void SurfacePreparation::IngestLayers(const SettlementInputs &inputs,
                                      SurfacePreparationBudget budget,
                                      SurfacePreparationMetrics &metrics) {
  Settled_.reset();
  for (size_t pass = 0; pass < budget.IngestTilesMost; ++pass) {
    if (HeapBytes() > kHoldsBytes) {
      const auto settleAt = std::chrono::steady_clock::now();
      Settle();
      metrics.SettlementMs += ElapsedMs(settleAt);
      Settled_ = inputs;
      if (HeapBytes() > kHoldsBytes) {
        ++Overflowed_;
        Overflowing_ = true;
        break;
      }
    }
    const size_t before =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    const auto streetsAt = std::chrono::steady_clock::now();
    const auto looked = Ways_.LookedCount();
    (void)Ways_.Ingest(*Vectors_, Templates_);
    metrics.IngestionUnits += static_cast<size_t>(Ways_.LookedCount() - looked);
    metrics.StreetsMs += ElapsedMs(streetsAt);
    const auto waterAt = std::chrono::steady_clock::now();
    (void)WaterBodies_.Ingest(*Ground_, *Vectors_, Templates_);
    metrics.IngestionUnits += WaterBodies_.LastIngest().AdvancedUnits;
    metrics.WaterMs += ElapsedMs(waterAt);
    const size_t after =
        Ways_.IngestedTiles() + WaterBodies_.IngestedTiles() + Footprints_.IngestedTiles();
    if (after != before) { Settled_.reset(); }
    if (after == before || Drained()) { break; }
  }
  if (Drained() && !Settled_) {
    const auto settleAt = std::chrono::steady_clock::now();
    Settle();
    metrics.SettlementMs += ElapsedMs(settleAt);
    Settled_ = inputs;
  }
}

bool SurfacePreparation::CanReuseSettlement(const SettlementInputs &inputs, int rings) const {
  return Settled_ == inputs && Cls_.Complete() && Vectors_->SettledWithin(rings) && Drained() &&
         HeapBytes() <= kHoldsBytes;
}

void SurfacePreparation::RecordAdvance(SurfacePreparationMetrics metrics) noexcept {
  LastAdvance_ = metrics;
  if (metrics.TotalMs > WorstAdvance_.TotalMs) { WorstAdvance_ = metrics; }
}

bool SurfacePreparation::AwaitProgress(double seconds) {
  if (seconds <= 0.0 || !Pool_) { return false; }
  if (Cls_.Building()) { return Cls_.AwaitBuild(seconds); }
  return Pool_->AwaitLanding(seconds, LandingCursor_);
}

void SurfacePreparation::Settle() {
  Cls_.Settle();
  Footprints_.Settle();
  Ways_.Settle();
  WaterBodies_.Settle();
  if (Vectors_) { Vectors_->Settle(); }
}

bool SurfacePreparation::Drained() const {
  if (!Vegetated_ || !Vectors_) { return true; }
  return Ways_.Ingested(*Vectors_) && WaterBodies_.Ingested(*Vectors_);
}

bool SurfacePreparation::Ingested() const {
  if (!Vegetated_ || !Vectors_) { return !Vegetated_; }
  return Vectors_->PendingTiles() <= 0 && Cls_.Complete() && Drained();
}

bool SurfacePreparation::IngestedWithin(int rings) const {
  if (!Vegetated_ || !Vectors_) { return !Vegetated_; }
  return Vectors_->SettledWithin(rings) && Cls_.Complete() &&
         Ways_.IngestedWithin(*Vectors_, rings) && WaterBodies_.IngestedWithin(*Vectors_, rings);
}

std::string SurfacePreparation::IngestionStatus() const {
  if (!Vectors_) { return "vectors=absent"; }
  return "streets=" + std::to_string(Ways_.IngestedTiles()) + "/" +
         std::to_string(static_cast<int>(Ways_.Ingested(*Vectors_))) +
         ", water=" + std::to_string(WaterBodies_.IngestedTiles()) + "/" +
         std::to_string(static_cast<int>(WaterBodies_.Ingested(*Vectors_))) +
         ", waterDeferrals=" + std::to_string(WaterBodies_.Deferrals()) +
         ", classes=" + std::to_string(static_cast<int>(Cls_.Complete()));
}

}
