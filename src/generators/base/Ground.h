#ifndef OUTSHINE_GENERATORS_BASE_GROUND_H
#define OUTSHINE_GENERATORS_BASE_GROUND_H

#include <memory>
#include <optional>

#include "math/Vec3.h"
#include "ClassStructure.h"
#include "Cover.h"
#include "FeatureField.h"
#include "GroundPatch.h"
#include "GroundTable.h"
#include "Tile.h"

namespace outshine::Generators {

class Ground {
public:
  struct Snapshot {
    std::shared_ptr<const GroundPatch> Patch;
    std::shared_ptr<const ClassStructure> Classes;
    std::shared_ptr<const FeatureField> Features;
    std::shared_ptr<const GroundTable> Table;
  };

  static std::optional<Ground> Of(const Tile &region, const Snapshot &snapshot);

  [[nodiscard]] const Tile &Where() const noexcept { return Region_; }

  [[nodiscard]] double HeightAslM(EastNorth at) const noexcept { return Patch_->HeightAslM(at); }

  [[nodiscard]] double SlopeDeg(EastNorth at) const noexcept { return Patch_->SlopeDeg(at); }

  [[nodiscard]] Gradient GradientAt(EastNorth at) const noexcept { return Patch_->GradientAt(at); }

  [[nodiscard]] Cover CoverAt(EastNorth at) const noexcept;

  [[nodiscard]] const FeatureField &Features() const noexcept { return *Features_; }

  [[nodiscard]] const GroundTable &Table() const noexcept { return *Table_; }

  [[nodiscard]] const Vec3 &AnchorEcef() const noexcept { return AnchorEcef_; }

  [[nodiscard]] size_t PatchHeapBytes() const noexcept { return Patch_->HeapBytes(); }

  [[nodiscard]] size_t FeatureHeapBytes() const noexcept { return Features_->HeapBytes(); }

private:
  Ground(const Tile &region, const Snapshot &snapshot);

  Tile Region_;
  std::shared_ptr<const GroundPatch> Patch_;
  std::shared_ptr<const ClassStructure> Classes_;
  std::shared_ptr<const FeatureField> Features_;
  std::shared_ptr<const GroundTable> Table_;
  Vec3 AnchorEcef_;
};

} // namespace outshine::Generators
#endif
