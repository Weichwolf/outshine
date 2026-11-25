#include <algorithm>
#include <expected>
#include <array>
#include "SurfacePreparation.h"
#include "PreparedTerrainAssets.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStreetGraph.h"
#include "PreparedGroundRegions.h"
#include "PreparedRoadPages.h"
#include "PreparedUrbanAreas.h"
#include "ReadTextFile.h"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>
#include <filesystem>
#include <utility>
#include <ratio>
#include <span>
#include <string_view>

#include <cmath>
#include <numbers>
#include <string>

#include "Log.h"
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
      VectorZoom_, std::span<const std::string>(layers), VectorSchema_, PreparedVectors_);
}

std::expected<void, std::string>
SurfacePreparation::BindRegionCache(const ContentDirectories &under,
                                    const Data::SourceSet &sources) {
  if (under.Assets.empty()) { return {}; }
  const auto &builtinData = under.BuiltinData;
  constexpr size_t bytesMost = size_t{1024} * 1024u;
  auto materials = ReadTextFile(builtinData + "/world/ground-materials.json", bytesMost);
  auto vegetation = ReadTextFile(builtinData + "/world/vegetation.json", bytesMost);
  if (!materials || !vegetation) {
    return std::unexpected("could not bind the prepared ground region rules");
  }
  materials->insert(0, "ground-palette-layers-1\n");
  materials->push_back('\0');
  materials->append(*vegetation);
  materials->push_back('\0');
  materials->append(PreparedBuildings_->Recipe());
  auto regions = Generators::Osm::PreparedGroundRegions::Open(AssetStore_, sources, *materials);
  if (!regions) { return std::unexpected(std::move(regions.error())); }
  PreparedRegions_ = std::move(*regions);
  return {};
}

std::expected<void, std::string>
SurfacePreparation::BindAssetCaches(const std::string &directory,
                                    Data::SourceTransport &transport,
                                    Tasks &compute,
                                    const AssetCache::Runtime &storage) {
  const auto &sources = Sources_->Sources();
  auto store =
      AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string(), storage);
  if (!store) { return std::unexpected("could not open the native asset store"); }
  std::shared_ptr<AssetCache> shared = std::move(*store);
  auto terrain = Generators::PreparedTerrainAssets::Open(shared, sources);
  if (!terrain) { return std::unexpected(std::move(terrain.error())); }
  auto buildings = Generators::PreparedBuildingAssets::Open(shared, sources);
  if (!buildings) { return std::unexpected(std::move(buildings.error())); }
  auto network = Generators::Osm::PreparedStreetGraph::Open(shared, sources);
  if (!network) { return std::unexpected(std::move(network.error())); }
  auto vectors = Generators::Osm::PreparedOsmTiles::Open(shared, Sources_, transport, compute);
  if (!vectors) { return std::unexpected(std::move(vectors.error())); }
  auto urban = Generators::Osm::PreparedUrbanAreas::Open(shared, sources, compute, *vectors);
  if (!urban) { return std::unexpected(std::move(urban.error())); }
  auto roads = ::outshine::Generators::PreparedRoadPages::Open(shared);
  if (!roads) { return std::unexpected(std::move(roads.error())); }
  PreparedRoads_ = std::move(*roads);
  AssetStore_ = std::move(shared);
  PreparedTerrain_ = std::move(*terrain);
  PreparedBuildings_ = std::move(*buildings);
  PreparedNetwork_ = std::move(*network);
  PreparedVectors_ = std::move(*vectors);
  PreparedUrban_ = std::move(*urban);
  return {};
}

std::expected<void, std::string>
SurfacePreparation::Open(const ContentDirectories &under,
                         std::span<const Data::SourceProvider> providers,
                         const Data::ProviderRegistry &registry,
                         LongitudeLatitude focus,
                         Data::SourceTransport &wire,
                         Tasks &compute,
                         LogSink *diagnostics,
                         double patienceS,
                         const AssetCache::Runtime &storage) {
  const auto position = Generators::Osm::OsmField::Locate(focus, kFineZoom);
  if (!position) { return std::unexpected(std::string(position.error())); }
  const auto config = GroundPoolConfig(focus, {.PatienceS = patienceS});
  if (!config) { return std::unexpected(std::string(config.error())); }
  auto sources = Data::SourceSession::Open(
      {.Directory = under.Sources}, providers, under.BuiltinData, registry);
  if (!sources) { return std::unexpected(sources.error()); }
  return OpenSession(
      under, std::move(*sources), focus, wire, compute, diagnostics, patienceS, storage);
}

std::expected<void, std::string>
SurfacePreparation::OpenSession(const ContentDirectories &under,
                                std::shared_ptr<Data::SourceSession> session,
                                LongitudeLatitude focus,
                                Data::SourceTransport &wire,
                                Tasks &compute,
                                LogSink *diagnostics,
                                double patienceS,
                                const AssetCache::Runtime &storage) {
  if (!session) { return std::unexpected("surface preparation requires configured sources"); }
  const auto position = ::outshine::Generators::Osm::OsmField::Locate(focus, kFineZoom);
  if (!position) { return std::unexpected(std::string(position.error())); }
  const auto config = GroundPoolConfig(focus, {.PatienceS = patienceS});
  if (!config) { return std::unexpected(std::string(config.error())); }
  auto digest = Generators::Osm::OsmField::DeclarationDigest({});
  if (!digest) { return std::unexpected(std::string(digest.error())); }

  SurfacePreparation candidate;
  candidate.Sources_ = std::move(session);
  auto &sources = candidate.Sources_->Sources();
  for (size_t at = 0; at < sources.Count(); ++at) {
    const auto &decl = sources.At(at).Declaration();
    if (decl.Kind != Data::DataKind::VectorMap) { continue; }
    const auto schema = Generators::Osm::ParseMvtSchema(decl.Schema);
    if (!schema || (candidate.HasVectorSource_ && *schema != candidate.VectorSchema_)) {
      return std::unexpected("vector sources require one supported semantic schema");
    }
    candidate.HasVectorSource_ = true;
    candidate.VectorSchema_ = *schema;
  }
  candidate.VectorZoom_ =
      candidate.HasVectorSource_ ? candidate.FinestZoomOf(Data::DataKind::VectorMap) : kFineZoom;
  if (!under.Assets.empty()) {
    if (auto bound = candidate.BindAssetCaches(under.Assets, wire, compute, storage); !bound) {
      return std::unexpected(std::move(bound.error()));
    }
  }
  const std::string &builtinData = under.BuiltinData;
  candidate.RecipesReady_ =
      candidate.Materials_.Load((builtinData + "/world/ground-materials.json").c_str()) &&
      candidate.Recipes_.Load((builtinData + "/world/vegetation.json").c_str(),
                              candidate.Materials_);
  if (candidate.RecipesReady_) {
    if (auto bound = candidate.BindRegionCache(under, sources); !bound) {
      return std::unexpected(std::move(bound.error()));
    }
  } else if (diagnostics != nullptr) {
    const LogThreadSinkScope logs(diagnostics);
    Log::Warn(LogTag::World, "ground_tables_unavailable", {{"directory", builtinData}});
  }

  Close();
  Sources_ = std::move(candidate.Sources_);
  AssetStore_ = std::move(candidate.AssetStore_);
  PreparedTerrain_ = std::move(candidate.PreparedTerrain_);
  PreparedBuildings_ = std::move(candidate.PreparedBuildings_);
  PreparedNetwork_ = std::move(candidate.PreparedNetwork_);
  PreparedRegions_ = std::move(candidate.PreparedRegions_);
  PreparedRoads_ = std::move(candidate.PreparedRoads_);
  PreparedVectors_ = std::move(candidate.PreparedVectors_);
  PreparedUrban_ = std::move(candidate.PreparedUrban_);
  Materials_ = std::move(candidate.Materials_);
  Recipes_ = std::move(candidate.Recipes_);
  RecipesReady_ = candidate.RecipesReady_;
  HasVectorSource_ = candidate.HasVectorSource_;
  VectorSchema_ = candidate.VectorSchema_;
  VectorZoom_ = candidate.VectorZoom_;
  Declared_.clear();
  DeclaredDigest_ = std::move(*digest);
  Cls_.Declares({}, DeclaredDigest_);
  if (diagnostics != nullptr) {
    const LogThreadSinkScope logs(diagnostics);
    Log::Info(LogTag::World,
              "sources_registered",
              {{"sources", static_cast<long long>(sources.Count())}});
  }
  outshine::Ground::GroundSurface surface;
  surface.Grid = outshine::Ground::kStreamGrid;
  surface.Z = TerrainRenderZoom() - 1;
  auto poolConfig = *config;
  poolConfig.Compute = &compute;
  poolConfig.Diagnostics = diagnostics;
  if (PreparedTerrain_) {
    poolConfig.PreparedFields = [assets = PreparedTerrain_](Data::TileId at,
                                                            const TerrainTiles::Shaped &shape) {
      return assets->RequestField(at, shape);
    };
  }
  Pool_ = std::make_unique<outshine::Ground::TilePool>(poolConfig, sources, wire);
  Ground_ = std::make_unique<outshine::Ground::GroundStream>(*Pool_, surface);
  SurfaceZoom_ = surface.Z;
  Cls_.SetVectorSource(HasVectorSource_, VectorSchema_);
  Cls_.Open(focus.LatitudeDeg, focus.LongitudeDeg, compute);
  Cls_.SetPreparedTiles(PreparedVectors_);
  if (RecipesReady_) { Cls_.SetRecipes(&Recipes_); }
  Opened_ = true;
  return {};
}

void SurfacePreparation::Close() {
  Cls_.Close();
  Vectors_.reset();
  VectorMotion_.reset();
  PreparedUrban_.reset();
  PreparedVectors_.reset();
  Ground_.reset();
  Pool_.reset();
  PreparedTerrain_.reset();
  PreparedBuildings_.reset();
  PreparedNetwork_.reset();
  PreparedRegions_.reset();
  PreparedRoads_.reset();
  AssetStore_.reset();
  StreetMode_ = StreetMode::Source;
  WaterWorker_.reset();
  WaterProducer_ = Generators::Osm::WaterField{};
  WaterBodies_ = Generators::WaterAsset{};
  WaterStage_ = WaterStage::Requested;
  WaterMode_ = WaterMode::Immediate;
  LandingCursor_ = {};
  Sources_.reset();
  HasVectorSource_ = false;
  VectorZoom_ = kFineZoom;
  VectorSchema_ = Generators::Osm::MvtSchema::Shortbread;
  Opened_ = false;
  Settled_.reset();
  LastAdvance_ = {};
  WorstAdvance_ = {};
}

int SurfacePreparation::TerrainRenderZoom() const {
  const int sourceZoom = FinestZoomOf(Data::DataKind::Elevation);
  return sourceZoom > 0 ? sourceZoom : kGeographicTerrainRenderZoom;
}

int SurfacePreparation::FinestZoomOf(Data::DataKind kind) const {
  int finest = 0;
  if (!Sources_) { return finest; }
  for (size_t at = 0; at < Sources_->Sources().Count(); ++at) {
    const Data::SourceDecl &decl = Sources_->Sources().At(at).Declaration();
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

void SurfacePreparation::PlanMotionCoverage(std::span<const LongitudeLatitude> path) {
  if (!HasVectorSource_ || !Declared_.empty()) { return; }
  VectorMotion_ = Generators::Osm::OsmField::CoverPath(path, VectorZoom_, kVectorRing);
  Cls_.PlanMotionCoverage(path);
}

void SurfacePreparation::ClearMotionCoverage() noexcept {
  VectorMotion_.reset();
  Cls_.ClearMotionCoverage();
}

const Generators::Osm::OsmTileCoverage *
SurfacePreparation::PlannedVectorCoverageAt(LongitudeLatitude at) const {
  if (!VectorMotion_) { return nullptr; }
  const auto current =
      Generators::Osm::OsmField::SourceWindow(at, VectorZoom_, kVectorRing, kVectorTiles);
  return current && VectorMotion_->Window.Covers(*current) ? &*VectorMotion_ : nullptr;
}

std::expected<Generators::Osm::OsmTileWindow, std::string_view>
SurfacePreparation::VectorWindowAt(LongitudeLatitude at, int rings) const {
  if (const auto *motion = PlannedVectorCoverageAt(at)) { return motion->Window; }
  return Generators::Osm::OsmField::SourceWindow(at, VectorZoom_, rings, kVectorTiles);
}

std::expected<Generators::Osm::OsmField::BuildMetrics, std::string>
SurfacePreparation::BuildVectors(LongitudeLatitude at,
                                 TileAt tile,
                                 const Generators::Osm::OsmTileCoverage *motion,
                                 int rings) {
  if (!HasVectorSource_ || !Declared_.empty()) {
    Vectors_->Declare(
        std::span<const Generators::Osm::OsmField::Declared>(Declared_), tile, DeclaredDigest_);
    return Generators::Osm::OsmField::BuildMetrics{};
  }
  constexpr Generators::Osm::OsmField::ParseBudget parsing{.TilesMost =
                                                               kVectorParseTilesPerAdvanceAt};
  const auto built = motion != nullptr ? Vectors_->Build(*Pool_, *motion, parsing)
                                       : Vectors_->Build(*Pool_, at, rings, kVectorTiles, parsing);
  if (!built) { return std::unexpected(built.error()); }
  return Vectors_->LastBuildMetrics();
}

std::expected<void, std::string> SurfacePreparation::AdvanceAt(LongitudeLatitude at,
                                                               SurfacePreparationBudget budget) {
  const auto restandAt = std::chrono::steady_clock::now();
  SurfacePreparationMetrics metrics;
  const auto complete = [&] -> std::expected<void, std::string> {
    metrics.TotalMs = ElapsedMs(restandAt);
    RecordAdvance(metrics);
    return {};
  };
  const auto vectorTile = ValidatePosition(at);
  if (!vectorTile) { return std::unexpected(std::string(vectorTile.error())); }
  if (!Pool_) { return complete(); }
  const auto *motion = PlannedVectorCoverageAt(at);
  const auto centre = motion != nullptr ? motion->Centre : *vectorTile;
  if (UsesCachedRegionNetwork() &&
      (centre.X != CachedNetworkTile_.X || centre.Y != CachedNetworkTile_.Y)) {
    RequestStreets();
  }
  const auto classificationAt = std::chrono::steady_clock::now();
  const auto classified = Cls_.Update(*Pool_, at);
  metrics.ClassificationMs = ElapsedMs(classificationAt);
  if (!classified) { return std::unexpected(classified.error()); }
  Stood_ = at;
  if (!RecipesReady_) { return complete(); }
  const auto vectorAt = std::chrono::steady_clock::now();
  if (!Vectors_) {
    Vectors_ = CreateVectorField();
    Footprints_.AnchorAt(Cls_.OriginEcef());
  }
  const uint64_t previousVectorGeneration = Vectors_->Generation();
  const auto built = BuildVectors(at, *vectorTile, motion, budget.VectorRing);
  if (!built) { return std::unexpected(built.error()); }
  metrics.VectorBuild = *built;
  if (Vectors_->Generation() != previousVectorGeneration) {
    Footprints_.ResetDerived();
    Ways_ = ::outshine::Generators::Osm::StreetField{};
    WaterBodies_ = Generators::WaterAsset{};
    WaterStage_ = PreparedRegions_ && WaterMode_ == WaterMode::RegionLookup ? WaterStage::Deferred
                                                                            : WaterStage::Requested;
  }
  if (WaterStage_ == WaterStage::Ready && !WaterReady()) {
    WaterStage_ = PreparedRegions_ && WaterMode_ == WaterMode::RegionLookup ? WaterStage::Deferred
                                                                            : WaterStage::Requested;
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
      Cls_.ReleaseBuildInputs();
      Settle();
      Settled_ = inputs;
      const size_t held = HeapBytes();
      if (held > kHoldsBytes && Pool_) { (void)Pool_->EvictInputs(held - kHoldsBytes); }
      metrics.SettlementMs += ElapsedMs(settleAt);
      if (HeapBytes() > kHoldsBytes) {
        ++Overflowed_;
        Overflowing_ = true;
        break;
      }
    }
    const size_t before =
        Ways_.IngestedTiles() + WaterProducer_.IngestedTiles() + Footprints_.IngestedTiles();
    if (!UsesCachedRegionNetwork()) {
      const auto streetsAt = std::chrono::steady_clock::now();
      const auto looked = Ways_.LookedCount();
      (void)Ways_.Ingest(*Vectors_, Recipes_, budget.StreetAdmissionTime);
      metrics.IngestionUnits += static_cast<size_t>(Ways_.LookedCount() - looked);
      metrics.StreetsMs += ElapsedMs(streetsAt);
    }
    const auto waterAt = std::chrono::steady_clock::now();
    if (WaterStage_ == WaterStage::Requested) {
      (void)WaterProducer_.Ingest(*Ground_, *Vectors_, Recipes_);
      metrics.IngestionUnits += WaterProducer_.LastIngest().AdvancedUnits;
      if (WaterProducer_.Ingested(*Vectors_)) {
        WaterBodies_ = WaterProducer_.Asset(*Vectors_);
        WaterInputs_ = CurrentWaterInputs();
        WaterStage_ = WaterStage::Ready;
      }
    }
    metrics.WaterMs += ElapsedMs(waterAt);
    const size_t after =
        Ways_.IngestedTiles() + WaterProducer_.IngestedTiles() + Footprints_.IngestedTiles();
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
  WaterProducer_.Settle();
  if (Vectors_) { Vectors_->Settle(); }
}

bool SurfacePreparation::Drained() const {
  if (!RecipesReady_ || !Vectors_) { return true; }
  return (UsesCachedRegionNetwork() || Ways_.Ingested(*Vectors_)) && WaterReady();
}

bool SurfacePreparation::Ingested() const {
  return InputsReady() && WaterInputsReady(std::nullopt);
}

bool SurfacePreparation::VectorSurfacesReady(std::optional<int> rings) const {
  return StreetInputsReady(rings) && WaterInputsReady(rings);
}

bool SurfacePreparation::InputsReadyWithin(int rings) const {
  return ClassificationReady() && StreetInputsReady(rings);
}

bool SurfacePreparation::InputsReady() const {
  return ClassificationReady() && StreetInputsReady(std::nullopt);
}

bool SurfacePreparation::StreetInputsReady(std::optional<int> rings) const {
  return VectorGeometryReady(rings) &&
         (!RecipesReady_ || UsesCachedRegionNetwork() ||
          (rings ? Ways_.IngestedWithin(*Vectors_, *rings) : Ways_.Ingested(*Vectors_)));
}

bool SurfacePreparation::WaterInputsReady(std::optional<int> rings) const {
  return !RecipesReady_ || WaterReady() ||
         (rings && WaterProducer_.UsesTerrain(*Ground_) &&
          WaterProducer_.IngestedWithin(*Vectors_, *rings));
}

bool SurfacePreparation::VectorGeometryReady(std::optional<int> rings) const {
  if (!RecipesReady_ || !Vectors_) { return !RecipesReady_; }
  return rings ? Vectors_->SettledWithin(*rings) : Vectors_->PendingTiles() == 0;
}

std::optional<std::expected<WaterGeometryPreparation::Completed, std::string>>
SurfacePreparation::AdvanceWaterGeometry(Tasks &pool, const TangentFrame &frame) {
  const auto &source = WaterBodies();
  if (WaterWorker_ && !WaterWorker_->Matches(source)) {
    WaterWorker_->Cancel();
    if (WaterWorker_->Complete()) {
      WaterWorker_.reset();
    } else if (!source.SurfacesReady()) {
      return std::nullopt;
    }
  }
  if (source.SurfacesReady()) { return WaterGeometryPreparation::Completed{.Water = source}; }
  if (!WaterWorker_) {
    WaterWorker_ = std::make_unique<WaterGeometryPreparation>(pool, source, frame);
    return std::nullopt;
  }
  auto result = WaterWorker_->Collect();
  if (result) {
    WaterWorker_.reset();
    if (*result && WaterReady()) { WaterBodies_ = result->value().Water; }
  }
  return result;
}

void SurfacePreparation::UseCachedRegionNetwork(LongitudeLatitude focus) {
  if (!Opened_ || StreetMode_ == StreetMode::RequiredSource) { return; }
  const auto tile = Generators::Osm::OsmField::Locate(focus, VectorZoom_);
  if (!tile) {
    ForgetCachedRegionNetwork();
    return;
  }
  if (StreetMode_ == StreetMode::CachedRegion && tile->X == CachedNetworkTile_.X &&
      tile->Y == CachedNetworkTile_.Y) {
    return;
  }
  StreetMode_ = StreetMode::CachedRegion;
  CachedNetworkTile_ = *tile;
  Settled_.reset();
}

void SurfacePreparation::ForgetCachedRegionNetwork() noexcept {
  if (!UsesCachedRegionNetwork()) { return; }
  StreetMode_ = StreetMode::Source;
  Settled_.reset();
}

void SurfacePreparation::RequestStreets() noexcept {
  StreetMode_ = StreetMode::RequiredSource;
  Settled_.reset();
}

bool SurfacePreparation::UsesCachedRegionNetwork() const noexcept {
  return StreetMode_ == StreetMode::CachedRegion;
}

std::string SurfacePreparation::IngestionStatus() const {
  if (!Vectors_) { return "vectors=absent"; }
  return "streets=" + std::to_string(Ways_.IngestedTiles()) + "/" +
         std::to_string(static_cast<int>(Ways_.Ingested(*Vectors_))) + ", water=" +
         std::to_string(WaterReady() ? WaterBodies_.IngestedTiles()
                                     : WaterProducer_.IngestedTiles()) +
         "/" + std::to_string(static_cast<int>(WaterReady())) +
         ", waterDeferrals=" + std::to_string(WaterProducer_.Deferrals()) +
         ", waterReused=" + std::to_string(WaterProducer_.ReusedTiles()) +
         ", classes=" + std::to_string(static_cast<int>(Cls_.Complete()));
}

}
