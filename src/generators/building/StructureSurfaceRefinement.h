#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURESURFACEREFINEMENT_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURESURFACEREFINEMENT_H

#include "StructureSurfaceError.h"
#include "StructureSurfaceIndex.h"
#include <limits>
#include "TriangleRegion.h"
#include "TriangleDistance.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace outshine::Generators {

struct StructureSurfaceRefinementLimits {
  size_t MaxTriangleQueries = 131072;
  size_t MaxRegions = 4096;
  static constexpr double DefaultTargetUncertaintyM = 0.02;
  double TargetUncertaintyM = DefaultTargetUncertaintyM;
};

struct StructureSurfaceRefinementBudget {
  size_t MaxWorkUnits = 128;
};

struct StructureSurfaceErrorInterval {
  StructureSurfaceErrorBound Upper;
  double ReferenceToVariantLowerM = 0;
  double VariantToReferenceLowerM = 0;
  [[nodiscard]] double LowerDistanceM() const noexcept;

  [[nodiscard]] double UpperDistanceM() const noexcept { return Upper.UpperDistanceM(); }
};

class StructureSurfaceRefinementTask {
public:
  StructureSurfaceRefinementTask() = default;
  ~StructureSurfaceRefinementTask() = default;
  StructureSurfaceRefinementTask(const StructureSurfaceRefinementTask &) = default;
  StructureSurfaceRefinementTask &operator=(const StructureSurfaceRefinementTask &) = default;
  StructureSurfaceRefinementTask(StructureSurfaceRefinementTask &&other) noexcept;
  StructureSurfaceRefinementTask &operator=(StructureSurfaceRefinementTask &&other) noexcept;

  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure>
  Reset(StructureSurfacePair inputs, StructureSurfaceRefinementLimits limits = {}) noexcept;
  [[nodiscard]] std::expected<StructureSurfaceErrorProgress, StructureSurfaceErrorFailure>
  Step(uint64_t currentSourceKey, StructureSurfaceRefinementBudget budget = {}) noexcept;
  void Cancel() noexcept;
  [[nodiscard]] std::optional<StructureSurfaceErrorInterval>
  Bound(uint64_t currentSourceKey) const noexcept;

  [[nodiscard]] size_t WorkUnits() const noexcept { return WorkUnits_; }

  [[nodiscard]] size_t TriangleQueries() const noexcept { return TriangleQueries_; }

  [[nodiscard]] size_t Splits() const noexcept { return Splits_; }

  [[nodiscard]] size_t RegionCount() const noexcept;
  [[nodiscard]] size_t ScratchCapacityBytes() const noexcept;

private:
  struct Region {
    std::array<PointEnclosure, 3> Vertices{};
    TriangleRegionEnclosure Enclosure{};
    double UpperM = 0;
    uint64_t Serial = 0;
    size_t Node = std::numeric_limits<size_t>::max();
  };

  struct TargetNode {
    uint32_t Node = 0;
    double LowerM = 0;
  };

  [[nodiscard]] static bool LaterTarget(const TargetNode &a, const TargetNode &b) noexcept;
  enum class Phase {
    Envelope,
    Index,
    Seed,
    Fold,
    Split,
    EvaluateSeed,
    EvaluateChild,
    Complete,
    Failed
  };
  [[nodiscard]] static bool LowerPriority(const Region &a, const Region &b) noexcept;
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> ProcessWork() noexcept;
  [[nodiscard]] std::expected<Region, StructureSurfaceErrorFailure>
  PrepareRegion(std::array<PointEnclosure, 3> vertices, double inheritedUpperM) noexcept;
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> PrepareSplit() noexcept;
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> EvaluatePoint() noexcept;
  [[nodiscard]] std::expected<Region, StructureSurfaceErrorFailure>
  PrepareNode(size_t node, double inheritedUpperM) noexcept;
  [[nodiscard]] bool Hierarchical() const noexcept;
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> PrepareSeed() noexcept;
  [[nodiscard]] std::expected<void, StructureSurfaceErrorFailure> FoldBounds() noexcept;
  void BeginCornerQueries() noexcept;
  void AdvanceTargetSearch() noexcept;
  void AcceptSampleBound(const TriangleDistanceBound &sample, double regionUpperM) noexcept;
  void BeginEvaluation(Region region) noexcept;
  void FinishEvaluation() noexcept;
  void KeepRegion(const Region &region);
  void AdvanceSeed() noexcept;
  void UpdateUpper() noexcept;
  StructureSurfaceErrorTask Envelope_;
  std::array<const Raised *, 2> Inputs_{};
  StructureSurfaceRefinementLimits Limits_;
  std::array<std::vector<Region>, 2> Heaps_;
  std::array<StructureSurfaceIndex, 2> Indices_;
  std::array<std::vector<double>, 2> NodeUpperM_;
  std::array<double, 2> SettledUpperM_{};
  std::vector<TargetNode> Targets_;
  std::optional<StructureSurfaceIndex::Match> Match_;
  size_t ChildCount_ = 4;
  std::array<Region, 4> Children_;
  std::array<double, 2> LowerM_{};
  Region Working_;
  StructureSurfaceErrorBound Bound_;
  uint64_t SourceKey_ = 0;
  uint64_t NextSerial_ = 0;
  size_t Direction_ = 0;
  size_t SeedCursor_ = 0;
  size_t TargetCursor_ = 0;
  size_t EvaluationPoint_ = 0;
  size_t ChildCursor_ = 0;
  size_t WorkUnits_ = 0;
  size_t TriangleQueries_ = 0;
  size_t Splits_ = 0;
  double SampleLowerM_ = 0;
  double SampleUpperM_ = 0;
  size_t BestTarget_ = 0;
  double TargetCornerUpperM_ = 0;
  Phase Phase_ = Phase::Failed;
  StructureSurfaceErrorFailure Failure_ = StructureSurfaceErrorFailure::InvalidSource;
};

}
#endif
