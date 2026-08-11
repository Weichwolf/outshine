#ifndef GROUND_H
#define GROUND_H

#include <memory>
#include <optional>

#include "ClassStructure.h"
#include "Cover.h"
#include "FeatureField.h"
#include "GroundPatch.h"
#include "GroundTable.h"
#include "Region.h"

namespace outshine::Generators {

class Ground {
public:
  /* Held as snapshots so that a republish while a region is being generated cannot tear what a
   * generator is halfway through reading. */
  struct Snapshot {
    std::shared_ptr<const GroundPatch> Patch;
    std::shared_ptr<const ClassStructure> Classes;
    std::shared_ptr<const FeatureField> Features;
    std::shared_ptr<const GroundTable> Table;
  };

  /* None when a part is missing; there is no Ground with a piece of itself absent. */
  static std::optional<Ground> Of(const Region &region, const Snapshot &snapshot);

  const Region &Where() const noexcept { return Region_; }
  double HeightAslM(double eastM, double northM) const noexcept {
    return Patch_->HeightAslM(eastM, northM);
  }
  double SlopeDeg(double eastM, double northM) const noexcept {
    return Patch_->SlopeDeg(eastM, northM);
  }
  void GradientAt(double eastM, double northM, double *dhde, double *dhdn) const noexcept {
    Patch_->GradientAt(eastM, northM, dhde, dhdn);
  }
  /* Evaluated from the outlines at this very point, never sampled off a lattice: the class is the
   * one quantity here that comes from vectors, and a raster fine enough not to notch a wood line
   * below a scatter's own step does not fit in the region budget. */
  Cover CoverAt(double eastM, double northM) const noexcept;
  [[nodiscard]] bool TryWaterLevelAslM(double eastM, double northM, double *out) const noexcept {
    return Patch_->TryWaterLevelAslM(eastM, northM, out);
  }
  const FeatureField &Features() const noexcept { return *Features_; }
  const GroundTable &Table() const noexcept { return *Table_; }
  /* The declared anchor every ECEF offset of this region is measured from, at sea level. */
  const double *AnchorEcef() const noexcept { return AnchorEcef_; }

private:
  Ground(const Region &region, const Snapshot &snapshot);

  Region Region_;
  std::shared_ptr<const GroundPatch> Patch_;
  std::shared_ptr<const ClassStructure> Classes_;
  std::shared_ptr<const FeatureField> Features_;
  std::shared_ptr<const GroundTable> Table_;
  double AnchorEcef_[3];
};

} // namespace outshine::Generators
#endif
