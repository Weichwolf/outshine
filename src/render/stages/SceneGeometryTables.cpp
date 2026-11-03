#include "GeometryRenderer.h"
#include "BatchVisibility.h"
#include "native/Render.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr size_t kSphereFloats = 12;
constexpr size_t kArgWords = 5;

bool Accepted(Native::Result<void> result, std::string &error) {
  if (result) { return true; }
  error = std::move(result.error().Message);
  return false;
}

std::optional<BatchSphere> TemplateSphere(std::span<const GeometryClusterTemplate> templates,
                                          std::span<const float> spheres) {
  if (templates.empty()) { return std::nullopt; }
  std::array<double, 3> lo;
  std::array<double, 3> hi;
  lo.fill(std::numeric_limits<double>::infinity());
  hi.fill(-std::numeric_limits<double>::infinity());
  for (const auto &one : templates) {
    const size_t first = size_t{one.SphereIndex} * kSphereFloats;
    if (first > spheres.size() || spheres.size() - first < kSphereFloats) { return std::nullopt; }
    const auto sphere = spheres.subspan(first, 4);
    if (sphere[3] < 0 || !std::ranges::all_of(sphere, NormalOrZero)) { return std::nullopt; }
    for (size_t axis = 0; axis < lo.size(); ++axis) {
      lo[axis] = std::min(lo[axis], double{sphere[axis]} - sphere[3]);
      hi[axis] = std::max(hi[axis], double{sphere[axis]} + sphere[3]);
    }
  }
  std::array<double, 3> centre;
  for (size_t axis = 0; axis < centre.size(); ++axis) { centre[axis] = (lo[axis] + hi[axis]) / 2; }
  double radius = 0;
  for (const auto &one : templates) {
    const size_t first = size_t{one.SphereIndex} * kSphereFloats;
    radius = std::max(radius,
                      std::hypot(spheres[first] - centre[0],
                                 spheres[first + 1] - centre[1],
                                 spheres[first + 2] - centre[2]) +
                          spheres[first + 3]);
  }
  return PackSphere(centre, radius);
}
}

bool GeometryRenderer::AppendRoot(GeometryRoot root, std::string &error) {
  const uint64_t logical = uint64_t{Jobs_} + root.TemplateCount;
  if (logical > std::numeric_limits<uint32_t>::max() / sizeof(std::array<uint32_t, 2>)) {
    error = "resident cluster queue exceeds GPU addressing";
    return false;
  }
  root.LogicalFirst = Jobs_;
  TableRoots_.push_back(root);
  Jobs_ = static_cast<uint32_t>(logical);
  return true;
}

bool GeometryRenderer::PrepareMainMeshTables(std::string &error) {
  TableTemplates_.clear();
  TableSpheres_ = MainMeshClusterSpheres_;
  const uint32_t mainIndexFirst = Bound().MainIndexRange().First;
  const bool clustered = !MainMeshClusterJobs_.empty() && !TableSpheres_.empty();
  if (clustered) {
    for (size_t at = 0; at < MainMeshClusterJobs_.size(); at += DrawList::kJobWords) {
      const auto &jobs = MainMeshClusterJobs_;
      TableTemplates_.push_back({.SphereIndex = jobs[at],
                                 .RelativeFirstIndex = mainIndexFirst + jobs[at + 2],
                                 .IndexCount = jobs[at + 3]});
    }
  }
  for (uint32_t at = 0; at < Batches.size(); ++at) {
    auto &batch = Batches[at];
    if (!clustered) { batch.FirstJob = batch.JobCount = 0; }
    if (batch.FirstJob > TableTemplates_.size() ||
        batch.JobCount > TableTemplates_.size() - batch.FirstJob) {
      error = "main cluster range exceeds its resident templates";
      return false;
    }
    const auto templates = std::span(TableTemplates_).subspan(batch.FirstJob, batch.JobCount);
    uint64_t capacity = 0;
    for (const auto &one : templates) { capacity += one.IndexCount; }
    if (capacity > std::numeric_limits<uint32_t>::max()) {
      error = "main cluster index capacity exceeds GPU addressing";
      return false;
    }
    const auto sphere = TemplateSphere(templates, TableSpheres_);
    uint32_t flags = sphere ? kGeometryRootBound : 0u;
    if (!MainMeshBoundsCurrent_ || batch.Instances != 1) { flags = kGeometryUnboundedDeformation; }
    if (!AppendRoot({.LocalSphere = sphere.value_or(BatchSphere{}),
                     .TemplateFirst = batch.FirstJob,
                     .TemplateCount = batch.JobCount,
                     .BatchRow = at,
                     .ModelSlot = batch.ModelSlot,
                     .IndexCapacity = static_cast<uint32_t>(capacity),
                     .Flags = flags},
                    error)) {
      return false;
    }
  }
  RootBoundsStale_ = !MainMeshBoundsCurrent_;
  return true;
}

void GeometryRenderer::OrderPieces() {
  TableOrder_.clear();
  size_t clusters = 0;
  size_t roots = Batches.size();
  auto nextRow = static_cast<uint32_t>(std::max(Placed_.size() / 16u, size_t{MainMeshRows_}));
  for (Piece &piece : Pieces_) {
    piece.FirstBatch.reset();
    piece.FirstRow = nextRow;
    nextRow += static_cast<uint32_t>(piece.Rows.size());
  }
  for (uint32_t at = 0; at < Pieces_.size(); ++at) {
    const auto &one = Pieces_[at];
    if (!one.Live || one.Rows.empty() || MaterialSlotFor(one) >= Slots.size()) { continue; }
    TableOrder_.push_back(at);
    if (!PiecesVisible_) { continue; }
    clusters += one.Clusters.size();
    roots += one.Clusters.empty() ? 1 : one.Rows.size();
  }
  TableTemplates_.reserve(TableTemplates_.size() + clusters);
  TableSpheres_.reserve(TableSpheres_.size() + clusters * kSphereFloats);
  TableRoots_.reserve(roots);
  std::ranges::sort(TableOrder_, [this](uint32_t a, uint32_t b) {
    const Piece &left = Pieces_[a];
    const Piece &right = Pieces_[b];
    if (MaterialSlotFor(left) != MaterialSlotFor(right)) {
      return MaterialSlotFor(left) < MaterialSlotFor(right);
    }
    if (left.Layout != right.Layout) { return left.Layout < right.Layout; }
    if (left.ProductOrder != right.ProductOrder) { return left.ProductOrder < right.ProductOrder; }
    return a < b;
  });
}

bool GeometryRenderer::UploadTables(std::string &error) {
  if (Jobs_ == 0 || TableSpheres_.empty()) { return true; }
  const auto *device = Bound().Device();
  uint64_t placements = std::max(Placed_.size() / 16u, size_t{MainMeshRows_});
  for (const auto &one : Pieces_) { placements += one.Rows.size(); }
  if (device == nullptr || placements > std::numeric_limits<uint32_t>::max()) {
    error = "resident hierarchy requires an admitted device and placement count";
    return false;
  }
  auto admitted = AdmitGeometryHierarchy(
      {.Roots = TableRoots_,
       .Templates = TableTemplates_,
       .SphereCount = static_cast<uint32_t>(TableSpheres_.size() / kSphereFloats),
       .PlacementCount = static_cast<uint32_t>(placements),
       .SourceIndexCount = Bound().IndexRoom(),
       .BatchCount = static_cast<uint32_t>(Batches.size())},
      device->Limits());
  if (!admitted) {
    error = std::move(admitted.error().Message);
    return false;
  }
  Hierarchy_ = std::move(*admitted);
  Args_.assign(Batches.size() * kArgWords, 0u);
  uint32_t base = 0;
  for (size_t at = 0; at < Batches.size(); ++at) {
    Args_[at * kArgWords + 1] = Batches[at].Instances;
    Args_[at * kArgWords + 2] = base;
    Args_[at * kArgWords + 4] = Batches[at].ModelSlot;
    base += TableRoots_[at].IndexCapacity;
  }
  using S = GeometryResidency::Stream;
  const auto read = Native::BufferUse::Storage;
  const auto crossing = []<class T>(S which, std::span<const T> values) {
    return GeometryResidency::Crossing{.Which = which,
                                       .Usage = Native::BufferUse::Storage,
                                       .From = values.data(),
                                       .Bytes = static_cast<uint32_t>(values.size_bytes())};
  };
  const std::array<GeometryQueueState, 1> reset{};
  std::vector<uint32_t> slots(Jobs_, std::numeric_limits<uint32_t>::max());
  slots.resize(slots.size() + Batches.size(), kGeometryIndexCold);
  std::array cut{
      crossing(S::ClusterSpheres, std::span<const float>(TableSpheres_)),
      crossing(S::ClusterTemplates, std::span<const GeometryClusterTemplate>(TableTemplates_)),
      crossing(S::ClusterRoots, std::span<const GeometryRoot>(TableRoots_)),
      crossing(S::ClusterPrefix, std::span<const uint32_t>(Hierarchy_.ChunkPrefix)),
      crossing(S::ClusterSlot, std::span<const uint32_t>(slots)),
      GeometryResidency::Crossing{.Which = S::ClusterReset,
                                  .Usage = Native::BufferUse::CopySource,
                                  .From = reset.data(),
                                  .Bytes = sizeof(GeometryQueueState)}};
  if (!Accepted(Bound().Cross(cut, true), error)) { return false; }
  constexpr auto discard = GeometryResidency::ExistingContents::Discard;
  const auto grow = [&](S which, uint32_t bytes, Native::UseMask<Native::BufferUse> usage) {
    return Accepted(
        Bound().Grow(which,
                     {.Usage = usage, .Bytes = bytes, .Existing = discard, .Deferred = true}),
        error);
  };
  const auto &view = Hierarchy_.View;
  const auto indirect = read | Native::BufferUse::Indirect;
  if (!grow(S::ClusterKept, Jobs_ * sizeof(uint32_t), read) ||
      !grow(S::ClusterVisible, view.RootCount * sizeof(uint32_t), read) ||
      !grow(S::ClusterChunks, view.ChunkCapacity * sizeof(std::array<uint32_t, 2>), read) ||
      !grow(S::ClusterSelected, view.ClusterCapacity * sizeof(std::array<uint32_t, 2>), read) ||
      !grow(S::ClusterChunkState, sizeof(GeometryQueueState), indirect) ||
      !grow(S::ClusterSelectedState, sizeof(GeometryQueueState), indirect) ||
      !grow(S::DrawIndex,
            Hierarchy_.IndexWords * sizeof(uint32_t),
            read | Native::BufferUse::Index) ||
      !grow(S::DrawArguments, static_cast<uint32_t>(Args_.size() * sizeof(uint32_t)), indirect)) {
    return false;
  }
  RootBoundsStale_ = false;
  return HandDrawArguments(true, error);
}

bool GeometryRenderer::HandRootBounds(std::string &error) {
  if (!RootBoundsStale_ || TableRoots_.empty() || Jobs_ == 0) { return true; }
  const size_t count = std::min(MainMeshBatches_.size(), TableRoots_.size());
  for (size_t at = 0; at < count; ++at) { TableRoots_[at].Flags = kGeometryUnboundedDeformation; }
  const auto roots = std::span(TableRoots_).first(count);
  std::array crossing{
      GeometryResidency::Crossing{.Which = GeometryResidency::Stream::ClusterRoots,
                                  .Usage = Native::BufferUse::Storage,
                                  .From = roots.data(),
                                  .Bytes = static_cast<uint32_t>(roots.size_bytes())}};
  if (!Accepted(Bound().Cross(crossing, true), error)) { return false; }
  RootBoundsStale_ = false;
  return true;
}
}
