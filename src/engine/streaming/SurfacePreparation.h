#ifndef OUTSHINE_ENGINE_STREAMING_SURFACEPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_SURFACEPREPARATION_H

#include <memory>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ContentStore.h"
#include "SourceConfiguration.h"
#include "SourceSet.h"
#include "PreparedTerrainAssets.h"
#include "TerrainLoader.h"
#include <world/data/SourceDecl.h>
#include "BuildingField.h"
#include "ClassificationPreparation.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "StreetField.h"
#include "TilePool.h"
#include "VegetationTemplates.h"
#include "WaterField.h"

#include <world/WorldStorage.h>

namespace outshine {
class LogSink;
class Sink;
}

namespace outshine::Ground {

constexpr int kVectorRing = 3;
constexpr int kVectorTiles = (2 * kVectorRing + 1) * (2 * kVectorRing + 1);
constexpr size_t kVectorParseTilesPerAdvanceAt = 2;
constexpr size_t kFrameIngestTiles = 1;

struct SurfacePreparationBudget {
  size_t IngestTilesMost = 0;
  int VectorRing = 0;
};

struct SurfacePreparationMetrics {
  double TotalMs = 0.0;
  double ClassificationMs = 0.0;
  double VectorsMs = 0.0;
  ::outshine::Generators::Osm::OsmField::BuildMetrics VectorBuild;
  double StreetsMs = 0.0;
  double WaterMs = 0.0;
  double SettlementMs = 0.0;
};

class SurfacePreparation {
public:
  SurfacePreparation() = default;

  ~SurfacePreparation() { Close(); }

  SurfacePreparation(const SurfacePreparation &) = delete;
  SurfacePreparation &operator=(const SurfacePreparation &) = delete;

  [[nodiscard]] bool Open(const World::StoragePaths &under,
                          std::span<const Data::SourceProvider> providers,
                          LongitudeLatitude focus,
                          Data::Transport &wire,
                          Tasks &compute,
                          Sink &say,
                          LogSink *diagnostics,
                          double patienceS = 0.0,
                          const Data::ProviderRegistry *registry = nullptr);
  void Close();

  [[nodiscard]] bool Opened() const { return Opened_; }

  static constexpr std::size_t kHoldsBytes = std::size_t{512} * 1024u * 1024u;

  [[nodiscard]] std::size_t OverCeiling() const { return Overflowed_; }

  [[nodiscard]] bool Overflowing() const { return Overflowing_; }

  [[nodiscard]] std::size_t HeapBytes() const {
    return Cls_.HeapBytes() + Footprints_.HeapBytes() + WaterBodies_.HeapBytes() +
           Ways_.HeapBytes() + (Vectors_ ? Vectors_->HeapBytes() : 0u) +
           (Pool_ ? Pool_->ResidentBytes() : 0u) + (Ground_ ? Ground_->HeapBytes() : 0u);
  }

  [[nodiscard]] TilePool &Pool() const { return *Pool_; }

  struct SourceCounters {
    Data::SourceSet::Ledger Sources;
    Data::ContentStore::Ledger Store;
  };

  [[nodiscard]] SourceCounters Counters() const {
    return {.Sources = Sources_ ? Sources_->Counters() : Data::SourceSet::Ledger{},
            .Store = Store_ ? Store_->Counters() : Data::ContentStore::Ledger{}};
  }

  [[nodiscard]] Data::ContentStore::Ledger StoreCosts() const {
    return Store_ ? Store_->Counters() : Data::ContentStore::Ledger{};
  }

  [[nodiscard]] auto ProviderCosts() const {
    return Sources_ ? Sources_->ProviderCosts()
                    : std::array<Data::SourceSet::Ledger::ProviderCost,
                                 Data::SourceSet::Ledger::KindCount>{};
  }

  [[nodiscard]] auto PreparedTerrainCosts() const noexcept {
    return PreparedTerrain_ ? PreparedTerrain_->Costs()
                            : ::outshine::Generators::PreparedTerrainAssets::Counters{};
  }

  void Declares(std::span<const ::outshine::Generators::Osm::OsmField::Declared> these) {
    Declared_.assign(these.begin(), these.end());
    Cls_.Declares(these);
  }

  [[nodiscard]] GroundStream &Ground() const { return *Ground_; }

  [[nodiscard]] const GroundQuery *TryGround() const noexcept { return Ground_.get(); }

  [[nodiscard]] const ClassificationPreparation &Classes() const { return Cls_; }

  [[nodiscard]] const GroundMaterials &Materials() const { return Materials_; }

  void SetVegetation(const VegetationTemplates *veg) { Cls_.SetVegetation(veg); }

  [[nodiscard]] std::unique_ptr<::outshine::Generators::Osm::OsmField> CreateVectorField() const;

  [[nodiscard]] bool HasDeclaredVectors() const noexcept { return !Declared_.empty(); }

  [[nodiscard]] const ::outshine::Generators::Osm::OsmField *Vectors() const {
    return Vectors_.get();
  }

  [[nodiscard]] bool HasVectorSource() const noexcept { return HasVectorSource_; }

  [[nodiscard]] int VectorZoom() const noexcept { return VectorZoom_; }

  [[nodiscard]] const ::outshine::Generators::Osm::BuildingField &Footprints() const {
    return Footprints_;
  }

  [[nodiscard]] ::outshine::Generators::Osm::BuildingField &Footprints() { return Footprints_; }

  void SeeFootprintsWith(ProjectedErrorBudget projection) { Footprints_.SeenWith(projection); }

  void FootprintTilesSpan(double tileSpanM) { Footprints_.TilesSpan(tileSpanM); }

  [[nodiscard]] const ::outshine::Generators::Osm::WaterField &WaterBodies() const {
    return WaterBodies_;
  }

  [[nodiscard]] const ::outshine::Generators::Osm::StreetField &Ways() const { return Ways_; }

  [[nodiscard]] const VegetationTemplates &Vegetation() const { return Templates_; }

  [[nodiscard]] bool Vegetated() const { return Vegetated_; }

  [[nodiscard]] std::expected<void, std::string_view> AdvanceAt(LongitudeLatitude at,
                                                                SurfacePreparationBudget budget);

  [[nodiscard]] const SurfacePreparationMetrics &LastAdvance() const noexcept {
    return LastAdvance_;
  }

  [[nodiscard]] const SurfacePreparationMetrics &WorstAdvance() const noexcept {
    return WorstAdvance_;
  }

  [[nodiscard]] bool AwaitProgress(double seconds);

  [[nodiscard]] bool StandsAt(LongitudeLatitude at) const { return Stood_ == at && Ingested(); }

  void Settle();
  [[nodiscard]] bool Drained() const;
  [[nodiscard]] bool Ingested() const;
  [[nodiscard]] bool IngestedWithin(int rings) const;
  [[nodiscard]] std::string IngestionStatus() const;
  [[nodiscard]] int FinestZoomOf(Data::DataKind kind) const;

private:
  void RecordAdvance(SurfacePreparationMetrics metrics) noexcept;
  [[nodiscard]] std::expected<TileAt, std::string_view>
  ValidatePosition(LongitudeLatitude at) const;

  std::unique_ptr<Data::ContentStore> Store_;
  std::unique_ptr<Data::SourceSet> Sources_;
  std::shared_ptr<::outshine::Generators::PreparedTerrainAssets> PreparedTerrain_;
  std::unique_ptr<TilePool> Pool_;
  TilePool::LandingCursor LandingCursor_;
  std::unique_ptr<GroundStream> Ground_;
  std::vector<::outshine::Generators::Osm::OsmField::Declared> Declared_;
  ClassificationPreparation Cls_;
  GroundMaterials Materials_;
  VegetationTemplates Templates_;
  std::size_t Overflowed_ = 0;

  struct SettlementInputs {
    uint64_t Classes = 0;
    uint64_t Vectors = 0;
    uint64_t Footprints = 0;
    [[nodiscard]] bool operator==(const SettlementInputs &) const = default;
  };

  void IngestLayers(const SettlementInputs &inputs,
                    SurfacePreparationBudget budget,
                    SurfacePreparationMetrics &metrics);

  [[nodiscard]] bool CanReuseSettlement(const SettlementInputs &inputs, int rings) const;

  std::optional<SettlementInputs> Settled_;
  bool Overflowing_ = false;
  std::unique_ptr<::outshine::Generators::Osm::OsmField> Vectors_;
  bool HasVectorSource_ = false;
  int VectorZoom_ = kFineZoom;
  Generators::Osm::MvtSchema VectorSchema_ = Generators::Osm::MvtSchema::Shortbread;
  ::outshine::Generators::Osm::BuildingField Footprints_;
  ::outshine::Generators::Osm::WaterField WaterBodies_;
  ::outshine::Generators::Osm::StreetField Ways_;
  int SurfaceZoom_ = 0;
  std::optional<LongitudeLatitude> Stood_;
  bool Vegetated_ = false;
  bool Opened_ = false;
  SurfacePreparationMetrics LastAdvance_;
  SurfacePreparationMetrics WorstAdvance_;
};

}

#endif
