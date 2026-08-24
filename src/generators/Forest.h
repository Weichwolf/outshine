#ifndef OUTSHINE_GENERATORS_FOREST_H
#define OUTSHINE_GENERATORS_FOREST_H

#include "AlpineLimit.h"
#include <vector>

#include "Generator.h"
#include "Span.h"

namespace outshine::Generators {

class Forest : public Generator {
public:

  struct Stem {
    double HeightM = 20.0;

    float HeightSigma = 0.0f;
    float TrunkRadiusM = 0.15f;
    float MassKg = 1000.0f;
    ContactMaterial Contact = ContactMaterial{0};
  };

  Forest(Span<const Stem> stems, Span<const float> perM2ByRow, const AlpineLimit &limit);
  Forest(const Stem &stem, Span<const float> perM2ByRow, const AlpineLimit &limit)
      : Forest(Span<const Stem>(&stem, 1), perM2ByRow, limit) {}

  [[nodiscard]] size_t SpeciesCount() const noexcept { return Stems_.size(); }
  [[nodiscard]] size_t SpeciesRefused() const noexcept { return Refused_; }

  static constexpr size_t kMostSpecies = 64;

  enum Note {
    NoTemplate, NoSpecies, ZeroDensity, DensityDraw, AboveTreeline, TooSteep, WoodyDraw,
    HighestStandAslM, kNotes
  };
  [[nodiscard]] Span<const char *const> NoteNames() const noexcept override;

  void Occupy(const Ground &ground, Yield &yield) const noexcept override;
  [[nodiscard]] bool At(const Ground &ground, double eastM, double northM,
                        Body *out) const noexcept override;
  [[nodiscard]] uint32_t Proposes(double areaM2) const noexcept override;

  static constexpr double kCellM = 3.33;

private:
  struct Cell {
    int I = 0, J = 0;
  };
  struct Lattice {
    int Cols = 1, Rows = 1;
    double Em = kCellM, Nm = kCellM;
  };

  enum class Outcome { Placed, NoTemplate, NoSpecies, ZeroDensity, DensityDraw, AboveTreeline,
                       TooSteep, WoodyDraw };

  static Lattice Of(const Region &region);
  [[nodiscard]] Outcome Consider(const Ground &ground, const Lattice &lattice, Cell cell,
                   Body *out) const noexcept;

  std::vector<Stem> Stems_;
  size_t Refused_ = 0;
  Span<const float> PerM2_;
  AlpineLimit Limit_;
};

}
#endif
