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

  /* Consumes whatever `field` has decoded and this has not seen yet; returns the surfaces standing. */
  /* The width of a watercourse is DECLARED per kind (vegetation.json, water_lines: drain 1.0 m to
   * river 12.0 m); one number for all of them drew a river as a ditch. */
  uint32_t Ingest(const OsmField &field, const VegetationTemplates &veg);

  const std::vector<Surface> &Surfaces() const { return Surfaces_; }
  const std::vector<Course> &Courses() const { return Courses_; }
  const std::vector<float> &Levels() const { return Levels_; }
  const double *Anchor() const { return Anchor_; }
  bool HaveAnchor() const { return HaveAnchor_; }

  /* pos3 + nrm3 per vertex, ECEF relative to Anchor(). A pure function of the surfaces. */
  void Tessellate(const OsmField &field, std::vector<float> &out) const;

  size_t HeapBytes() const {
    return CapacityBytes(Surfaces_) + CapacityBytes(Courses_) + CapacityBytes(Levels_);
  }

  long NoGroundCount() const { return NoGround_; }
  long OutlierCount() const { return Outliers_; }

private:
  std::vector<Surface> Surfaces_;
  std::vector<Course> Courses_;
  std::vector<float> Levels_;
  double Anchor_[3] = {0, 0, 0};
  bool HaveAnchor_ = false;
  uint32_t Consumed_ = 0;
  long NoGround_ = 0, Outliers_ = 0;
};

} // namespace outshine::World
#endif
