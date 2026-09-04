#ifndef OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H
#define OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H

#include <span>
#include "math/Vec3.h"
#include "OsmField.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "Capacity.h"
#include <Earth.h>
#include "StructureMesher.h"
#include "TileRanges.h"
#include "TileWatermark.h"

#include <generate/Generate.h>

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

    Generators::Detail Coarseness = Generators::Detail::Fine;
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

  void SeenWith(double focalPx) { FocalPx_ = focalPx; }

  void TilesSpan(double tileSpanM) { TileSpanM_ = tileSpanM; }

  [[nodiscard]] double CarriesFromM() const { return outshine::CarriesFromM(TileSpanM_); }

  [[nodiscard]] double FocalPx() const { return FocalPx_; }

  [[nodiscard]] double TileSpanM() const { return TileSpanM_; }

  void AnchorAt(const Vec3 &ecef);

  [[nodiscard]] std::optional<TileWatermark::Next>
  Next(const OsmField &field, const std::function<bool(FeatureRun)> &groundStands);

  void Take(uint32_t tile) {
    Mark_.Take(tile);
    ++Taken_;
  }

  void Accept(uint32_t tile, const OsmField &field, const Baked &baked);

  [[nodiscard]] double AwayFromCentreM(const OsmField &field, uint32_t tile) const;

  [[nodiscard]] size_t TrianglesHanded() const { return TrianglesHanded_; }

  [[nodiscard]] const Vec3 &Anchor() const { return Anchor_; }

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

  void Settle() { Prints_.shrink_to_fit(); }

  [[nodiscard]] size_t PrintBytes() const { return CapacityBytes(Prints_); }

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Prints_) + Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const {
    return Mark_.Done(field.Features()) && Taken_ == Accepted_;
  }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  std::vector<Footprint> Prints_;
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

} // namespace outshine::Ground
#endif
