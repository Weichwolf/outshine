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

Native::Result<void> GroundPatchSelection::Configure(Native::Device &device) {
  if (Cull_ && Scan_ && Compact_) { return {}; }
  auto cull = CreateComputePipeline(device, ComputeShaderId::GroundPatchCull);
  if (!cull) { return std::unexpected(cull.error()); }
  auto scan = CreateComputePipeline(device, ComputeShaderId::SubjectScan);
  if (!scan) { return std::unexpected(scan.error()); }
  auto compact = CreateComputePipeline(device, ComputeShaderId::GroundPatchCompact);
  if (!compact) { return std::unexpected(compact.error()); }
  Cull_ = std::move(*cull);
  Scan_ = std::move(*scan);
  Compact_ = std::move(*compact);
  return {};
}

Native::Result<void> GroundPatchSelection::Cull(Native::Commands &commands,
                                                const FrameContext &frame,
                                                const Vec3 &anchor,
                                                const Native::Viewport &viewport,
                                                Native::Buffer &pyramid,
                                                const PyramidShape &shape,
                                                const Native::Buffer &candidates) {
  Ready_ = false;
  if (Seen_.empty()) { return {}; }
  if (!Cull_ || !Scan_ || !Compact_ || !Bounds_ || !Slots_ || !Kept_ || !Batch_ || !Selected_ ||
      !Arguments_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground patch selection is not prepared"});
  }
  View view{.Clip = frame.Mvp,
            .Viewport = {viewport.X, viewport.Y, viewport.Width, viewport.Height},
            .Wide = shape.Wide,
            .High = shape.High,
            .At = shape.At,
            .Source = {static_cast<uint32_t>(Seen_.size()), shape.SourceWide, shape.SourceHigh, 0}};
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
  auto accepted =
      commands.Dispatch(*Cull_, cull, {.X = (view.Source[0] + kCullThreads - 1u) / kCullThreads});
  if (!accepted) { return accepted; }
  const std::array scan{Storage(0, 0, *Kept_),
                        Storage(0, 1, *Batch_),
                        Storage(1, 0, *Slots_),
                        Storage(1, 1, *Arguments_, CounterOffset_, 4u * sizeof(uint32_t))};
  accepted = commands.Dispatch(*Scan_, scan, {.X = 1});
  if (!accepted) { return accepted; }
  const std::array compact{Storage(0, 0, candidates),
                           Storage(0, 1, *Slots_),
                           Storage(1, 0, *Selected_),
                           Uniform(std::as_bytes(std::span(&view.Source, 1)))};
  return commands.Dispatch(
      *Compact_, compact, {.X = (view.Source[0] + kCompactThreads - 1u) / kCompactThreads});
}
}
