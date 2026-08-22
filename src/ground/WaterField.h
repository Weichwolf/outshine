#ifndef WATERFIELD_H
#define WATERFIELD_H

#include "OsmField.h"
#include "VegetationTemplates.h"

#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "Span.h"
#include "TileRanges.h"
#include "TerrainLoader.h"
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

  uint32_t Ingest(const GroundStream &ground, const OsmField &field,
                  const VegetationTemplates &veg);

  void AnchorAt(const double ecef[3]);

  const std::vector<Surface> &Surfaces() const { return Surfaces_; }

  Span<const Surface> OfTile(int tile) const {
    if (tile < 0) return Span<const Surface>();
    const TileRanges::Range r = ByTile_.At((uint32_t)tile);
    return Span<const Surface>(Surfaces_.data() + r.First, r.Count);
  }
  const std::vector<Course> &Courses() const { return Courses_; }
  const std::vector<float> &Levels() const { return Levels_; }
  const double *Anchor() const { return Anchor_; }

  void Tessellate(const OsmField &field, std::vector<float> &out) const;

  size_t HeapBytes() const {
    return CapacityBytes(Surfaces_) + CapacityBytes(Courses_) + CapacityBytes(Levels_) +
           Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  long NoGroundCount() const { return NoGround_; }
  long OutlierCount() const { return Outliers_; }
  int Deferrals() const { return Mark_.Deferrals(); }

private:

  [[nodiscard]] bool TileGroundResolved(const GroundStream &ground, const OsmField &field,
                                        size_t from, size_t to, int poly, int line) const;
  std::vector<Surface> Surfaces_;
  std::vector<Course> Courses_;
  std::vector<float> Levels_;
  TileRanges ByTile_;
  double Anchor_[3] = {0, 0, 0};
  bool Anchored_ = false;
  TileWatermark Mark_;
  long NoGround_ = 0, Outliers_ = 0;
};

}
#endif
