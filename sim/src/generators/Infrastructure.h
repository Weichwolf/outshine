/* WHAT IS MADE AT A PLACE: the road, the track, the footpath, the rail bed and the paved area, as
 * the one answer a simulation needs — am I on a made surface, of what kind, and how wide is the way
 * it belongs to. The centrelines and their declared widths come from the core (Ground::Features);
 * this is the half that turns them into a query.
 *
 * IT CLAIMS NO OCCUPANCY, for a measured reason rather than by analogy: the class model already
 * gives every sealed row 0 trees/m2, so nothing grows on a carriageway and a claim would buy an
 * empty strip twice. What a way DOES need eventually is a contact material — asphalt, gravel and
 * ballast answer a wheel differently — and that arrives with the physics that asks, not before.
 *
 * THE HEIGHT IT ANSWERS IS THE GROUND'S. A way is graded onto the terrain, so it carries no base of
 * its own (generators/FeatureLevel.h) and the surface at a point is the patch's height there. The
 * day an embankment is built, the way gains a base and this answer changes with it — not the
 * caller. */
#ifndef INFRASTRUCTURE_H
#define INFRASTRUCTURE_H

#include <optional>

#include "FeatureField.h"
#include "Generator.h"

namespace outshine::Generators {

class Infrastructure : public Generator {
public:
  /* The widest way covering the point, because a service road crossing a motorway is not what the
   * place is: the wider way is the one that decides what the surface is made of. */
  struct Made {
    int32_t CoverRow = -1;
    float WidthM = 0.0f;
    double SurfaceAslM = 0.0;
  };
  [[nodiscard]] std::optional<Made> MadeAt(const Ground &ground, double eastM,
                                           double northM) const noexcept;

  enum Note { Ways, WidestM, kNotes };
  Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;
};

} // namespace outshine::Generators
#endif
