/* HOW DEEP THE WATER IS OVER A PLACE. The outline and its level come from the core — the core owns
 * "where water is and its level" and the generator owns "how water looks" — and this is the half
 * that turns those into an answer a simulation can use.
 *
 * IT CLAIMS NO OCCUPANCY. Water is a medium, not a contact: nothing stands on it and a body that
 * substituted for it would be a solid where the physics wants a force source. Its yield is the point
 * query and the counts beside it, and one of those counts is the interesting one — how often the
 * level model and the DEM disagree about this region, which is the state core/WaterDepth.h carries
 * instead of a negative metre.
 *
 * WATERCOURSES ARE NOT IN IT. A ribbon's level belongs to its rung and falls along the way, so a
 * point query would have to interpolate along a line that is one to twelve metres wide — below the
 * DEM's own posting spacing everywhere the demo stands. Areas only, and that is stated rather than
 * silently true. */
#ifndef WATER_H
#define WATER_H

#include "FeatureField.h"
#include "Generator.h"
#include "WaterDepth.h"

namespace outshine::Generators {

class Water : public Generator {
public:
  /* THE ANSWER, and it cannot be negative (core/WaterDepth.h). */
  [[nodiscard]] WaterDepth DepthAt(const Ground &ground, double eastM,
                                   double northM) const noexcept;

  enum Note { Surfaces, Untested, LevelBelowGround, DeepestM, kNotes };
  Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;
};

} // namespace outshine::Generators
#endif
