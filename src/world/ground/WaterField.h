#ifndef OUTSHINE_WORLD_GROUND_WATERFIELD_H
#define OUTSHINE_WORLD_GROUND_WATERFIELD_H

#include "math/Vec3.h"
#include "OsmField.h"
#include "VegetationTemplates.h"

#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "Span.h"
#include "TileRanges.h"
#include "GroundQuery.h"
#include "TileWatermark.h"

namespace outshine::Ground {

class WaterField {
public:
  struct Surface {
    uint32_t FirstPoint = 0, PointCount = 0;
    float LevelM = 0.0f;
  };

  struct Course {
    uint32_t FirstPoint = 0, PointCount = 0;
    uint32_t FirstLevel = 0;
    float HalfWidthM = 0.0f;
  };

  uint32_t Ingest(const GroundQuery &ground, const OsmField &field, const VegetationTemplates &veg);

  void AnchorAt(const Vec3 &ecef);

  [[nodiscard]] const std::vector<Surface> &Surfaces() const { return Surfaces_; }

  [[nodiscard]] Span<const Surface> OfTile(int tile) const {
    if (tile < 0) { return {}; }
    const TileRanges::Range r = ByTile_.At(static_cast<uint32_t>(tile));
    return {Surfaces_.data() + r.First, r.Count};
  }

  [[nodiscard]] const std::vector<Course> &Courses() const { return Courses_; }

  [[nodiscard]] const std::vector<float> &Levels() const { return Levels_; }

  [[nodiscard]] const Vec3 &Anchor() const { return Anchor_; }

  void Tessellate(const OsmField &field, std::vector<float> &out) const;

  [[nodiscard]] size_t HeapBytes() const {
    return CapacityBytes(Surfaces_) + CapacityBytes(Courses_) + CapacityBytes(Levels_) +
           Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  [[nodiscard]] long NoGroundCount() const { return NoGround_; }

  [[nodiscard]] long OutlierCount() const { return Outliers_; }

  [[nodiscard]] int Deferrals() const { return Mark_.Deferrals(); }

  [[nodiscard]] bool Ingested(const OsmField &field) const { return Mark_.Done(field.Features()); }

  [[nodiscard]] size_t IngestedTiles() const { return Mark_.Takes(); }

private:
  [[nodiscard]] static bool TileGroundResolved(
      const GroundQuery &ground, const OsmField &field, size_t from, size_t to, int poly, int line);
  std::vector<Surface> Surfaces_;
  std::vector<Course> Courses_;
  std::vector<float> Levels_;
  TileRanges ByTile_;
  Vec3 Anchor_;
  bool Anchored_ = false;
  TileWatermark Mark_;
  long NoGround_ = 0, Outliers_ = 0;
};

} // namespace outshine::Ground
#endif
