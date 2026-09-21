#ifndef OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H
#define OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H

#include <span>
#include "math/Vec3.h"
#include "OsmField.h"

#include <cstdint>
#include <cassert>
#include <functional>
#include <optional>
#include <vector>

#include "Capacity.h"
#include <Earth.h>
#include "StructureMesher.h"
#include "TileRanges.h"
#include "TileWatermark.h"

#include <scene/LevelOfDetail.h>

namespace outshine::Ground {

class BuildingField {
public:
  enum class HeightSource : uint8_t { Osm, Default };

  struct Footprint {
    uint32_t FirstPoint = 0, PointCount = 0;
    float HeightM = 0.0f;
    float BaseM = 0.0f;
    float SeatM = 0.0f;
    float FootM = 0.0f;
    HeightSource Source = HeightSource::Default;
    Frontage Street;

    LevelOfDetail Coarseness = LevelOfDetail::Fine;
  };

  struct Baked {
    std::span<const Footprint> Prints;
    std::span<const double> SeatSpreadM;
    std::span<const double> AcrossM;
    size_t Triangles = 0;
    int OsmHeights = 0;
    int DefaultHeights = 0;
    int Fronted = 0;
  };

  struct AcceptanceCapacity {
    size_t Prints = 0;
    size_t Spread = 0;
    size_t Across = 0;
    uint32_t LargestTile = 0;
  };

  class PendingAcceptance {
    friend class BuildingField;

  public:
    PendingAcceptance(const PendingAcceptance &) = delete;
    PendingAcceptance &operator=(const PendingAcceptance &) = delete;
    PendingAcceptance(PendingAcceptance &&) noexcept = default;
    PendingAcceptance &operator=(PendingAcceptance &&) noexcept = default;

  private:
    PendingAcceptance(const BuildingField *owner, uint32_t tile, const Baked &baked) noexcept
        : Owner_(owner),
          Tile_(tile),
          Prints_(baked.Prints.size()),
          Spread_(baked.SeatSpreadM.size()),
          Across_(baked.AcrossM.size()) {}

    const BuildingField *Owner_ = nullptr;
    uint32_t Tile_ = 0;
    size_t Prints_ = 0;
    size_t Spread_ = 0;
    size_t Across_ = 0;
  };

  void SeenWith(double focalPx) { FocalPx_ = focalPx; }

  void TilesSpan(double tileSpanM) { TileSpanM_ = tileSpanM; }

  [[nodiscard]] double CarriesFromM() const { return outshine::CarriesFromM(TileSpanM_); }

  [[nodiscard]] double FocalPx() const { return FocalPx_; }

  [[nodiscard]] double TileSpanM() const { return TileSpanM_; }

  void AnchorAt(const Vec3 &ecef);
  void ResetDerived();

  [[nodiscard]] uint64_t Revision() const noexcept { return Revision_; }

  [[nodiscard]] std::optional<TileWatermark::Next>
  Next(const OsmField &field,
       const std::function<bool(FeatureRun)> &groundStands,
       size_t candidatesMost);

  void Take(uint32_t tile) {
    Mark_.Take(tile);
    ++Taken_;
  }

  void Release(uint32_t tile) {
    Mark_.Release(tile);
    --Taken_;
  }

  [[nodiscard]] PendingAcceptance PrepareAcceptance(uint32_t tile, const Baked &baked);
  void PreparesAcceptances(AcceptanceCapacity capacity);
  void
  CommitAcceptance(PendingAcceptance pending, const OsmField &field, const Baked &baked) noexcept;

  [[nodiscard]] size_t TrianglesHanded() const { return TrianglesHanded_; }

  [[nodiscard]] const Vec3 &Anchor() const { return Anchor_; }

  [[nodiscard]] bool Anchored() const noexcept { return Anchored_; }

  [[nodiscard]] const std::vector<Footprint> &Footprints() const { return Prints_; }

  [[nodiscard]] std::span<const Footprint> OfTile(int tile) const {
    if (tile < 0) { return {}; }
    const TileRanges::Range r = ByTile_.At(static_cast<uint32_t>(tile));
    return {Prints_.data() + r.First, r.Count};
  }

  [[nodiscard]] int OsmHeights() const { return OsmHeights_; }

  [[nodiscard]] int DefaultHeights() const { return DefaultHeights_; }

  [[nodiscard]] const std::vector<double> &SeatSpreadM() const { return SeatSpread_; }

  [[nodiscard]] const std::vector<double> &FootprintAcrossM() const { return Across_; }

  [[nodiscard]] int Deferrals() const { return Mark_.Deferrals(); }

  void Settle() {
    Prints_.shrink_to_fit();
    AcceptedTiles_.shrink_to_fit();
  }

  [[nodiscard]] bool IngestedWithin(const OsmField &field, int rings) const noexcept;

  [[nodiscard]] size_t PrintBytes() const { return CapacityBytes(Prints_); }

  [[nodiscard]] size_t MeasurementBytes() const {
    return CapacityBytes(SeatSpread_) + CapacityBytes(Across_);
  }

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Prints_) + CapacityBytes(AcceptedTiles_) + Mark_.HeapBytes() +
           ByTile_.HeapBytes() + MeasurementBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const {
    return Mark_.Done(field.Features()) && Taken_ == Accepted_;
  }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  uint64_t Revision_ = 0;
  std::vector<Footprint> Prints_;
  std::vector<uint32_t> AcceptedTiles_;
  size_t TrianglesHanded_ = 0;
  size_t Taken_ = 0, Accepted_ = 0;
  TileRanges ByTile_;
  TileWatermark Mark_;
  double FocalPx_ = 0.0;
  double TileSpanM_ = 0.0;
  Vec3 Anchor_;
  bool Anchored_ = false;
  int OsmHeights_ = 0, DefaultHeights_ = 0, Fronted_ = 0;
  std::vector<double> SeatSpread_;
  std::vector<double> Across_;
};

}
#endif
