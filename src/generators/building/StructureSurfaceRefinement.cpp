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

[[nodiscard]] bool CornersCovered(const std::array<PointEnclosure, 3> &source,
                                  const std::array<PointEnclosure, 3> &target) noexcept {
  return std::ranges::all_of(source, [&target](const auto &point) {
    return point.RadiusM == 0 && std::ranges::any_of(target, [&point](const auto &corner) {
             return corner.RadiusM == 0 && point.EstimateM == corner.EstimateM;
           });
  });
}
}

double StructureSurfaceErrorInterval::LowerDistanceM() const noexcept {
  return std::max(ReferenceToVariantLowerM, VariantToReferenceLowerM);
}

StructureSurfaceRefinementTask::StructureSurfaceRefinementTask(
    StructureSurfaceRefinementTask &&other) noexcept {
  *this = std::move(other);
}

StructureSurfaceRefinementTask &
StructureSurfaceRefinementTask::operator=(StructureSurfaceRefinementTask &&other) noexcept {
  if (this == &other) { return *this; }
  Envelope_ = other.Envelope_;
  Inputs_ = other.Inputs_;
  Limits_ = other.Limits_;
  Heaps_ = std::move(other.Heaps_);
  Indices_ = std::move(other.Indices_);
  NodeUpperM_ = std::move(other.NodeUpperM_);
  SettledUpperM_ = other.SettledUpperM_;
  Targets_ = std::move(other.Targets_);
  Match_ = other.Match_;
  ChildCount_ = other.ChildCount_;
  Children_ = other.Children_;
  LowerM_ = other.LowerM_;
  Working_ = other.Working_;
  Bound_ = other.Bound_;
  SourceKey_ = other.SourceKey_;
  NextSerial_ = other.NextSerial_;
  Direction_ = other.Direction_;
  SeedCursor_ = other.SeedCursor_;
  TargetCursor_ = other.TargetCursor_;
  EvaluationPoint_ = other.EvaluationPoint_;
  ChildCursor_ = other.ChildCursor_;
  WorkUnits_ = other.WorkUnits_;
  TriangleQueries_ = other.TriangleQueries_;
  Splits_ = other.Splits_;
  SampleLowerM_ = other.SampleLowerM_;
  SampleUpperM_ = other.SampleUpperM_;
  BestTarget_ = other.BestTarget_;
  TargetCornerUpperM_ = other.TargetCornerUpperM_;
  Phase_ = other.Phase_;
  Failure_ = other.Failure_;
  other.Cancel();
  return *this;
}

bool StructureSurfaceRefinementTask::LaterTarget(const TargetNode &a,
                                                 const TargetNode &b) noexcept {
  return a.LowerM > b.LowerM || (a.LowerM == b.LowerM && a.Node > b.Node);
}

bool StructureSurfaceRefinementTask::LowerPriority(const Region &a, const Region &b) noexcept {
  return a.UpperM < b.UpperM || (a.UpperM == b.UpperM && a.Serial > b.Serial);
}

size_t StructureSurfaceRefinementTask::RegionCount() const noexcept {
  return Heaps_[0].size() + Heaps_[1].size();
}

size_t StructureSurfaceRefinementTask::ScratchCapacityBytes() const noexcept {
  return (Heaps_[0].capacity() + Heaps_[1].capacity()) * sizeof(Region) + sizeof(Children_) +
         sizeof(Working_) + Targets_.capacity() * sizeof(TargetNode) + Indices_[0].CapacityBytes() +
         Indices_[1].CapacityBytes() +
         (NodeUpperM_[0].capacity() + NodeUpperM_[1].capacity()) * sizeof(double);
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
  SettledUpperM_ = {};
  Match_.reset();
  Targets_.clear();
  ChildCount_ = 4;
  for (auto &heap : Heaps_) { heap.clear(); }
  for (auto &bounds : NodeUpperM_) { bounds.clear(); }
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
  Match_.reset();
  if (region.Node == std::numeric_limits<size_t>::max() &&
      std::ranges::all_of(region.Vertices, [](const auto &point) { return point.RadiusM == 0; })) {
    Match_ = Indices_[1 - Direction_].MatchOf(region.Vertices);
  }
  TargetCursor_ = std::numeric_limits<size_t>::max();
  Targets_.clear();
  Targets_.push_back({.Node = 0,
                      .LowerM = Indices_[1 - Direction_].LowerDistance(
                          0, region.Enclosure.InteriorSample.EstimateM)});
  EvaluationPoint_ = 0;
  TargetCornerUpperM_ = 0;
  SampleLowerM_ = std::numeric_limits<double>::infinity();
  SampleUpperM_ = std::numeric_limits<double>::infinity();
  BestTarget_ = std::numeric_limits<size_t>::max();
}

bool StructureSurfaceRefinementTask::Hierarchical() const noexcept {
  return (IndexCount(*Inputs_[0]) + IndexCount(*Inputs_[1])) / 3 > Limits_.MaxRegions;
}

std::expected<StructureSurfaceRefinementTask::Region, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::PrepareNode(size_t node, double inheritedUpperM) noexcept {
  const auto &index = Indices_[Direction_];
  inheritedUpperM = std::min(inheritedUpperM, NodeUpperM_[Direction_][node]);
  if (index.At(node).Count == 1) {
    return PrepareRegion(index.Triangle(index.At(node).First), inheritedUpperM);
  }
  return Region{.UpperM = inheritedUpperM, .Serial = NextSerial_++, .Node = node};
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::PrepareSplit() noexcept {
  const auto upper = [this](size_t direction) {
    return Heaps_[direction].empty() ? -1.0 : Heaps_[direction].front().UpperM;
  };
  Direction_ = upper(0) >= upper(1) ? 0 : 1;
  const Region &parent = Heaps_[Direction_].front();
  const bool node = parent.Node != std::numeric_limits<size_t>::max();
  ChildCount_ = node ? 2 : 4;
  if (RegionCount() + ChildCount_ - 1 > Limits_.MaxRegions) {
    Phase_ = Phase::Complete;
    return {};
  }
  if (node) {
    const auto &branch = Indices_[Direction_].At(parent.Node);
    const std::array children{branch.Left, branch.Right};
    for (size_t child = 0; child < ChildCount_; ++child) {
      const auto region = PrepareNode(children[child], parent.UpperM);
      if (!region) { return std::unexpected(region.error()); }
      Children_[child] = *region;
    }
  } else {
    const auto &v = parent.Vertices;
    const auto ab = EnclosePointBlend({v[0], v[1]}, 0.5);
    const auto bc = EnclosePointBlend({v[1], v[2]}, 0.5);
    const auto ca = EnclosePointBlend({v[2], v[0]}, 0.5);
    if (!ab || !bc || !ca) {
      return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance);
    }
    const std::array<std::array<PointEnclosure, 3>, 4> corners{
        {{v[0], *ab, *ca}, {*ab, v[1], *bc}, {*ca, *bc, v[2]}, {*ab, *bc, *ca}}};
    for (size_t child = 0; child < ChildCount_; ++child) {
      const auto region = PrepareRegion(corners[child], parent.UpperM);
      if (!region) { return std::unexpected(region.error()); }
      Children_[child] = *region;
    }
  }
  ChildCursor_ = 0;
  BeginEvaluation(Children_[0]);
  Phase_ = Phase::EvaluateChild;
  return {};
}

void StructureSurfaceRefinementTask::UpdateUpper() noexcept {
  const auto upperOf = [this](size_t direction) {
    if (Heaps_[direction].empty()) { return SettledUpperM_[direction]; }
    return std::max(SettledUpperM_[direction], Heaps_[direction].front().UpperM);
  };
  Bound_.ReferenceToVariantM = upperOf(0);
  Bound_.VariantToReferenceM = upperOf(1);
  const double upper = Bound_.UpperDistanceM();
  const double lower = std::max(LowerM_[0], LowerM_[1]);
  const double width =
      Subtract({.Lower = upper, .Upper = upper}, {.Lower = lower, .Upper = lower}).Upper;
  Phase_ = (Heaps_[0].empty() && Heaps_[1].empty()) || width <= Limits_.TargetUncertaintyM
               ? Phase::Complete
               : Phase::Split;
}

void StructureSurfaceRefinementTask::KeepRegion(const Region &region) {
  const double lower = std::max(LowerM_[0], LowerM_[1]);
  const double width =
      Subtract({.Lower = region.UpperM, .Upper = region.UpperM}, {.Lower = lower, .Upper = lower})
          .Upper;
  if (width <= Limits_.TargetUncertaintyM || region.UpperM == 0) {
    SettledUpperM_[Direction_] = std::max(SettledUpperM_[Direction_], region.UpperM);
  } else {
    auto &heap = Heaps_[Direction_];
    heap.push_back(region);
    std::ranges::push_heap(heap, LowerPriority);
  }
}

void StructureSurfaceRefinementTask::AdvanceSeed() noexcept {
  const bool indexed = Hierarchical();
  SeedCursor_ += indexed ? 1 : 3;
  if (SeedCursor_ ==
      (indexed ? Indices_[Direction_].NodeCount() : IndexCount(*Inputs_[Direction_]))) {
    ++Direction_;
    SeedCursor_ = 0;
  }
  if (Direction_ != Inputs_.size()) {
    Phase_ = Phase::Seed;
    return;
  }
  if (!indexed) {
    UpdateUpper();
    return;
  }
  Direction_ = 0;
  SeedCursor_ = Indices_[0].NodeCount();
  Phase_ = Phase::Fold;
}

void StructureSurfaceRefinementTask::FinishEvaluation() noexcept {
  const double deviation = Working_.Enclosure.InteriorSample.RadiusM;
  const double lower = Subtract({.Lower = SampleLowerM_, .Upper = SampleLowerM_},
                                {.Lower = deviation, .Upper = deviation})
                           .Lower;
  LowerM_[Direction_] = std::max({LowerM_[Direction_], 0.0, lower});
  auto &heap = Heaps_[Direction_];
  if (Phase_ == Phase::EvaluateSeed) {
    if (Hierarchical()) {
      NodeUpperM_[Direction_].back() = Working_.UpperM;
    } else {
      KeepRegion(Working_);
    }
    AdvanceSeed();
    return;
  }
  Children_[ChildCursor_++] = Working_;
  if (ChildCursor_ < ChildCount_) {
    BeginEvaluation(Children_[ChildCursor_]);
    return;
  }
  std::ranges::pop_heap(heap, LowerPriority);
  heap.pop_back();
  for (size_t child = 0; child < ChildCount_; ++child) { KeepRegion(Children_[child]); }
  ++Splits_;
  UpdateUpper();
}

void StructureSurfaceRefinementTask::BeginCornerQueries() noexcept {
  assert(BestTarget_ != std::numeric_limits<size_t>::max());
  TargetCursor_ = BestTarget_;
  EvaluationPoint_ = 1;
  TargetCornerUpperM_ = 0;
}

void StructureSurfaceRefinementTask::AdvanceTargetSearch() noexcept {
  if (Targets_.empty()) {
    BeginCornerQueries();
    return;
  }
  const auto next = Targets_.front();
  std::ranges::pop_heap(Targets_, LaterTarget);
  Targets_.pop_back();
  if (next.LowerM >= SampleLowerM_ && next.LowerM >= SampleUpperM_) {
    Targets_.clear();
    BeginCornerQueries();
    return;
  }
  const auto &index = Indices_[1 - Direction_];
  const auto &branch = index.At(next.Node);
  if (branch.Count == 1) {
    TargetCursor_ = static_cast<size_t>(branch.First) * 3;
    return;
  }
  const Vec3 sample = Working_.Enclosure.InteriorSample.EstimateM;
  Targets_.push_back({.Node = branch.Left, .LowerM = index.LowerDistance(branch.Left, sample)});
  std::ranges::push_heap(Targets_, LaterTarget);
  Targets_.push_back({.Node = branch.Right, .LowerM = index.LowerDistance(branch.Right, sample)});
  std::ranges::push_heap(Targets_, LaterTarget);
}

void StructureSurfaceRefinementTask::AcceptSampleBound(double lowerM,
                                                       double upperM,
                                                       double regionUpperM) noexcept {
  SampleLowerM_ = std::min(SampleLowerM_, lowerM);
  if (upperM < SampleUpperM_) {
    SampleUpperM_ = upperM;
    BestTarget_ = TargetCursor_;
  }
  Working_.UpperM = std::min(Working_.UpperM, regionUpperM);
  const double lower = std::max(LowerM_[0], LowerM_[1]);
  const double width = Subtract({.Lower = Working_.UpperM, .Upper = Working_.UpperM},
                                {.Lower = lower, .Upper = lower})
                           .Upper;
  if (width <= Limits_.TargetUncertaintyM) {
    SampleLowerM_ = 0;
    FinishEvaluation();
    return;
  }
  TargetCursor_ = std::numeric_limits<size_t>::max();
  if (Targets_.empty()) { BeginCornerQueries(); }
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::EvaluatePoint() noexcept {
  if (Working_.Node != std::numeric_limits<size_t>::max()) {
    SampleLowerM_ = 0;
    FinishEvaluation();
    return {};
  }
  const auto &index = Indices_[1 - Direction_];
  if (Match_) {
    const auto candidate = index.NextMatch(*Match_);
    if (candidate && CornersCovered(Working_.Vertices, index.Triangle(*candidate))) {
      Working_.UpperM = 0;
      SampleLowerM_ = 0;
      FinishEvaluation();
      return {};
    }
    if (Match_->Remaining == 0) { Match_.reset(); }
    return {};
  }
  if (TargetCursor_ == std::numeric_limits<size_t>::max()) {
    AdvanceTargetSearch();
    return {};
  }
  const auto triangle = TriangleAt(*Inputs_[1 - Direction_], TargetCursor_);
  if (EvaluationPoint_ == 0 && CornersCovered(Working_.Vertices, triangle)) {
    Working_.UpperM = 0;
    SampleLowerM_ = 0;
    FinishEvaluation();
    return {};
  }
  if (TriangleQueries_ == Limits_.MaxTriangleQueries) {
    Phase_ = Phase::Complete;
    return {};
  }
  const bool sample = EvaluationPoint_ == 0;
  const auto &point =
      sample ? Working_.Enclosure.InteriorSample : Working_.Vertices[EvaluationPoint_ - 1];
  const auto value = BoundPointTriangleDistance(
      point.EstimateM, triangle[0].EstimateM, triangle[1].EstimateM, triangle[2].EstimateM);
  ++TriangleQueries_;
  if (!value) { return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance); }
  const double radius = sample ? Working_.Enclosure.RadiusM : point.RadiusM;
  const double upper = Add({.Lower = value->UpperDistanceM, .Upper = value->UpperDistanceM},
                           {.Lower = radius, .Upper = radius})
                           .Upper;
  if (!std::isfinite(upper)) {
    return std::unexpected(StructureSurfaceErrorFailure::NonfiniteDistance);
  }
  if (sample) {
    AcceptSampleBound(value->LowerDistanceM, value->UpperDistanceM, upper);
    return {};
  }
  TargetCornerUpperM_ = std::max(TargetCornerUpperM_, upper);
  if (++EvaluationPoint_ <= Working_.Vertices.size() && TargetCornerUpperM_ < Working_.UpperM) {
    return {};
  }
  Working_.UpperM = std::min(Working_.UpperM, TargetCornerUpperM_);
  FinishEvaluation();
  return {};
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::PrepareSeed() noexcept {
  const double upper = Direction_ == 0 ? Bound_.ReferenceToVariantM : Bound_.VariantToReferenceM;
  size_t cursor = SeedCursor_;
  if (Hierarchical()) {
    const auto &node = Indices_[Direction_].At(SeedCursor_);
    NodeUpperM_[Direction_].push_back(upper);
    if (node.Count > 1) {
      AdvanceSeed();
      return {};
    }
    cursor = static_cast<size_t>(node.First) * 3;
  }
  const auto region = PrepareRegion(TriangleAt(*Inputs_[Direction_], cursor), upper);
  if (!region) { return std::unexpected(region.error()); }
  BeginEvaluation(*region);
  Phase_ = Phase::EvaluateSeed;
  if (cursor < IndexCount(*Inputs_[1 - Direction_]) &&
      CornersCovered(Working_.Vertices, TriangleAt(*Inputs_[1 - Direction_], cursor))) {
    Working_.UpperM = 0;
    SampleLowerM_ = 0;
    FinishEvaluation();
  }
  return {};
}

std::expected<void, StructureSurfaceErrorFailure>
StructureSurfaceRefinementTask::FoldBounds() noexcept {
  const size_t cursor = --SeedCursor_;
  const auto &node = Indices_[Direction_].At(cursor);
  auto &bounds = NodeUpperM_[Direction_];
  if (node.Count > 1) { bounds[cursor] = std::max(bounds[node.Left], bounds[node.Right]); }
  if (SeedCursor_ > 0) { return {}; }
  if (++Direction_ < Inputs_.size()) {
    SeedCursor_ = Indices_[Direction_].NodeCount();
    return {};
  }
  for (Direction_ = 0; Direction_ < Inputs_.size(); ++Direction_) {
    const auto root = PrepareNode(0, NodeUpperM_[Direction_][0]);
    if (!root) { return std::unexpected(root.error()); }
    KeepRegion(*root);
  }
  UpdateUpper();
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
      if (Limits_.MaxRegions < 2 || Limits_.MaxTriangleQueries == 0) {
        Phase_ = Phase::Complete;
      } else {
        for (auto &heap : Heaps_) { heap.reserve(Limits_.MaxRegions); }
        Targets_.reserve(std::max(IndexCount(*Inputs_[0]), IndexCount(*Inputs_[1])) / 3);
        Indices_[0].Reset(*Inputs_[0]);
        Indices_[1].Reset(*Inputs_[1]);
        Phase_ = Phase::Index;
      }
      return {};
    }
    case Phase::Index: {
      if (!Indices_[Direction_].Step()) { return {}; }
      ++Direction_;
      if (Direction_ == Inputs_.size()) {
        Direction_ = 0;
        if (Hierarchical()) {
          NodeUpperM_[0].reserve(Indices_[0].NodeCount());
          NodeUpperM_[1].reserve(Indices_[1].NodeCount());
        }
        Phase_ = Phase::Seed;
      }
      return {};
    }
    case Phase::Seed: return PrepareSeed();
    case Phase::Fold: return FoldBounds();
    case Phase::Split: return PrepareSplit();
    case Phase::EvaluateSeed:
    case Phase::EvaluateChild: return EvaluatePoint();
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
