#ifndef OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H
#define OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H

#include <scene/ProjectedErrorBudget.h>
#include <algorithm>
#include <array>
#include <span>
#include "math/Vec3.h"
#include "OsmField.h"
#include "HeightField.h"
#include "StructureSourceKey.h"
#include "BuildingGeometry.h"

#include <cstdint>
#include <cassert>
#include <functional>
#include <optional>
#include <memory>
#include <utility>
#include <vector>

#include "Capacity.h"
#include <Earth.h>
#include "StructureMesher.h"
#include "StructureCellGrid.h"
#include "TileWatermark.h"

namespace outshine::Ground {

class BuildingField {
public:
  enum class HeightSource : uint8_t { Osm, Default };

  struct Footprint {
    uint32_t FirstPoint = 0, PointCount = 0;
    uint32_t FirstHole = 0, HoleCount = 0;
    float HeightM = 0.0f;
    float MinimumHeightM = 0.0f;
    float BaseM = 0.0f;
    float SeatM = 0.0f;
    float FootM = 0.0f;
    HeightSource Source = HeightSource::Default;
    Frontage Street;

    [[nodiscard]] bool operator==(const Footprint &) const noexcept = default;
  };

  struct Baked {
    std::shared_ptr<const BuildingGeometry> Coordinates;
    std::span<const Footprint> Prints;
    std::span<const double> SeatSpreadM;
    std::span<const double> AcrossM;
    uint64_t OccupiedCells = 0;
    std::array<GeoBounds, kStructureCellsPerTile> CellBounds{};
    std::array<float, kStructureCellsPerTile> CellMaxHeightM{};
    size_t Triangles = 0;
    int OsmHeights = 0;
    int DefaultHeights = 0;
    int Fronted = 0;
  };

  struct AcceptanceCapacity {
    size_t Prints = 0;
    size_t Spread = 0;
    size_t Across = 0;
    size_t Tiles = 0;
  };

  struct BakeInputs {
    uint64_t HeightRasterDigest = 0;
    uint64_t StreetDigest = 0;
    ProjectedErrorBudget Projection{};
    double TileSpanM = 0.0;
    LongitudeLatitude Eye;
  };

  struct AcceptedInput {
    std::shared_ptr<const BuildingGeometry> Coordinates;
    std::optional<Data::TileSourceIdentity> Vector;
    std::vector<Data::TileSourceIdentity> Sources;
    uint64_t OccupiedCells = 0;
    std::array<GeoBounds, kStructureCellsPerTile> CellBounds{};
    std::array<float, kStructureCellsPerTile> CellMaxHeightM{};
    bool Qualified = false;
    uint64_t SourceKey = 0;
    BakeInputs Bake;
    TerrainCertificate Terrain;
    HeightField::Request Heights;
  };

  class PendingAcceptance {
    friend class BuildingField;

  public:
    PendingAcceptance(const PendingAcceptance &) = delete;
    PendingAcceptance &operator=(const PendingAcceptance &) = delete;
    PendingAcceptance(PendingAcceptance &&) noexcept = default;
    PendingAcceptance &operator=(PendingAcceptance &&) noexcept = default;

  private:
    PendingAcceptance(const BuildingField *owner,
                      uint32_t tile,
                      const Baked &baked,
                      std::optional<Data::TileSourceIdentity> vector,
                      std::span<const Data::TileSourceIdentity> sources,
                      bool qualified,
                      BakeInputs bake,
                      TerrainCertificate certificate,
                      HeightField::Request heights)
        : Owner_(owner),
          Tile_(tile),
          Prints_(baked.Prints.size()),
          Spread_(baked.SeatSpreadM.size()),
          Across_(baked.AcrossM.size()),
          Input_{.Coordinates = baked.Coordinates,
                 .Vector = std::move(vector),
                 .Sources = std::vector<Data::TileSourceIdentity>(sources.begin(), sources.end()),
                 .OccupiedCells = baked.OccupiedCells,
                 .CellBounds = baked.CellBounds,
                 .CellMaxHeightM = baked.CellMaxHeightM,
                 .Qualified = qualified,
                 .SourceKey = 0,
                 .Bake = bake,
                 .Terrain = std::move(certificate),
                 .Heights = std::move(heights)} {
      std::ranges::sort(Input_.Sources);
      Input_.Sources.erase(std::ranges::unique(Input_.Sources).begin(), Input_.Sources.end());
      if (qualified) {
        Input_.SourceKey = StructureSourceKey(
            {.Vector = Input_.Vector,
             .HeightSources = Input_.Sources,
             .HeightDigest = Input_.Bake.HeightRasterDigest,
             .StreetDigest = Input_.Bake.StreetDigest,
             .TileSpanM = Input_.Bake.TileSpanM,
             .FallbackHeights = false,
             .Origin = Input_.Coordinates ? &Input_.Coordinates->Origin : nullptr});
      }
    }

    [[maybe_unused]] const BuildingField *Owner_ = nullptr;
    uint32_t Tile_ = 0;
    size_t Prints_ = 0;
    size_t Spread_ = 0;
    size_t Across_ = 0;
    AcceptedInput Input_;
  };

  void SeenWith(ProjectedErrorBudget projection) { Projection_ = projection; }

  void TilesSpan(double tileSpanM) { TileSpanM_ = tileSpanM; }

  [[nodiscard]] double CarriesFromM() const { return outshine::CarriesFromM(TileSpanM_); }

  [[nodiscard]] ProjectedErrorBudget Projection() const noexcept { return Projection_; }

  [[nodiscard]] double TileSpanM() const { return TileSpanM_; }

  void AnchorAt(const Vec3 &ecef);
  void ResetDerived();

  [[nodiscard]] BuildingField SnapshotAccepted() const;

  void BeginRefinement() noexcept {
    RefinementAt_ = 0;
    RefinementEnd_ = AcceptedTiles_.size();
    RefinementActive_ = true;
  }

  [[nodiscard]] std::span<const AcceptedInput> AcceptedInputs() const noexcept {
    return AcceptedInputs_;
  }

  [[nodiscard]] std::span<const uint32_t> AcceptedTiles() const noexcept { return AcceptedTiles_; }

  [[nodiscard]] std::optional<uint32_t> RefinementTile() const noexcept {
    if (!RefinementActive_ || RefinementAt_ == RefinementEnd_) { return std::nullopt; }
    return AcceptedTiles_[RefinementAt_];
  }

  void AdvanceRefinement() noexcept {
    assert(RefinementActive_ && RefinementAt_ < RefinementEnd_);
    ++RefinementAt_;
  }

  void RetryRefinement(uint32_t tile) noexcept {
    const auto at = std::ranges::lower_bound(AcceptedTiles_, tile);
    assert(RefinementActive_ && at != AcceptedTiles_.end() && *at == tile);
    RefinementAt_ = std::min(RefinementAt_, static_cast<size_t>(at - AcceptedTiles_.begin()));
  }

  [[nodiscard]] bool RefinementComplete() const noexcept {
    return RefinementActive_ && RefinementAt_ == RefinementEnd_;
  }

  [[nodiscard]] size_t RefinementRemaining() const noexcept {
    return RefinementActive_ ? RefinementEnd_ - RefinementAt_ : 0;
  }

  [[nodiscard]] uint64_t Revision() const noexcept { return Revision_; }

  [[nodiscard]] std::optional<TileWatermark::Next>
  Next(const OsmField &field,
       const std::function<bool(FeatureRun)> &groundStands,
       size_t candidatesMost);

  [[nodiscard]] const std::shared_ptr<const void> &ReservationOwner() const noexcept {
    return ReservationOwner_.Token;
  }

  void Take(uint32_t tile) {
    Mark_.Take(tile);
    ++Taken_;
  }

  void Release(uint32_t tile) {
    Mark_.Release(tile);
    --Taken_;
  }

  [[nodiscard]] PendingAcceptance
  PrepareAcceptance(uint32_t tile,
                    const Baked &baked,
                    std::span<const Data::TileSourceIdentity> sources = {},
                    bool qualified = false,
                    std::optional<Data::TileSourceIdentity> vector = std::nullopt);
  [[nodiscard]] PendingAcceptance
  PrepareAcceptance(uint32_t tile,
                    const Baked &baked,
                    std::span<const Data::TileSourceIdentity> sources,
                    bool qualified,
                    std::optional<Data::TileSourceIdentity> vector,
                    BakeInputs bake,
                    TerrainCertificate certificate = {},
                    HeightField::Request heights = {});
  void PreparesAcceptances(AcceptanceCapacity capacity);
  void CommitAcceptance(PendingAcceptance pending, const Baked &baked) noexcept;
  void
  CommitAcceptance(PendingAcceptance pending, const OsmField &field, const Baked &baked) noexcept;
  void ReplaceAcceptance(PendingAcceptance pending, const Baked &baked) noexcept;

  [[nodiscard]] size_t TrianglesHanded() const { return TrianglesHanded_; }

  [[nodiscard]] const Vec3 &Anchor() const { return Anchor_; }

  [[nodiscard]] bool Anchored() const noexcept { return Anchored_; }

  [[nodiscard]] const std::vector<Footprint> &Footprints() const { return Prints_; }

  [[nodiscard]] std::span<const Footprint> OfTile(int tile) const {
    if (tile < 0) { return {}; }
    const auto at = std::ranges::lower_bound(AcceptedTiles_, static_cast<uint32_t>(tile));
    if (at == AcceptedTiles_.end() || std::cmp_not_equal(*at, tile)) { return {}; }
    const Range r = Products_[static_cast<size_t>(at - AcceptedTiles_.begin())].Prints;
    return r.Count == 0 ? std::span<const Footprint>{}
                        : std::span<const Footprint>{Prints_.data() + r.First, r.Count};
  }

  [[nodiscard]] const BuildingGeometry *GeometryOfFootprint(size_t index) const noexcept {
    const auto at = std::ranges::upper_bound(
        Products_, index, {}, [](const TileProduct &product) { return product.Prints.First; });
    if (at == Products_.begin()) { return nullptr; }
    const size_t product = static_cast<size_t>(at - Products_.begin() - 1);
    const Range range = Products_[product].Prints;
    return index < range.First + range.Count ? AcceptedInputs_[product].Coordinates.get() : nullptr;
  }

  [[nodiscard]] const AcceptedInput *InputOfTile(uint32_t tile) const noexcept {
    const auto at = std::ranges::lower_bound(AcceptedTiles_, tile);
    if (at == AcceptedTiles_.end() || *at != tile) { return nullptr; }
    return &AcceptedInputs_[static_cast<size_t>(at - AcceptedTiles_.begin())];
  }

  [[nodiscard]] int OsmHeights() const { return OsmHeights_; }

  [[nodiscard]] int DefaultHeights() const { return DefaultHeights_; }

  [[nodiscard]] const std::vector<double> &SeatSpreadM() const { return SeatSpread_; }

  [[nodiscard]] const std::vector<double> &FootprintAcrossM() const { return Across_; }

  [[nodiscard]] int Deferrals() const { return Mark_.Deferrals(); }

  void Settle() {
    Prints_.shrink_to_fit();
    AcceptedTiles_.shrink_to_fit();
    AcceptedInputs_.shrink_to_fit();
    Products_.shrink_to_fit();
  }

  [[nodiscard]] bool IngestedWithin(const OsmField &field, int rings) const noexcept;

  [[nodiscard]] size_t PrintBytes() const { return CapacityBytes(Prints_); }

  [[nodiscard]] size_t MeasurementBytes() const {
    return CapacityBytes(SeatSpread_) + CapacityBytes(Across_);
  }

  [[nodiscard]] size_t HeapBytes() const {
    size_t bytes = CapacityBytes(Prints_) + CapacityBytes(AcceptedTiles_) +
                   CapacityBytes(AcceptedInputs_) + CapacityBytes(Products_) + Mark_.HeapBytes() +
                   MeasurementBytes();
    for (const AcceptedInput &input : AcceptedInputs_) {
      if (input.Coordinates) { bytes += input.Coordinates->HeapBytes(); }
      if (input.Vector) {
        bytes += input.Vector->SourceId.capacity() + input.Vector->Revision.capacity();
      }
      bytes += CapacityBytes(input.Sources) + input.Terrain.HeapBytes() +
               CapacityBytes(input.Heights.Tiles);
      for (const auto &source : input.Sources) {
        bytes += source.SourceId.capacity() + source.Revision.capacity();
      }
    }
    return bytes;
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const {
    return Mark_.Done(field.Features()) && Taken_ == Accepted_;
  }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  struct ReservationDomain {
    std::shared_ptr<const void> Token = std::make_shared<const uint8_t>(0);
    ReservationDomain() = default;

    ReservationDomain([[maybe_unused]] const ReservationDomain &other) : ReservationDomain() {}

    ReservationDomain &operator=(const ReservationDomain &other) {
      if (this != &other) { Token = std::make_shared<const uint8_t>(0); }
      return *this;
    }

    ReservationDomain(ReservationDomain &&) noexcept = default;
    ReservationDomain &operator=(ReservationDomain &&) noexcept = default;
  };

  ReservationDomain ReservationOwner_;

  struct Range {
    size_t First = 0;
    size_t Count = 0;
  };

  struct TileProduct {
    Range Prints;
    Range Spread;
    Range Across;
    size_t Triangles = 0;
    int OsmHeights = 0;
    int DefaultHeights = 0;
    int Fronted = 0;
  };

  uint64_t Revision_ = 0;
  std::vector<Footprint> Prints_;
  std::vector<uint32_t> AcceptedTiles_;
  std::vector<AcceptedInput> AcceptedInputs_;
  std::vector<TileProduct> Products_;
  size_t TrianglesHanded_ = 0;
  size_t Taken_ = 0, Accepted_ = 0;
  size_t RefinementAt_ = 0, RefinementEnd_ = 0;
  bool RefinementActive_ = false;
  TileWatermark Mark_;
  ProjectedErrorBudget Projection_{};
  double TileSpanM_ = 0.0;
  Vec3 Anchor_;
  bool Anchored_ = false;
  int OsmHeights_ = 0, DefaultHeights_ = 0, Fronted_ = 0;
  std::vector<double> SeatSpread_;
  std::vector<double> Across_;
};

}
#endif
