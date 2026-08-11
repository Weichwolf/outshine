/* WHAT STANDS ON AN OUTLINE, as geometry — the seam between the half that knows WHERE a structure is
 * and the half that decides WHAT it looks like. The streamer resolves the ring, the ground under it
 * and a height; a generator turns that into mass, roof and facade. Neither can name the other, so the
 * statement of what passes between them is here and nowhere else.
 *
 * The server target installs no mesher and therefore builds no soup: a machine with no device has
 * nothing to draw and, until now, was extruding a town anyway. */
#ifndef STRUCTUREMESHER_H
#define STRUCTUREMESHER_H

#include <vector>

#include "Span.h"

namespace outshine {

/* A WAY'S CENTRELINE AND THE WIDTH ITS CLASS DECLARES, as a value, so the field that resolves
 * footprints and the field that resolves ways agree about a street without naming each other. */
struct WayLine {
  Span<const double> LatLon;   /* the centreline, lat/lon pairs, not closed */
  double HalfWidthM = 0.0;
  double MinLat = 0.0, MinLon = 0.0, MaxLat = 0.0, MaxLon = 0.0;
};

/* THE STREET A FOOTPRINT LOOKS AT, as a straight kerb line in the plan's own frame — the tangent of
 * the nearest way at the nearest point, offset to the edge of the carriageway. A way is a polyline
 * and this is a chord of it; over the depth of one plot that is the difference between a kerb and a
 * kerb drawn 2 cm out, and a polyline in the mesher would be a second copy of the street. */
struct Frontage {
  bool Known = false;
  /* Metres east/north of the outline's FIRST CORNER, which is the frame the mesher works in. */
  double KerbEm = 0.0, KerbNm = 0.0;
  double AlongE = 0.0, AlongN = 0.0;         /* unit, along the kerb */
  double ToStreetE = 0.0, ToStreetN = 0.0;   /* unit, from the building towards the carriageway */
};

struct StructurePlan {
  /* The outline as the vector source has it: lat/lon pairs, exterior, not closed. */
  Span<const double> RingLatLon;
  double BaseAslM = 0.0;
  /* The terrain ASL under each of those corners, one per corner. A single base cannot carry a
   * pavement: an apron four metres wide follows the ground and a building's base does not. */
  Span<const double> CornerAslM;
  /* To the TOP of the structure — ridge, parapet or cap, whichever the roof turns out to be. Where
   * it was not measured it is the classifier's default, and a generator is told which so it can
   * treat an unknown as an unknown rather than as a fact. */
  double HeightM = 0.0;
  bool HeightMeasured = false;
  Frontage Street;
  /* Every position the mesher writes is an offset from this, in metres, ECEF. */
  const double *AnchorEcef = nullptr;
};

class StructureMesher {
public:
  virtual ~StructureMesher() = default;
  StructureMesher(const StructureMesher &) = delete;
  StructureMesher &operator=(const StructureMesher &) = delete;

  /* Appends core/ChunkVtx.h vertices, three per triangle, no index. The soup is the caller's so its
   * capacity survives a town (`F.20`, the hot-loop exception). */
  virtual void Mesh(const StructurePlan &plan, std::vector<float> &soup) const noexcept = 0;

protected:
  StructureMesher() = default;
};

}  // namespace outshine
#endif
