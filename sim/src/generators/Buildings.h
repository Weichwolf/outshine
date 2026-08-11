/* WHAT STANDS ON A FOOTPRINT, as a function of the region and the ground under it. The outlines are
 * the vector source's own (Ground::Features), the roof is the height the core resolved for each of
 * them, and this turns the pair into the one answer a simulation needs at a place: is there a
 * structure over me and how high does it reach.
 *
 * IT CLAIMS NO OCCUPANCY, and that is a decision rather than an omission. The engine's substitute
 * body is a cylinder; a terrace is a row of prisms sharing walls, so a circumscribed cylinder cuts
 * its own neighbours and every second house is refused, while an inscribed one leaves the walls
 * unclaimed and buys nothing. The thing that would need a real contact body — something walking into
 * a wall — does not exist yet, and a wrong body standing in for it would be worse than none. What
 * this yields is the point query, which is where the roof is actually asked for. */
#ifndef BUILDINGS_H
#define BUILDINGS_H

#include "FeatureField.h"
#include "Generator.h"

namespace outshine::Generators {

class Buildings : public Generator {
public:
  /* `coverRow` is the row of the declared table an outline of this kind classifies as. The class
   * model is what says which OSM layer that is, so this generator never names one. */
  Buildings(int coverRow, ContactMaterial contact);

  enum Note { Footprints, Roofless, HighestRoofAglM, kNotes };
  Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;

private:
  /* The tallest outline covering the point, because a courtyard building is several rings and the
   * one a point stands under is the one that decides what is over it. */
  const FeatureField::Feature *Over(const Ground &ground, double eastM,
                                    double northM) const noexcept;

  FeatureField::Sieve Sieve_;
  ContactMaterial Contact_;
};

} // namespace outshine::Generators
#endif
