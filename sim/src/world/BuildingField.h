/* OSM building footprints, and the ground and height resolved on each of them.
 *
 * WHAT THIS DOES NOT DECIDE: the shape. A Footprint keeps its ring, its height, its base and where
 * the height came from; what stands on it is a StructureMesher's answer, installed from above
 * (core/StructureMesher.h). That is what lets the mass, the roof and the facade be a generator while
 * the streaming, the watermark and the ground sampling stay here — and it is why a target with no
 * renderer installs no mesher and builds no geometry at all.
 *
 * The ring is NOT stored here — it is OsmField's, and a Footprint is an index into it. The ground
 * class under this house and the kerb in front of it have to come off the same geometry, and a second
 * parse is a second chance to disagree.
 *
 * The one thing the tile does not give us is a real height for most buildings — see kFillHeightM. */
#ifndef BUILDINGFIELD_H
#define BUILDINGFIELD_H

#include "OsmField.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Capacity.h"
#include "GroundSample.h"
#include "StructureMesher.h"
#include "Span.h"
#include "TileRanges.h"
#include "TileWatermark.h"

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

  /* WHO TURNS AN OUTLINE INTO TRIANGLES. Installed once, before anything streams; without one this
   * field resolves footprints and builds no geometry at all, which is exactly what a target with no
   * renderer wants. */
  void Shapes(const StructureMesher *mesher) { Mesher_ = mesher; }

  /* Raises whatever `field` has decoded and this has not seen yet. Returns the number of footprints
   * standing; 0 while the tiles are still streaming.
   *
   * AT MOST ONE TILE PER CALL, because everything downstream of a decoded tile — extrusion and the
   * cluster DAG — is main-thread work in the frame that asked. A ring of nine landing together is one
   * frame doing nine tiles' worth of it, and that frame is gone. The stop is on the FEATURE'S TILE,
   * not on the field's schedule, so a field that later runs ahead cannot undo it.
   *
   * DEFERRAL IS PER TILE (world/TileWatermark.h). */
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
  /* One tile's footprints, by OsmField tile index. Good until the next Build(). */
  Span<const Footprint> OfTile(int tile) const {
    if (tile < 0) return Span<const Footprint>();
    const TileRanges::Range r = ByTile_.At((uint32_t)tile);
    return Span<const Footprint>(Prints_.data() + r.First, r.Count);
  }
  int OsmHeights() const { return OsmHeights_; }
  int DefaultHeights() const { return DefaultHeights_; }
  int Deferrals() const { return Mark_.Deferrals(); }
  size_t HeapBytes() const {
    return CapacityBytes(Prints_) + CapacityBytes(Verts_) + Mark_.HeapBytes() +
           ByTile_.HeapBytes();
  }

private:
  /* A ring's lowest corner, carrying WHY when there is none — the ground's own answer, not a copy of
   * its shape: two records with the same three fields drift apart the moment one of them is read. */
  static GroundSample RingBase(const OsmField &field, const OsmField::Ring &ring);
  bool TileGroundResolved(const OsmField &field, size_t from, size_t to, int layer) const;
  void Raise(const OsmField &field, const Footprint &f);

  const StructureMesher *Mesher_ = nullptr;
  std::vector<Footprint> Prints_;
  std::vector<float> Verts_;
  TileRanges ByTile_;
  TileWatermark Mark_;
  uint32_t AddedFirst_ = 0, AddedCount_ = 0;
  double Anchor_[3] = {0, 0, 0};
  bool HaveAnchor_ = false;
  int OsmHeights_ = 0, DefaultHeights_ = 0, NoGround_ = 0;
};

} // namespace outshine::World
#endif
