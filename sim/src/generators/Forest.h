/* WHERE TREES STAND, as a function of the region and the ground under it. One cell of the region's
 * own lattice proposes one stand; every way out of that decision has a name, so the counts partition
 * the region and a missing tree can be attributed instead of guessed.
 *
 * The lattice is the REGION'S OWN subdivision, which is what makes the border exact: a cell belongs
 * to one region, a point of the plane lies in one region's cell, and neither a gap nor a doubled
 * stand can be written. */
#ifndef FOREST_H
#define FOREST_H

#include "AlpineLimit.h"
#include "Generator.h"
#include "Span.h"

namespace outshine::Generators {

class Forest : public Generator {
public:
  /* What a stand is, before the ground has a say — the species declaration reduced to what standing
   * costs space. The mesh is the picture's business and is not named here. */
  struct Stem {
    double HeightM = 20.0;
    /* The species' relative spread of stand height, 1 sigma of the triangular draw. */
    float HeightSigma = 0.0f;
    float TrunkRadiusM = 0.15f;
    float MassKg = 1000.0f;
    ContactMaterial Contact = ContactMaterial{0};
  };

  /* Stands per square metre by ground class row, indexed as the ground's own table is. A row past
   * the end grows nothing, which is what an unclassified place does too. */
  Forest(const Stem &stem, Span<const float> perM2ByRow, const AlpineLimit &limit);

  enum Note {
    NoTemplate, ZeroDensity, DensityDraw, AboveTreeline, TooSteep, WoodyDraw, HighestStandAslM,
    kNotes
  };
  Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;

  /* [SET] metres. WELL below the mean stand spacing: at 11.09 m2 the densest declared class
   * (conifer forest, 0.033/m2) accepts 36.6 % of cells, two in three stay empty, and that is what
   * stops the lattice from reading as rows. At the stand spacing itself the acceptance probability
   * would be 1 and the forest a raster. */
  static constexpr double kCellM = 3.33;

private:
  struct Cell {
    int I = 0, J = 0;
  };
  struct Lattice {
    int Cols = 1, Rows = 1;
    double Em = kCellM, Nm = kCellM;
  };
  /* Placed carries the body; every other value is the reason there is none. */
  enum class Outcome { Placed, NoTemplate, ZeroDensity, DensityDraw, AboveTreeline, TooSteep,
                       WoodyDraw };

  static Lattice Of(const Region &region);
  Outcome Consider(const Ground &ground, const Lattice &lattice, Cell cell,
                   Body *out) const noexcept;

  Stem Stem_;
  Span<const float> PerM2_;
  AlpineLimit Limit_;
};

} // namespace outshine::Generators
#endif
