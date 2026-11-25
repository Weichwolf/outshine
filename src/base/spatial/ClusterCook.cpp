#include "ClusterCook.h"
#include "Vec3.h"
#include "Octahedral.h"
#include <numbers>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace Says {
constexpr std::array kClusterErrors = {
    "cluster input has an incomplete vertex or triangle layout",
    "cluster input contains an out-of-range vertex index",
    "cluster input contains a non-finite position",
    "cluster triangle limit is outside the supported index range",
    "cluster input exceeds the 32-bit index range",
    "cluster bounds cannot be represented as a finite GPU sphere"};
}

std::string_view Describe(ClusterError error) noexcept {
  const auto index = static_cast<size_t>(error);
  return index < Says::kClusterErrors.size() ? Says::kClusterErrors[index] : std::string_view{};
}

bool ValidClusterRanges(std::span<const DagCluster> clusters, size_t indices) noexcept {
  const auto finite = [](float value) { return std::isfinite(value); };
  size_t first = 0;
  for (const auto &cluster : clusters) {
    if (!std::ranges::all_of(cluster.SelfCenter, finite) ||
        !std::ranges::all_of(cluster.ParentCenter, finite) || !finite(cluster.SelfRadius) ||
        !finite(cluster.ParentRadius) || !finite(cluster.SelfErr) || !finite(cluster.ParentErr) ||
        cluster.SelfRadius < 0 || cluster.ParentRadius < 0 || cluster.SelfErr < 0 ||
        cluster.ParentErr < 0 || cluster.First != first || first > indices || cluster.Count == 0 ||
        cluster.Count % 3 != 0 || cluster.Count > indices - first) {
      return false;
    }
    first += cluster.Count;
  }
  return first == indices;
}

namespace {
constexpr uint32_t kMortonBits = 0x3ffu;
constexpr uint32_t kMortonSpread16 = 0x030000ffu;
constexpr uint32_t kMortonSpread8 = 0x0300f00fu;
constexpr uint32_t kMortonSpread4 = 0x030c30c3u;
constexpr uint32_t kMortonSpread2 = 0x09249249u;
constexpr double kMortonSteps = 1023.0;
constexpr size_t kTriangleCorners = 3;
constexpr unsigned kMortonCodeBits = 30;
constexpr unsigned kRadixBits = 10;
constexpr unsigned kRadixPasses = kMortonCodeBits / kRadixBits;
constexpr uint64_t kRadixMask = 0x3ffu;

Vec3 Position(ClusterMeshInput input, size_t index) {
  const size_t first = index * input.StrideFloats;
  return {{input.PositionsM[first], input.PositionsM[first + 1], input.PositionsM[first + 2]}};
}

uint32_t Spread(uint32_t bits) {
  bits &= kMortonBits;
  bits = (bits | (bits << 16u)) & kMortonSpread16;
  bits = (bits | (bits << 8u)) & kMortonSpread8;
  bits = (bits | (bits << 4u)) & kMortonSpread4;
  return (bits | (bits << 2u)) & kMortonSpread2;
}

uint32_t
Morton(const Vec3 &point, const std::array<double, 3> &low, const std::array<double, 3> &high) {
  std::array<uint32_t, 3> quantized{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double width = high[axis] - low[axis];
    const double fraction = width > 0 ? (point[axis] - low[axis]) / width : 0;
    quantized[axis] = static_cast<uint32_t>(std::clamp(fraction, 0.0, 1.0) * kMortonSteps);
  }
  return (Spread(quantized[0]) << 2u) | (Spread(quantized[1]) << 1u) | Spread(quantized[2]);
}

std::expected<DagCluster, ClusterError> BoundCluster(const std::array<double, 3> &low,
                                                     const std::array<double, 3> &high,
                                                     size_t first,
                                                     size_t count) {
  DagCluster cluster{};
  double radiusSquared = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    cluster.SelfCenter[axis] = static_cast<float>((low[axis] + high[axis]) * 0.5);
    const double extent = std::max(std::abs(low[axis] - cluster.SelfCenter[axis]),
                                   std::abs(high[axis] - cluster.SelfCenter[axis]));
    radiusSquared += extent * extent;
  }
  const double radius = std::sqrt(radiusSquared);
  if (radius > std::numeric_limits<float>::max()) {
    return std::unexpected(ClusterError::BoundsOverflow);
  }
  cluster.SelfRadius = static_cast<float>(radius);
  if (cluster.SelfRadius > 0) {
    cluster.SelfRadius = std::nextafter(cluster.SelfRadius, std::numeric_limits<float>::infinity());
  }
  if (!std::isfinite(cluster.SelfRadius)) { return std::unexpected(ClusterError::BoundsOverflow); }
  cluster.ParentErr = kDagRootErr;
  cluster.First = static_cast<uint32_t>(first);
  cluster.Count = static_cast<uint32_t>(count);
  return cluster;
}
}

ClusterCookJob::ClusterCookJob(ClusterMeshInput input, uint32_t triangleLimit) noexcept
    : Input_(input), TriangleLimit_(triangleLimit) {
  Clear(InputBounds_);
  Clear(ClusterBounds_);
}

void ClusterCookJob::Clear(Bounds &bounds) noexcept {
  bounds.Low.fill(std::numeric_limits<double>::max());
  bounds.High.fill(std::numeric_limits<double>::lowest());
}

void ClusterCookJob::Include(Bounds &bounds, const Vec3 &point) noexcept {
  for (size_t axis = 0; axis < 3; ++axis) {
    bounds.Low[axis] = std::min(bounds.Low[axis], point[axis]);
    bounds.High[axis] = std::max(bounds.High[axis], point[axis]);
  }
}

bool ClusterCookJob::Ready() const noexcept {
  return Stage_ == Stage::Complete;
}

ClusteredMesh ClusterCookJob::Take() noexcept {
  assert(Ready());
  return std::move(Result_);
}

ClusterError ClusterCookJob::Fail(ClusterError error) noexcept {
  Failure_ = error;
  Stage_ = Stage::Failed;
  return error;
}

void ClusterCookJob::BeginsPack() {
  Result_.Index.clear();
  Result_.Clusters.clear();
  Result_.Index.reserve(Input_.Indices.size());
  const size_t triangles = Input_.Indices.size() / kTriangleCorners;
  Result_.Clusters.reserve(triangles / TriangleLimit_ +
                           (triangles % TriangleLimit_ != 0 ? 1u : 0u));
  Cursor_ = 0;
  ClusterFirst_ = 0;
  ClusterTriangles_ = 0;
  Clear(ClusterBounds_);
  Stage_ = Stage::Pack;
}

std::expected<ClusterCookJob::Flow, ClusterError> ClusterCookJob::AdvancesPack(size_t &itemsLeft) {
  const size_t triangles = Input_.Indices.size() / kTriangleCorners;
  while (Cursor_ < triangles && itemsLeft > 0) {
    if (ClusterTriangles_ == 0) {
      ClusterFirst_ = Result_.Index.size();
      Clear(ClusterBounds_);
    }
    const size_t triangle = Order_.empty() ? Cursor_ : static_cast<uint32_t>(Order_[Cursor_]);
    const size_t first = triangle * kTriangleCorners;
    for (size_t corner = 0; corner < kTriangleCorners; ++corner) {
      const uint32_t index = Input_.Indices[first + corner];
      Result_.Index.push_back(index);
      Include(ClusterBounds_, Position(Input_, index));
    }
    ++Cursor_;
    ++ClusterTriangles_;
    --itemsLeft;
    if (ClusterTriangles_ == TriangleLimit_ || Cursor_ == triangles) {
      const auto cluster = BoundCluster(ClusterBounds_.Low,
                                        ClusterBounds_.High,
                                        ClusterFirst_,
                                        ClusterTriangles_ * kTriangleCorners);
      if (!cluster) { return std::unexpected(Fail(cluster.error())); }
      Result_.Clusters.push_back(*cluster);
      ClusterTriangles_ = 0;
    }
  }
  if (Cursor_ == triangles) {
    Stage_ = Stage::Complete;
    return Flow::Complete;
  }
  return Flow::Yield;
}

std::expected<ClusterCookJob::Flow, ClusterError> ClusterCookJob::AdvancesLayout() {
  constexpr size_t maximum = std::numeric_limits<uint32_t>::max();
  if (TriangleLimit_ == 0 || TriangleLimit_ > maximum / kTriangleCorners) {
    return std::unexpected(Fail(ClusterError::InvalidLimit));
  }
  if (Input_.StrideFloats < 3 || Input_.PositionsM.size() % Input_.StrideFloats != 0 ||
      Input_.Indices.size() % kTriangleCorners != 0) {
    return std::unexpected(Fail(ClusterError::InvalidLayout));
  }
  if (Input_.PositionsM.size() / Input_.StrideFloats > maximum || Input_.Indices.size() > maximum) {
    return std::unexpected(Fail(ClusterError::CapacityExceeded));
  }
  Cursor_ = 0;
  Stage_ = Stage::Positions;
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError>
ClusterCookJob::AdvancesPositions(size_t &itemsLeft) {
  const size_t vertices = Input_.PositionsM.size() / Input_.StrideFloats;
  const size_t count = std::min(itemsLeft, vertices - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    const Vec3 point = Position(Input_, Cursor_);
    if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || !std::isfinite(point[2])) {
      return std::unexpected(Fail(ClusterError::NonFinitePosition));
    }
    Include(InputBounds_, point);
  }
  itemsLeft -= count;
  if (Cursor_ != vertices) { return Flow::Yield; }
  Cursor_ = 0;
  Stage_ = Stage::Indices;
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError>
ClusterCookJob::AdvancesIndices(size_t &itemsLeft) {
  const size_t count = std::min(itemsLeft, Input_.Indices.size() - Cursor_);
  const size_t end = Cursor_ + count;
  const size_t vertices = Input_.PositionsM.size() / Input_.StrideFloats;
  for (; Cursor_ < end; ++Cursor_) {
    if (Input_.Indices[Cursor_] >= vertices) {
      return std::unexpected(Fail(ClusterError::InvalidIndex));
    }
  }
  itemsLeft -= count;
  if (Cursor_ != Input_.Indices.size()) { return Flow::Yield; }
  if (Input_.Indices.empty()) {
    Stage_ = Stage::Complete;
    return Flow::Complete;
  }
  const size_t triangles = Input_.Indices.size() / kTriangleCorners;
  if (triangles <= TriangleLimit_) {
    BeginsPack();
  } else {
    Order_.resize(triangles);
    Cursor_ = 0;
    Stage_ = Stage::Order;
  }
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError> ClusterCookJob::AdvancesOrder(size_t &itemsLeft) {
  const size_t count = std::min(itemsLeft, Order_.size() - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    const size_t first = Cursor_ * kTriangleCorners;
    Vec3 center;
    for (size_t corner = 0; corner < kTriangleCorners; ++corner) {
      center = center + Position(Input_, Input_.Indices[first + corner]) * (1.0 / 3.0);
    }
    const uint64_t code = Morton(center, InputBounds_.Low, InputBounds_.High);
    Order_[Cursor_] = (code << 32u) | static_cast<uint32_t>(Cursor_);
  }
  itemsLeft -= count;
  if (Cursor_ != Order_.size()) { return Flow::Yield; }
  Sorting_.resize(Order_.size());
  Cursor_ = 0;
  Stage_ = Stage::SortClear;
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError>
ClusterCookJob::AdvancesSortClear(size_t &itemsLeft) {
  const size_t count = std::min(itemsLeft, Counts_.size() - Cursor_);
  std::fill_n(Counts_.begin() + static_cast<ptrdiff_t>(Cursor_), count, size_t{0});
  Cursor_ += count;
  itemsLeft -= count;
  if (Cursor_ != Counts_.size()) { return Flow::Yield; }
  Cursor_ = 0;
  Stage_ = Stage::SortCount;
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError>
ClusterCookJob::AdvancesSortCount(size_t &itemsLeft) {
  const size_t count = std::min(itemsLeft, Order_.size() - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    const auto bucket =
        static_cast<size_t>((Order_[Cursor_] >> (32u + Pass_ * kRadixBits)) & kRadixMask);
    ++Counts_[bucket];
  }
  itemsLeft -= count;
  if (Cursor_ != Order_.size()) { return Flow::Yield; }
  Cursor_ = 0;
  Prefix_ = 0;
  Running_ = 0;
  Stage_ = Stage::SortPrefix;
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError>
ClusterCookJob::AdvancesSortPrefix(size_t &itemsLeft) {
  const size_t count = std::min(itemsLeft, Counts_.size() - Prefix_);
  const size_t end = Prefix_ + count;
  for (; Prefix_ < end; ++Prefix_) {
    Offsets_[Prefix_] = Running_;
    Running_ += Counts_[Prefix_];
  }
  itemsLeft -= count;
  if (Prefix_ != Counts_.size()) { return Flow::Yield; }
  Cursor_ = 0;
  Stage_ = Stage::SortScatter;
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError>
ClusterCookJob::AdvancesSortScatter(size_t &itemsLeft) {
  const size_t count = std::min(itemsLeft, Order_.size() - Cursor_);
  const size_t end = Cursor_ + count;
  for (; Cursor_ < end; ++Cursor_) {
    const uint64_t key = Order_[Cursor_];
    const auto bucket = static_cast<size_t>((key >> (32u + Pass_ * kRadixBits)) & kRadixMask);
    Sorting_[Offsets_[bucket]++] = key;
  }
  itemsLeft -= count;
  if (Cursor_ != Order_.size()) { return Flow::Yield; }
  Order_.swap(Sorting_);
  ++Pass_;
  if (Pass_ == kRadixPasses) {
    BeginsPack();
  } else {
    Cursor_ = 0;
    Stage_ = Stage::SortClear;
  }
  return Flow::Continue;
}

std::expected<ClusterCookJob::Flow, ClusterError> ClusterCookJob::AdvanceStage(size_t &itemsLeft) {
  switch (Stage_) {
    case Stage::Layout: return AdvancesLayout();
    case Stage::Positions: return AdvancesPositions(itemsLeft);
    case Stage::Indices: return AdvancesIndices(itemsLeft);
    case Stage::Order: return AdvancesOrder(itemsLeft);
    case Stage::SortClear: return AdvancesSortClear(itemsLeft);
    case Stage::SortCount: return AdvancesSortCount(itemsLeft);
    case Stage::SortPrefix: return AdvancesSortPrefix(itemsLeft);
    case Stage::SortScatter: return AdvancesSortScatter(itemsLeft);
    case Stage::Pack: return AdvancesPack(itemsLeft);
    case Stage::Complete: return Flow::Complete;
    case Stage::Failed: return std::unexpected(Failure_);
  }
  return std::unexpected(Failure_);
}

std::expected<bool, ClusterError> ClusterCookJob::Advance(size_t itemsMost) {
  return AdvanceWithin(itemsMost);
}

std::expected<bool, ClusterError> ClusterCookJob::AdvanceWithin(size_t &itemsLeft) {
  if (Stage_ == Stage::Failed) { return std::unexpected(Failure_); }
  if (Ready()) { return true; }
  for (;;) {
    const auto advanced = AdvanceStage(itemsLeft);
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (*advanced == Flow::Complete) { return true; }
    if (*advanced == Flow::Yield) { return false; }
  }
}

std::expected<ClusteredMesh, ClusterError> CookClusters(ClusterMeshInput input,
                                                        uint32_t triangleLimit) {
  ClusterCookJob job(input, triangleLimit);
  for (;;) {
    const auto advanced = job.Advance(65536);
    if (!advanced) { return std::unexpected(advanced.error()); }
    if (*advanced) { return job.Take(); }
  }
}

namespace {
constexpr size_t kDirectionCount = 16;
constexpr uint32_t kDirectionBits = 12;
constexpr uint32_t kDirectionMaximum = (1u << kDirectionBits) - 1u;

Vec3 FaceNormal(ClusterMeshInput input, size_t first) {
  const Vec3 a = Position(input, input.Indices[first]);
  const Vec3 b = Position(input, input.Indices[first + 1]);
  const Vec3 c = Position(input, input.Indices[first + 2]);
  return Cross(b - a, c - a);
}

void SetNormalCone(ClusterMeshInput input, DagCluster &cluster) {
  Vec3 sum;
  const size_t end = size_t{cluster.First} + cluster.Count;
  for (size_t at = cluster.First; at < end; at += 3) {
    Vec3 normal = FaceNormal(input, at);
    if (Normalise(normal)) { sum = sum + normal; }
  }
  if (!Normalise(sum)) { return; }
  const auto fold = OctFolded(
      {{static_cast<float>(sum[0]), static_cast<float>(sum[1]), static_cast<float>(sum[2])}});
  const auto quantise = [](float axis) {
    return static_cast<uint32_t>(
        std::lround((std::clamp(axis, -1.0f, 1.0f) * 0.5f + 0.5f) * kDirectionMaximum));
  };
  const uint32_t u = quantise(fold[0]);
  const uint32_t v = quantise(fold[1]);
  const auto unquantise = [](uint32_t axis) {
    return static_cast<float>(axis) * (2.0f / kDirectionMaximum) - 1.0f;
  };
  const Vec3f axis{OctUnfolded({{unquantise(u), unquantise(v)}})};
  const Vec3 centre{{axis[0], axis[1], axis[2]}};
  double cosine = 1.0;
  for (size_t at = cluster.First; at < end; at += 3) {
    Vec3 normal = FaceNormal(input, at);
    if (Normalise(normal)) { cosine = std::min(cosine, Dot(normal, centre)); }
  }
  constexpr float guard = 8.0f * std::numeric_limits<float>::epsilon();
  cluster.NormalAxis = (u << kDirectionBits) | v;
  cluster.NormalCosine = std::max(0.0f, static_cast<float>(cosine) - guard);
}
}

std::expected<ClusteredMesh, ClusterError> CookDirectionalClusters(ClusterMeshInput input,
                                                                   uint32_t triangleLimit) {
  constexpr size_t maximum = std::numeric_limits<uint32_t>::max();
  if (triangleLimit == 0 || triangleLimit > maximum / 3u) {
    return std::unexpected(ClusterError::InvalidLimit);
  }
  if (input.Indices.size() > maximum) { return std::unexpected(ClusterError::CapacityExceeded); }
  if (input.StrideFloats < 3 || input.PositionsM.size() % input.StrideFloats != 0 ||
      input.Indices.size() % 3 != 0) {
    return std::unexpected(ClusterError::InvalidLayout);
  }
  const size_t vertices = input.PositionsM.size() / input.StrideFloats;
  if (!std::ranges::all_of(input.Indices, [vertices](auto index) { return index < vertices; })) {
    return std::unexpected(ClusterError::InvalidIndex);
  }
  std::array<std::vector<uint32_t>, kDirectionCount> bins;
  for (size_t at = 0; at < input.Indices.size(); at += 3) {
    const Vec3 normal = FaceNormal(input, at);
    const double direction = std::atan2(normal[2], normal[0]);
    if (!std::isfinite(direction)) { return std::unexpected(ClusterError::NonFinitePosition); }
    const double phase = (direction + std::numbers::pi) * (0.5 / std::numbers::pi);
    auto &indices = bins[static_cast<size_t>(phase * kDirectionCount) % kDirectionCount];
    const auto words = input.Indices.subspan(at, 3);
    indices.insert(indices.end(), words.begin(), words.end());
  }
  ClusteredMesh result;
  result.Index.reserve(input.Indices.size());
  for (const auto &bin : bins) {
    if (bin.empty()) { continue; }
    auto binInput = input;
    binInput.Indices = bin;
    const size_t step = size_t{triangleLimit} * 3u;
    for (size_t first = 0; first < bin.size(); first += step) {
      const size_t count = std::min(step, bin.size() - first);
      std::array<double, 3> low;
      std::array<double, 3> high;
      low.fill(std::numeric_limits<double>::max());
      high.fill(std::numeric_limits<double>::lowest());
      for (const uint32_t index : std::span(bin).subspan(first, count)) {
        const auto point = Position(input, index);
        for (size_t axis = 0; axis < 3; ++axis) {
          low[axis] = std::min(low[axis], point[axis]);
          high[axis] = std::max(high[axis], point[axis]);
        }
      }
      auto cluster = BoundCluster(low, high, first, count);
      if (!cluster) { return std::unexpected(cluster.error()); }
      SetNormalCone(binInput, *cluster);
      cluster->First += static_cast<uint32_t>(result.Index.size());
      result.Clusters.push_back(*cluster);
    }
    result.Index.insert(result.Index.end(), bin.begin(), bin.end());
  }
  return result;
}
}
