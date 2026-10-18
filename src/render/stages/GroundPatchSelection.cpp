#include "GroundPatchSelection.h"
#include "ComputeShaders.h"
#include "StagePipelines.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
constexpr uint32_t kCullThreads = FindComputeShader(ComputeShaderId::GroundPatchCull)->Shape.GroupX;
constexpr uint32_t kScanThreads = FindComputeShader(ComputeShaderId::GroundPatchScan)->Shape.GroupX;
constexpr uint32_t kCompactThreads =
    FindComputeShader(ComputeShaderId::GroundPatchCompact)->Shape.GroupX;

Native::Binding Storage(uint32_t set,
                        uint32_t slot,
                        const Native::Buffer &buffer,
                        uint64_t offset = 0,
                        uint64_t bytes = 0) {
  return {
      .Set = set,
      .Slot = slot,
      .Value = Native::StorageBuffer{
          .Data = &buffer,
          .Range = {.Offset = offset, .Bytes = bytes > 0 ? bytes : buffer.Description().Bytes}}};
}

Native::Binding Uniform(std::span<const std::byte> bytes) {
  return {.Set = 2, .Slot = 0, .Value = Native::Uniform{.Bytes = bytes}};
}
}

Native::Result<void> GroundPatchSelection::Configure(Native::Device &device, Pipelines &pipelines) {
  if (pipelines.Cull && pipelines.LocalScan && pipelines.Scan && pipelines.Compact) { return {}; }
  auto cull = CreateComputePipeline(device, ComputeShaderId::GroundPatchCull);
  if (!cull) { return std::unexpected(cull.error()); }
  auto local = CreateComputePipeline(device, ComputeShaderId::GroundPatchScan);
  if (!local) { return std::unexpected(local.error()); }
  auto scan = CreateComputePipeline(device, ComputeShaderId::SubjectScan);
  if (!scan) { return std::unexpected(scan.error()); }
  auto compact = CreateComputePipeline(device, ComputeShaderId::GroundPatchCompact);
  if (!compact) { return std::unexpected(compact.error()); }
  pipelines.Cull = std::move(*cull);
  pipelines.LocalScan = std::move(*local);
  pipelines.Scan = std::move(*scan);
  pipelines.Compact = std::move(*compact);
  return {};
}

Native::Result<void> GroundPatchSelection::Cull(Native::Commands &commands,
                                                const FrameContext &frame,
                                                const Vec3 &anchor,
                                                const Native::Viewport &viewport,
                                                Native::Buffer &pyramid,
                                                const PyramidShape &shape,
                                                const Native::Buffer &candidates,
                                                float errorPx) {
  Ready_ = false;
  if (Seen_.empty()) { return {}; }
  if (Pipelines_ == nullptr || !Pipelines_->Cull || !Pipelines_->LocalScan || !Pipelines_->Scan ||
      !Pipelines_->Compact || !Bounds_ || !Slots_ || !Kept_ || !Batch_ || !Selected_ ||
      !Arguments_ || !Ranges_ || !Totals_ || !Offsets_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground patch selection is not prepared"});
  }
  View view{.Clip = frame.Mvp,
            .Viewport = {viewport.X, viewport.Y, viewport.Width, viewport.Height},
            .Wide = shape.Wide,
            .High = shape.High,
            .At = shape.At,
            .Source = {static_cast<uint32_t>(Seen_.size()),
                       shape.SourceWide,
                       shape.SourceHigh,
                       kScanThreads},
            .Work = {Room_, Blocks_, kScanThreads, 4},
            .Detail = {errorPx, 0, 0, 0}};
  for (int axis = 0; axis < 3; ++axis) {
    view.Shift[axis] = static_cast<float>(anchor[axis] + frame.PreViewTranslation[axis]);
  }
  const auto accepted = Select(commands, view, pyramid, candidates);
  Ready_ = accepted.has_value();
  return accepted;
}

Native::Result<void> GroundPatchSelection::Select(Native::Commands &commands,
                                                  const View &view,
                                                  Native::Buffer &pyramid,
                                                  const Native::Buffer &candidates) {
  const std::array cull{Storage(0, 0, *Bounds_),
                        Storage(0, 1, pyramid),
                        Storage(1, 0, *Kept_),
                        Uniform(std::as_bytes(std::span(&view, 1)))};
  auto accepted = commands.Dispatch(
      *Pipelines_->Cull, cull, {.X = (view.Source[0] + kCullThreads - 1u) / kCullThreads});
  if (!accepted) { return accepted; }
  const std::array local{Storage(0, 0, *Kept_),
                         Storage(0, 1, *Ranges_),
                         Storage(1, 0, *Slots_),
                         Storage(1, 1, *Totals_)};
  accepted = commands.Dispatch(*Pipelines_->LocalScan, local, {.X = 4u * Blocks_});
  if (!accepted) { return accepted; }
  const std::array scan{Storage(0, 0, *Totals_),
                        Storage(0, 1, *Batch_),
                        Storage(1, 0, *Offsets_),
                        Storage(1, 1, *Arguments_, CounterOffset_, 19u * sizeof(uint32_t))};
  accepted = commands.Dispatch(*Pipelines_->Scan, scan, {.X = 4});
  if (!accepted) { return accepted; }
  const std::array compactView{view.Source, view.Work};
  const std::array compact{Storage(0, 0, candidates),
                           Storage(0, 1, *Slots_),
                           Storage(0, 2, *Offsets_),
                           Storage(1, 0, *Selected_),
                           Uniform(std::as_bytes(std::span(compactView)))};
  return commands.Dispatch(*Pipelines_->Compact,
                           compact,
                           {.X = (view.Source[0] + kCompactThreads - 1u) / kCompactThreads});
}
}
