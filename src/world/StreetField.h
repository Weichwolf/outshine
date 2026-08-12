/* OSM ways as made surfaces: the carriageway, the track, the footpath, the rail bed and the paved
 * area, each with the width its class declares.
 *
 * Same shape as BuildingField and WaterField, and for the same reason: the ring stays OsmField's and
 * a Way is an index into it, so the strip the class grid rasterises and the strip a generator answers
 * from cannot be two parses of one line. What this adds over the class grid is the WAY ITSELF — the
 * grid knows only that a point classifies as sealed, not that it lies on a 5.5 m residential street.
 *
 * NO GROUND IS RESOLVED HERE, and that is the difference to the other two. A house has one base and a
 * lake one level; a way is graded onto the terrain and its surface height is the ground at the point
 * asked for. Carrying a per-vertex height would be a second model of the terrain that can disagree
 * with the drawn one. */
#ifndef STREETFIELD_H
#define STREETFIELD_H

#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "OsmField.h"
#include "Span.h"
#include "TileRanges.h"
#include "TileWatermark.h"
#include "VegetationTemplates.h"

namespace outshine::World {

class StreetField {
public:
  enum class Shape : uint8_t { Ribbon, Area };

  struct Way {
    uint32_t FirstPoint = 0, PointCount = 0;   /* into OsmField::Points(), lat/lon */
    float HalfWidthM = 0.0f;                   /* Ribbon only — an Area's extent is its own ring */
    int32_t CoverRow = -1;                     /* the declared table's row this surface classifies as */
    Shape Form = Shape::Ribbon;
  };

  /* Consumes ONE tile of whatever `field` has decoded and this has not seen yet; returns the ways
   * standing. The width per kind is the class table's (vegetation.json, streets: 1.5 m path to 45 m
   * runway) — the same number the classifier rasterises with, read once here instead of per query. */
  uint32_t Ingest(const OsmField &field, const VegetationTemplates &veg);

  const std::vector<Way> &Ways() const { return Ways_; }
  /* One tile's ways, by OsmField tile index. Good until the next Ingest(). */
  Span<const Way> OfTile(int tile) const {
    if (tile < 0) return Span<const Way>();
    const TileRanges::Range r = ByTile_.At((uint32_t)tile);
    return Span<const Way>(Ways_.data() + r.First, r.Count);
  }

  long UnwidthedCount() const { return Unwidthed_; }
  long TunnelCount() const { return Tunnels_; }
  size_t HeapBytes() const {
    return CapacityBytes(Ways_) + Mark_.HeapBytes() + ByTile_.HeapBytes();
  }

private:
  std::vector<Way> Ways_;
  TileRanges ByTile_;
  TileWatermark Mark_;
  long Unwidthed_ = 0, Tunnels_ = 0;
};

} // namespace outshine::World
#endif
