#ifndef OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H
#define OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H

#include <span>
#include "math/Vec3.h"
#include "OsmField.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "GroundSample.h"
#include "GroundQuery.h"
#include "StructureMesher.h"
#include "TileRanges.h"
#include "TileWatermark.h"

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
  };

  void SeenWith(double focalPx) { FocalPx_ = focalPx; }

  void Shapes(const StructureMesher *mesher) { Mesher_ = mesher; }

  void AnchorAt(const Vec3 &ecef);

  int Build(const GroundQuery &ground, const OsmField &field, std::span<const WayLine> ways);

  [[nodiscard]] uint32_t AddedFirst() const { return AddedFirst_; }

  [[nodiscard]] uint32_t AddedCount() const { return AddedCount_; }

  [[nodiscard]] const Raised &Built() const { return Built_; }

  [[nodiscard]] double MeshMs() const { return MeshMs_; }

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

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Prints_) + Built_.HeapBytes() + Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const { return Mark_.Done(field.Features()); }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  static GroundSample RingBase(const GroundQuery &ground,
                               const OsmField &field,
                               const OsmField::Ring &ring,
                               std::vector<double> *corners,
                               double *seatAslM = nullptr);
  [[nodiscard]] static bool TileGroundResolved(
      const GroundQuery &ground, const OsmField &field, size_t from, size_t to, int layer);
  void Raise(const OsmField &field, const Footprint &f);

  const StructureMesher *Mesher_ = nullptr;
  std::vector<Footprint> Prints_;
  Raised Built_;
  double MeshMs_ = 0.0;
  TileRanges ByTile_;
  TileWatermark Mark_;
  double FocalPx_ = 0.0;
  uint32_t AddedFirst_ = 0, AddedCount_ = 0;

  std::vector<double> Corners_;
  Vec3 Anchor_;
  bool Anchored_ = false;
  int OsmHeights_ = 0, DefaultHeights_ = 0, NoGround_ = 0, Fronted_ = 0;
  std::vector<double> SeatSpread_;
  std::vector<double> Across_;
};

} // namespace outshine::Ground
#endif
