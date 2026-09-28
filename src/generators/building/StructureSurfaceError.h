#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURESURFACEERROR_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURESURFACEERROR_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include "StructureMesher.h"

namespace outshine::Generators {

struct StructureSurfacePair {
  const Raised &Reference;
  const Raised &Variant;
  uint64_t SourceKey;
};

struct StructureSurfaceErrorBound {
  uint64_t SourceKey = 0;
  double ReferenceToVariantM = 0;
  double VariantToReferenceM = 0;
  [[nodiscard]] double UpperDistanceM() const noexcept;
};

enum class StructureSurfaceErrorFailure {
  InvalidSource,
  InvalidBudget,
  InvalidGeometry,
  NonfiniteDistance,
  EmptyMismatch,
  SourceChanged,
  Cancelled
};

enum class StructureSurfaceErrorProgress { Pending, Complete };

struct StructureSurfaceWorkBudget {
  size_t MaxCorners = 128;
};

class StructureSurfaceErrorTask {
public:
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure>
  Reset(StructureSurfacePair inputs) noexcept;
  [[nodiscard]] std::expected<StructureSurfaceErrorProgress, StructureSurfaceErrorFailure>
  Step(uint64_t currentSourceKey, StructureSurfaceWorkBudget budget = {}) noexcept;
  void Cancel() noexcept;
  [[nodiscard]] std::optional<StructureSurfaceErrorBound>
  Bound(uint64_t currentSourceKey) const noexcept;

  [[nodiscard]] size_t WorkUnits() const noexcept { return WorkUnits_; }

  [[nodiscard]] size_t VertexDistanceQueries() const noexcept { return VertexDistanceQueries_; }

private:
  enum class Phase {
    ValidateReference,
    ValidateVariant,
    BoundReference,
    BoundVariant,
    Complete,
    Failed
  };
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> ProcessCorner() noexcept;
  void AdvancePhase() noexcept;
  const Raised *Reference_ = nullptr;
  const Raised *Variant_ = nullptr;
  uint64_t SourceKey_ = 0;
  size_t Cursor_ = 0;
  size_t WorkUnits_ = 0;
  size_t VertexDistanceQueries_ = 0;
  Phase Phase_ = Phase::Failed;
  StructureSurfaceErrorFailure Failure_ = StructureSurfaceErrorFailure::InvalidSource;
  StructureSurfaceErrorBound Bound_;
};

}
#endif
