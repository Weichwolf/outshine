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

struct StructurePlan {
  /* The outline as the vector source has it: lat/lon pairs, exterior, not closed. */
  Span<const double> RingLatLon;
  double BaseAslM = 0.0;
  /* To the TOP of the structure — ridge, parapet or cap, whichever the roof turns out to be. Where
   * it was not measured it is the classifier's default, and a generator is told which so it can
   * treat an unknown as an unknown rather than as a fact. */
  double HeightM = 0.0;
  bool HeightMeasured = false;
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
