#include "StructureSurfaceRefinement.h"
#include "DistanceInterval.h"
#include "TriangleDistance.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace outshine::Generators {
namespace {
using DistanceArithmetic::Add;
using DistanceArithmetic::Subtract;

[[nodiscard]] size_t IndexCount(const Raised &mesh) noexcept {
  return mesh.WallRun.size() + mesh.RoofRun.size();
}

[[nodiscard]] std::array<PointEnclosure, 3> TriangleAt(const Raised &mesh, size_t cursor) noexcept {
  const bool wall = cursor < mesh.WallRun.size();
  const auto &indices = wall ? mesh.WallRun : mesh.RoofRun;
  const auto &vertices = wall ? mesh.WallCorners : mesh.RoofCorners;
  const size_t offset = wall ? cursor : cursor - mesh.WallRun.size();
  std::array<PointEnclosure, 3> result;
  for (size_t corner = 0; corner < result.size(); ++corner) {
    const auto &point = vertices[indices[offset + corner]].pos;
    result[corner].EstimateM = {{point[0], point[1], point[2]}};
  }
  return result;
}
}

double StructureSurfaceErrorInterval::LowerDistanceM() const noexcept {
  return std::max(ReferenceToVariantLowerM, VariantToReferenceLowerM);
}

bool StructureSurfaceRefinementTask::LowerPriority(const Region &a, const Region &b) noexcept {
  return a.UpperM < b.UpperM || (a.UpperM == b.UpperM && a.Serial > b.Serial);
}

size_t StructureSurfaceRefinementTask::RegionCount() const noexcept {
  return Heaps_[0].size() + Heaps_[1].size();
}

size_t StructureSurfaceRefinementTask::ScratchCapacityBytes() const noexcept {
  return (Heaps_[0].capacity() + Heaps_[1].capacity()) * sizeof(Region) + sizeof(Children_) +
         sizeof(Working_);
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::Reset(StructureSurfacePair inputs,
                                      StructureSurfaceRefinementLimits limits) noexcept {
  Phase_ = Phase::Failed;
  Bound_ = {};
  Failure_ = StructureSurfaceErrorFailure::InvalidBudget;
  if (limits.MaxRegions == 0 || limits.MaxRegions > 4096 || limits.MaxTriangleQueries > 131072 ||
      !std::isfinite(limits.TargetUncertaintyM) || limits.TargetUncertaintyM < 0) {
    return std::unexpected(Failure_);
  }
  Inputs_ = {&inputs.Reference, &inputs.Variant};
  Limits_ = limits;
  SourceKey_ = inputs.SourceKey;
  NextSerial_ = 0;
  Direction_ = 0;
  SeedCursor_ = 0;
  TargetCursor_ = 0;
  ChildCursor_ = 0;
  WorkUnits_ = 0;
  TriangleQueries_ = 0;
  Splits_ = 0;
  LowerM_ = {};
  for (auto &heap : Heaps_) { heap.clear(); }
  const auto reset = Envelope_.Reset(inputs);
  if (!reset) {
    Failure_ = reset.error();
    return std::unexpected(Failure_);
  }
  const auto coarse = Envelope_.Bound(SourceKey_);
  if (coarse) {
    Bound_ = *coarse;
    Phase_ = Phase::Complete;
  } else {
    Phase_ = Phase::Envelope;
  }
  return {};
}

std::expected<StructureSurfaceRefinementTask::Region, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::PrepareRegion(std::array<PointEnclosure, 3> vertices,
                                              double inheritedUpperM) noexcept {
  const auto enclosure = EncloseTriangleRegion(vertices);
  if (!enclosure) { return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance); }
  return Region{.Vertices = vertices,
                .Enclosure = *enclosure,
                .UpperM = inheritedUpperM,
                .Serial = NextSerial_++};
}

void StructureSurfaceRefinementTask::BeginEvaluation(Region region) noexcept {
  Working_ = region;
  TargetCursor_ = 0;
  SampleLowerM_ = std::numeric_limits<double>::infinity();
  SampleUpperM_ = std::numeric_limits<double>::infinity();
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::PrepareSplit() noexcept {
  if (RegionCount() + 3 > Limits_.MaxRegions) {
    Phase_ = Phase::Complete;
    return {};
  }
  Direction_ = Heaps_[0].front().UpperM >= Heaps_[1].front().UpperM ? 0 : 1;
  const Region &parent = Heaps_[Direction_].front();
  const auto &v = parent.Vertices;
  const auto ab = EnclosePointBlend({v[0], v[1]}, 0.5);
  const auto bc = EnclosePointBlend({v[1], v[2]}, 0.5);
  const auto ca = EnclosePointBlend({v[2], v[0]}, 0.5);
  if (!ab || !bc || !ca) {
    return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance);
  }
  const std::array<std::array<PointEnclosure, 3>, 4> corners{
      {{v[0], *ab, *ca}, {*ab, v[1], *bc}, {*ca, *bc, v[2]}, {*ab, *bc, *ca}}};
  for (size_t child = 0; child < Children_.size(); ++child) {
    const auto region = PrepareRegion(corners[child], parent.UpperM);
    if (!region) { return std::unexpected(region.error()); }
    Children_[child] = *region;
  }
  ChildCursor_ = 0;
  BeginEvaluation(Children_[0]);
  Phase_ = Phase::EvaluateChild;
  return {};
}

void StructureSurfaceRefinementTask::UpdateUpper() noexcept {
  Bound_.ReferenceToVariantM = Heaps_[0].front().UpperM;
  Bound_.VariantToReferenceM = Heaps_[1].front().UpperM;
  const double upper = Bound_.UpperDistanceM();
  const double lower = std::max(LowerM_[0], LowerM_[1]);
  const double width =
      Subtract({.Lower = upper, .Upper = upper}, {.Lower = lower, .Upper = lower}).Upper;
  Phase_ = width <= Limits_.TargetUncertaintyM ? Phase::Complete : Phase::Split;
}

void StructureSurfaceRefinementTask::FinishEvaluation() noexcept {
  const double deviation = Working_.Enclosure.InteriorSample.RadiusM;
  const double lower = Subtract({.Lower = SampleLowerM_, .Upper = SampleLowerM_},
                                {.Lower = deviation, .Upper = deviation})
                           .Lower;
  LowerM_[Direction_] = std::max({LowerM_[Direction_], 0.0, lower});
  auto &heap = Heaps_[Direction_];
  if (Phase_ == Phase::EvaluateSeed) {
    heap.push_back(Working_);
    std::ranges::push_heap(heap, LowerPriority);
    SeedCursor_ += 3;
    if (SeedCursor_ == IndexCount(*Inputs_[Direction_])) {
      ++Direction_;
      SeedCursor_ = 0;
    }
    if (Direction_ == Inputs_.size()) {
      UpdateUpper();
    } else {
      Phase_ = Phase::Seed;
    }
    return;
  }
  Children_[ChildCursor_++] = Working_;
  if (ChildCursor_ < Children_.size()) {
    BeginEvaluation(Children_[ChildCursor_]);
    return;
  }
  std::ranges::pop_heap(heap, LowerPriority);
  heap.pop_back();
  for (const Region &child : Children_) {
    heap.push_back(child);
    std::ranges::push_heap(heap, LowerPriority);
  }
  ++Splits_;
  UpdateUpper();
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::EvaluateTriangle() noexcept {
  if (TriangleQueries_ == Limits_.MaxTriangleQueries) {
    Phase_ = Phase::Complete;
    return {};
  }
  const auto triangle = TriangleAt(*Inputs_[1 - Direction_], TargetCursor_);
  const auto value = BoundPointTriangleDistance(Working_.Enclosure.InteriorSample.EstimateM,
                                                triangle[0].EstimateM,
                                                triangle[1].EstimateM,
                                                triangle[2].EstimateM);
  ++TriangleQueries_;
  if (!value) { return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance); }
  SampleLowerM_ = std::min(SampleLowerM_, value->LowerDistanceM);
  SampleUpperM_ = std::min(SampleUpperM_, value->UpperDistanceM);
  const double radius = Working_.Enclosure.RadiusM;
  const double upper =
      Add({.Lower = SampleUpperM_, .Upper = SampleUpperM_}, {.Lower = radius, .Upper = radius})
          .Upper;
  if (!std::isfinite(upper)) {
    return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance);
  }
  Working_.UpperM = std::min(Working_.UpperM, upper);
  TargetCursor_ += 3;
  if (TargetCursor_ == IndexCount(*Inputs_[1 - Direction_])) { FinishEvaluation(); }
  return {};
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::ProcessWork() noexcept {
  switch (Phase_) {
    case Phase::Envelope: {
      const auto progress = Envelope_.Step(SourceKey_, {.MaxCorners = 1});
      if (!progress) { return std::unexpected(progress.error()); }
      if (*progress != StructureSurfaceErrorProgress::Complete) { return {}; }
      const auto coarse = Envelope_.Bound(SourceKey_);
      assert(coarse.has_value());
      Bound_ = *coarse;
      const size_t triangles = (IndexCount(*Inputs_[0]) + IndexCount(*Inputs_[1])) / 3;
      if (triangles > Limits_.MaxRegions || Limits_.MaxTriangleQueries == 0) {
        Phase_ = Phase::Complete;
      } else {
        for (auto &heap : Heaps_) { heap.reserve(Limits_.MaxRegions); }
        Phase_ = Phase::Seed;
      }
      return {};
    }
    case Phase::Seed: {
      const double upper =
          Direction_ == 0 ? Bound_.ReferenceToVariantM : Bound_.VariantToReferenceM;
      const auto region = PrepareRegion(TriangleAt(*Inputs_[Direction_], SeedCursor_), upper);
      if (!region) { return std::unexpected(region.error()); }
      BeginEvaluation(*region);
      Phase_ = Phase::EvaluateSeed;
      return {};
    }
    case Phase::Split: return PrepareSplit();
    case Phase::EvaluateSeed:
    case Phase::EvaluateChild: return EvaluateTriangle();
    case Phase::Complete:
    case Phase::Failed: return {};
  }
  std::unreachable();
}

std::expected<StructureSurfaceErrorProgress, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::Step(uint64_t currentSourceKey,
                                     StructureSurfaceRefinementBudget budget) noexcept {
  if (Phase_ == Phase::Failed) { return std::unexpected(Failure_); }
  if (currentSourceKey != SourceKey_) {
    Phase_ = Phase::Failed;
    Failure_ = StructureSurfaceErrorFailure::SourceChanged;
    Bound_ = {};
    return std::unexpected(Failure_);
  }
  while (budget.MaxWorkUnits > 0 && Phase_ != Phase::Complete) {
    --budget.MaxWorkUnits;
    ++WorkUnits_;
    const auto processed = ProcessWork();
    if (!processed) {
      Phase_ = Phase::Failed;
      Failure_ = processed.error();
      Bound_ = {};
      return std::unexpected(Failure_);
    }
  }
  return Phase_ == Phase::Complete ? StructureSurfaceErrorProgress::Complete
                                   : StructureSurfaceErrorProgress::Pending;
}

void StructureSurfaceRefinementTask::Cancel() noexcept {
  Envelope_.Cancel();
  Phase_ = Phase::Failed;
  Failure_ = StructureSurfaceErrorFailure::Cancelled;
  Bound_ = {};
}

std::optional<StructureSurfaceErrorInterval>
StructureSurfaceRefinementTask::Bound(uint64_t currentSourceKey) const noexcept {
  if (Phase_ == Phase::Envelope || Phase_ == Phase::Failed || currentSourceKey != SourceKey_) {
    return std::nullopt;
  }
  return StructureSurfaceErrorInterval{.Upper = Bound_,
                                       .ReferenceToVariantLowerM = LowerM_[0],
                                       .VariantToReferenceLowerM = LowerM_[1]};
}

}
