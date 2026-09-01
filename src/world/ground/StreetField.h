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
    int32_t Lanes = 0;
    int32_t Layer = 0;
    float ClearanceM = 0.0f;
    float MaxGradient = 0.0f;
    bool Bridge = false;
  };

  uint32_t Ingest(const OsmField &field, const VegetationTemplates &veg);

  [[nodiscard]] const std::vector<Way> &Ways() const { return Ways_; }

  [[nodiscard]] Span<const Way> OfTile(int tile) const {
    if (tile < 0) { return {}; }
    const TileRanges::Range r = ByTile_.At(static_cast<uint32_t>(tile));
    return {Ways_.data() + r.First, r.Count};
  }

  [[nodiscard]] long UnwidthedCount() const { return Unwidthed_; }

  [[nodiscard]] long UnruledCount() const { return Unruled_; }

  [[nodiscard]] long LookedCount() const { return Looked_; }

  [[nodiscard]] long TunnelCount() const { return Tunnels_; }

  [[nodiscard]] long BridgeCount() const { return Bridges_; }

  [[nodiscard]] long LayeredCount() const { return Layered_; }

  [[nodiscard]] long LayerSaidCount() const { return LayerSaid_; }

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Ways_) + Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const { return Mark_.Done(field.Features()); }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  std::vector<Way> Ways_;
  TileRanges ByTile_;
  TileWatermark Mark_;
  long Bridges_ = 0, Layered_ = 0, LayerSaid_ = 0;
  long Unwidthed_ = 0, Tunnels_ = 0, Unruled_ = 0, Looked_ = 0;
};

} // namespace outshine::Ground
#endif
