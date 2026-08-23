#ifndef OUTSHINE_GENERATORS_GROUND_H
#define OUTSHINE_GENERATORS_GROUND_H

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

  struct Snapshot {
    std::shared_ptr<const GroundPatch> Patch;
    std::shared_ptr<const ClassStructure> Classes;
    std::shared_ptr<const FeatureField> Features;
    std::shared_ptr<const GroundTable> Table;
  };

  static std::optional<Ground> Of(const Region &region, const Snapshot &snapshot);

  [[nodiscard]] const Region &Where() const noexcept { return Region_; }
  [[nodiscard]] double HeightAslM(double eastM, double northM) const noexcept {
    return Patch_->HeightAslM(eastM, northM);
  }
  [[nodiscard]] double SlopeDeg(double eastM, double northM) const noexcept {
    return Patch_->SlopeDeg(eastM, northM);
  }
  void GradientAt(double eastM, double northM, double *dhde, double *dhdn) const noexcept {
    Patch_->GradientAt(eastM, northM, dhde, dhdn);
  }

  [[nodiscard]] Cover CoverAt(double eastM, double northM) const noexcept;
  [[nodiscard]] const FeatureField &Features() const noexcept { return *Features_; }
  [[nodiscard]] const GroundTable &Table() const noexcept { return *Table_; }

  [[nodiscard]] const double *AnchorEcef() const noexcept { return AnchorEcef_; }

  [[nodiscard]] size_t PatchHeapBytes() const noexcept { return Patch_->HeapBytes(); }
  [[nodiscard]] size_t FeatureHeapBytes() const noexcept { return Features_->HeapBytes(); }

private:
  Ground(const Region &region, const Snapshot &snapshot);

  Region Region_;
  std::shared_ptr<const GroundPatch> Patch_;
  std::shared_ptr<const ClassStructure> Classes_;
  std::shared_ptr<const FeatureField> Features_;
  std::shared_ptr<const GroundTable> Table_;
  double AnchorEcef_[3];
};

}
#endif
