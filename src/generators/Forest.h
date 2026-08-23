#ifndef OUTSHINE_GENERATORS_FOREST_H
#define OUTSHINE_GENERATORS_FOREST_H

#include "AlpineLimit.h"
#include "Generator.h"
#include "Span.h"

namespace outshine::Generators {

class Forest : public Generator {
public:

  struct Stem {
    // [SET] the default stem, a mature temperate broadleaf: 20 m tall (measured, European
    // beech at canopy age), 0.15 m of trunk radius at breast height and about a tonne of
    // standing mass -- a scenario that means another tree declares one
    double HeightM = 20.0;

    float HeightSigma = 0.0f;
    float TrunkRadiusM = 0.15f;
    float MassKg = 1000.0f;
    ContactMaterial Contact = ContactMaterial{0};
  };

  Forest(const Stem &stem, Span<const float> perM2ByRow, const AlpineLimit &limit);

  enum Note {
    NoTemplate, ZeroDensity, DensityDraw, AboveTreeline, TooSteep, WoodyDraw, HighestStandAslM,
    kNotes
  };
  [[nodiscard]] Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;

  // derived: the placement lattice's cell, 3.33 m -- one stem per cell at the densest
  // declared stand (0.09 stems/m2, closed temperate canopy) is exactly 1/sqrt(0.09)
  static constexpr double kCellM = 3.33;

private:
  struct Cell {
    int I = 0, J = 0;
  };
  struct Lattice {
    int Cols = 1, Rows = 1;
    double Em = kCellM, Nm = kCellM;
  };

  enum class Outcome { Placed, NoTemplate, ZeroDensity, DensityDraw, AboveTreeline, TooSteep,
                       WoodyDraw };

  static Lattice Of(const Region &region);
  [[nodiscard]] Outcome Consider(const Ground &ground, const Lattice &lattice, Cell cell,
                   Body *out) const noexcept;

  Stem Stem_;
  Span<const float> PerM2_;
  AlpineLimit Limit_;
};

}
#endif
