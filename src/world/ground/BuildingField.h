#ifndef OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H
#define OUTSHINE_WORLD_GROUND_BUILDINGFIELD_H

#include "OsmField.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "GroundSample.h"
#include "GroundQuery.h"
#include "StructureMesher.h"
#include "Span.h"
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
    HeightSource Source = HeightSource::Default;
    Frontage Street;
  };

  void SeenWith(double focalPx) { FocalPx_ = focalPx; }

  void Shapes(const StructureMesher *mesher) { Mesher_ = mesher; }

  void AnchorAt(const double ecef[3]);

  int Build(const GroundQuery &ground, const OsmField &field, Span<const WayLine> ways);

  uint32_t AddedFirst() const { return AddedFirst_; }

  uint32_t AddedCount() const { return AddedCount_; }

  const std::vector<float> &Verts() const { return Verts_; }

  const double *Anchor() const { return Anchor_; }

  const std::vector<Footprint> &Footprints() const { return Prints_; }

  Span<const Footprint> OfTile(int tile) const {
    if (tile < 0) { return Span<const Footprint>(); }
    const TileRanges::Range r = ByTile_.At((uint32_t)tile);
    return Span<const Footprint>(Prints_.data() + r.First, r.Count);
  }

  int OsmHeights() const { return OsmHeights_; }

  int DefaultHeights() const { return DefaultHeights_; }

  int Deferrals() const { return Mark_.Deferrals(); }

  size_t HeapBytes() const {
    return CapacityBytes(Prints_) + CapacityBytes(Verts_) + Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const { return Mark_.Done(field.Features()); }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  static GroundSample RingBase(const GroundQuery &ground,
                               const OsmField &field,
                               const OsmField::Ring &ring,
                               std::vector<double> *corners);
  [[nodiscard]] bool TileGroundResolved(
      const GroundQuery &ground, const OsmField &field, size_t from, size_t to, int layer) const;
  void Raise(const OsmField &field, const Footprint &f);

  const StructureMesher *Mesher_ = nullptr;
  std::vector<Footprint> Prints_;
  std::vector<float> Verts_;
  TileRanges ByTile_;
  TileWatermark Mark_;
  double FocalPx_ = 0.0;
  uint32_t AddedFirst_ = 0, AddedCount_ = 0;

  std::vector<double> Corners_;
  double Anchor_[3] = {0, 0, 0};
  bool Anchored_ = false;
  int OsmHeights_ = 0, DefaultHeights_ = 0, NoGround_ = 0, Fronted_ = 0;
};

} // namespace outshine::Ground
#endif
