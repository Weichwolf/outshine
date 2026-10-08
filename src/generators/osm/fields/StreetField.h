#ifndef OUTSHINE_GENERATORS_OSM_FIELDS_STREETFIELD_H
#define OUTSHINE_GENERATORS_OSM_FIELDS_STREETFIELD_H

#include <span>
#include <cstdint>
#include <vector>
#include <memory>
#include <optional>

#include "Capacity.h"
#include "OsmField.h"
#include "TileRanges.h"
#include "TileAdmission.h"
#include "VegetationTemplates.h"

namespace outshine::Generators::Osm {

using namespace outshine::Ground;

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
    float SpeedMps = 0.0f;
    int32_t Priority = 0;
    bool Sealed = false;
    bool Oneway = false;
    bool Bridge = false;
  };

  uint32_t Ingest(const OsmField &field, const VegetationTemplates &veg);

  [[nodiscard]] std::optional<uint64_t> SourceDigest(const OsmField &field,
                                                     uint32_t tile) const noexcept;

  [[nodiscard]] const std::vector<Way> &Ways() const { return Ways_; }

  [[nodiscard]] std::span<const Way> OfTile(int tile) const {
    if (tile < 0) { return {}; }
    const TileRanges::Range r = ByTile_.At(static_cast<uint32_t>(tile));
    return {Ways_.data() + r.First, r.Count};
  }

  [[nodiscard]] long UnwidthedCount() const { return Unwidthed_; }

  [[nodiscard]] long UnruledCount() const { return Unruled_; }

  [[nodiscard]] long LookedCount() const { return Looked_; }

  [[nodiscard]] long InvalidLayerCount() const { return InvalidLayers_; }

  [[nodiscard]] long TunnelCount() const { return Tunnels_; }

  [[nodiscard]] long BridgeCount() const { return Bridges_; }

  [[nodiscard]] long LayeredCount() const { return Layered_; }

  [[nodiscard]] long LayerSaidCount() const { return LayerSaid_; }

  void Settle() {
    Ways_.shrink_to_fit();
    SourceDigests_.shrink_to_fit();
  }

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Ways_) + CapacityBytes(SourceDigests_) + Admission_.HeapBytes() +
           ByTile_.HeapBytes();
  }

  [[nodiscard]] bool Ingested(const OsmField &field) const {
    return Admission_.Done(field.Tiles());
  }

  [[nodiscard]] bool IngestedWithin(const OsmField &field, int rings) const {
    return Admission_.AcceptedWithin(field.Tiles(), field.CentreX(), field.CentreY(), rings);
  }

  [[nodiscard]] size_t IngestedTiles() const { return Admission_.Takes(); }

private:
  void AppendFeature(const OsmField &field,
                     const OsmField::Feature &feature,
                     const VegetationTemplates::Rule &rule,
                     Shape shape);
  long InvalidLayers_ = 0;
  std::vector<Way> Ways_;
  std::vector<uint64_t> SourceDigests_;
  std::shared_ptr<const void> SourceOrigin_;
  uint64_t SourceGeneration_ = 0;
  TileRanges ByTile_;
  TileAdmission Admission_;
  long Bridges_ = 0, Layered_ = 0, LayerSaid_ = 0;
  long Unwidthed_ = 0, Tunnels_ = 0, Unruled_ = 0, Looked_ = 0;
};

}
#endif
