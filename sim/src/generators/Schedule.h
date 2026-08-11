#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <optional>
#include <vector>

#include "Region.h"

namespace outshine::Generators {

class Schedule {
public:
  /* A zoom and a radius in regions, both statements about place: the scheduler knows a distance and
   * a region size and nothing else about what is generated. */
  struct Ring {
    /* [SET] the tier the vector source is read at, so a region is one OSM tile and no second grid
     * exists: 1506 m per side at 52 deg N, 2445 m at the equator. */
    int Zoom = 14;
    /* [SET] regions around the camera's own, so a 3x3 ring — the standpoint's region plus the eight
     * that can hold something visible over its edge. What replaces it is a view distance measured
     * against the region size, not a preference. */
    int RadiusRegions = 1;
  };

  explicit Schedule(const Ring &ring);

  int Zoom() const { return Zoom_; }
  size_t Count() const { return Offsets_.size(); }

  /* The i-th nearest region to a place, nearest first. None where the ring reaches past a pole. */
  std::optional<Region> At(size_t i, double lat, double lon) const;

  /* The ring member with the most ground in it — the one every buffer set has to be able to hold.
   * A Mercator row shrinks with cos(latitude), so it is the equator-most, never the centre. */
  std::optional<Region> Widest(double lat, double lon) const;

private:
  struct Offset {
    int X, Y;
  };

  int Zoom_;
  std::vector<Offset> Offsets_;
};

} // namespace outshine::Generators
#endif
