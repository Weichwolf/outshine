/* OSM water surfaces as their own geometry, level by construction.
 *
 * Water was a ground class painted onto the terrain mesh, which makes a lake follow the hillside under
 * it and a 3 m brook a sampling problem on a 16 m grid — measured 17 % coverage over 731 m of the
 * Hannover canal. A surface cannot be level because a shader says so;
 * it is level because its mesh is.
 *
 * Same shape as BuildingField, and deliberately: a lake is a footprint whose height comes from the
 * shore instead of from a tag. The ring stays OsmField's and a Surface is an index into it, so the
 * class under the bank and the water on it can never be two parses of the same line. */
#ifndef WATERFIELD_H
#define WATERFIELD_H

#include "OsmField.h"
#include "VegetationTemplates.h"

#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "Span.h"
#include "TileRanges.h"
#include "TileWatermark.h"

namespace outshine::World {

class WaterField {
public:
  struct Surface {
    uint32_t FirstPoint = 0, PointCount = 0;   /* into OsmField::Points(), lat/lon, ring not closed */
    float LevelM = 0.0f;                       /* the water surface, ASL — ONE value for the whole ring */
  };

  /* A watercourse. Level across it, falling along it, so the level belongs to the VERTEX and the
   * ribbon is level across each rung. */
  struct Course {
    uint32_t FirstPoint = 0, PointCount = 0;
    uint32_t FirstLevel = 0;                   /* into Levels(), PointCount entries */
    float HalfWidthM = 0.0f;
  };

  /* Consumes ONE tile of whatever `field` has decoded and this has not seen yet; returns the
   * surfaces standing. The width of a watercourse is DECLARED per kind (vegetation.json,
   * water_lines: drain 1.0 m to river 12.0 m); one number for all of them drew a river as a ditch.
   *
   * DEFERRAL IS PER TILE (world/TileWatermark.h). A single watermark folded "the DEM has not landed"
   * into "there is no water here" and advanced past it, so a lake whose elevation arrived one pass
   * late was dropped for the whole run. */
  uint32_t Ingest(const OsmField &field, const VegetationTemplates &veg);

  const std::vector<Surface> &Surfaces() const { return Surfaces_; }
  /* One tile's surfaces, by OsmField tile index. Good until the next Ingest(). */
  Span<const Surface> OfTile(int tile) const {
    if (tile < 0) return Span<const Surface>();
    const TileRanges::Range r = ByTile_.At((uint32_t)tile);
    return Span<const Surface>(Surfaces_.data() + r.First, r.Count);
  }
  const std::vector<Course> &Courses() const { return Courses_; }
  const std::vector<float> &Levels() const { return Levels_; }
  const double *Anchor() const { return Anchor_; }
  [[nodiscard]] bool HaveAnchor() const { return HaveAnchor_; }

  /* pos3 + nrm3 per vertex, ECEF relative to Anchor(). A pure function of the surfaces. */
  void Tessellate(const OsmField &field, std::vector<float> &out) const;

  size_t HeapBytes() const {
    return CapacityBytes(Surfaces_) + CapacityBytes(Courses_) + CapacityBytes(Levels_) +
           Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

  long NoGroundCount() const { return NoGround_; }
  long OutlierCount() const { return Outliers_; }
  int Deferrals() const { return Mark_.Deferrals(); }

private:
  /* Every ring point of every water feature of this tile has a height, so the tile can be taken
   * whole. Read twice per tile — the second read is a hit in the oracle's own tile cache. */
  [[nodiscard]] bool TileGroundResolved(const OsmField &field, size_t from, size_t to, int poly, int line) const;
  std::vector<Surface> Surfaces_;
  std::vector<Course> Courses_;
  std::vector<float> Levels_;
  TileRanges ByTile_;
  double Anchor_[3] = {0, 0, 0};
  bool HaveAnchor_ = false;
  TileWatermark Mark_;
  long NoGround_ = 0, Outliers_ = 0;
};

} // namespace outshine::World
#endif
