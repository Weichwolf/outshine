/* OSM building footprints, extruded to prisms.
 *
 * WHY THE FOOTPRINT SURVIVES THE MESH. A Footprint keeps its ring, its height, its base and where the
 * height came from, and Tessellate() is a pure function of it — so a roof generator, a facade
 * parameterisation or a per-material split is another READER of the same record, added beside the
 * extrusion instead of replacing it. That is also why every wall vertex carries (run along the wall,
 * height above base) in metres rather than a 0..1 uv: floor lines, window grids and storey counts are
 * all functions of those two numbers, and nothing about them has to be decided today.
 *
 * The ring is NOT stored here — it is OsmField's, and a Footprint is an index into it. The ground
 * class under this house and the kerb in front of it have to come off the same geometry, and a second
 * parse is a second chance to disagree.
 *
 * The one thing the tile does not give us is a real height for most buildings — see kFillHeightM. */
#ifndef BUILDINGFIELD_H
#define BUILDINGFIELD_H

#include "OsmField.h"

#include <cstdint>
#include <vector>

#include "Capacity.h"

namespace outshine::World {

class BuildingField {
public:
  enum class HeightSource : uint8_t { Osm, Default };

  struct Footprint {
    uint32_t FirstPoint = 0, PointCount = 0;   /* into OsmField::Points(), lat/lon, ring not closed */
    float HeightM = 0.0f;
    float BaseM = 0.0f;                        /* lowest terrain under the ring, ASL */
    HeightSource Source = HeightSource::Default;
  };

  /* Extrudes whatever `field` has decoded and this has not seen yet. Returns the number of footprints
   * standing; 0 while the tiles are still streaming.
   *
   * AT MOST ONE TILE PER CALL, because everything downstream of a decoded tile — extrusion and the
   * cluster DAG — is main-thread work in the frame that asked. A ring of nine landing together is one
   * frame doing nine tiles' worth of it, and that frame is gone. The stop is on the FEATURE'S TILE,
   * not on the field's schedule, so a field that later runs ahead cannot undo it. */
  int Build(const OsmField &field);

  /* The vertex range `Verts_` grew by in the last Build that consumed something, as float indices.
   * Count 0 = nothing new, and the caller's derived data is still current. */
  uint32_t AddedFirst() const { return AddedFirst_; }
  uint32_t AddedCount() const { return AddedCount_; }

  /* core/ChunkVtx.h's layout, uv = (metres along the wall, metres above the base). Camera-relative
   * is the caller's business: positions are ECEF offsets from Anchor(). */
  const std::vector<float> &Verts() const { return Verts_; }
  const double *Anchor() const { return Anchor_; }
  const std::vector<Footprint> &Footprints() const { return Prints_; }
  /* THE ROOF OVER A POINT, ASL, or -1e30 where no footprint stands. An eye inside a wall is not a
   * standpoint, and this is the only thing that can say so: the extrusion is a prism over a ring and
   * the ring lives in `field`. */
  double RoofAslAt(const OsmField &field, double lat, double lon) const;
  int OsmHeights() const { return OsmHeights_; }
  int DefaultHeights() const { return DefaultHeights_; }
  size_t HeapBytes() const { return CapacityBytes(Prints_) + CapacityBytes(Verts_); }

private:
  void Extrude(const OsmField &field, const Footprint &f);

  std::vector<Footprint> Prints_;
  std::vector<float> Verts_;
  uint32_t Consumed_ = 0;          /* into OsmField::Features() — the watermark, never rewound */
  uint32_t AddedFirst_ = 0, AddedCount_ = 0;
  double Anchor_[3] = {0, 0, 0};
  bool HaveAnchor_ = false;
  int OsmHeights_ = 0, DefaultHeights_ = 0, NoGround_ = 0;
};

} // namespace outshine::World
#endif
