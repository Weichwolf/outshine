#ifndef OUTSHINE_WORLD_GROUND_STREETFIELD_H
#define OUTSHINE_WORLD_GROUND_STREETFIELD_H

#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"
#include "Span.h"
#include "TileRanges.h"
#include "TileWatermark.h"
#include "VegetationTemplates.h"

namespace outshine::Ground {

class StreetField {
public:
  enum class Shape : uint8_t { Ribbon, Area };

  struct Way {
    uint32_t FirstPoint = 0, PointCount = 0;
    float HalfWidthM = 0.0f;
    int32_t CoverRow = -1;
    Shape Form = Shape::Ribbon;
  };

  uint32_t Ingest(const OsmField &field, const VegetationTemplates &veg);

  const std::vector<Way> &Ways() const { return Ways_; }

  Span<const Way> OfTile(int tile) const {
    if (tile < 0) { return Span<const Way>(); }
    const TileRanges::Range r = ByTile_.At((uint32_t)tile);
    return Span<const Way>(Ways_.data() + r.First, r.Count);
  }

  long UnwidthedCount() const { return Unwidthed_; }

  long TunnelCount() const { return Tunnels_; }

  size_t HeapBytes() const {
    return CapacityBytes(Ways_) + Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const { return Mark_.Done(field.Features()); }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  std::vector<Way> Ways_;
  TileRanges ByTile_;
  TileWatermark Mark_;
  long Unwidthed_ = 0, Tunnels_ = 0;
};

}
#endif
