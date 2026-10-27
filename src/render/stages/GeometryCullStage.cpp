#include "GeometryCullStage.h"
#include "PipelineFactory.h"
#include "Units.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
constexpr size_t kPlaneFloats = 24;
std::atomic<uint32_t> gJobsAdmitted{0};

void PlanesOf(const Mat4f &mvp, std::span<float, kPlaneFloats> out, const Vec2f &guard = {}) {
  const auto row = [mvp](int r, int c) { return mvp[c * 4 + r]; };
  for (int at = 0; at < 6; ++at) {
    const int axis = at / 2;
    const bool minus = at % 2 == 1;
    for (int c = 0; c < 4; ++c) {
      const float w = row(3, c) * (axis < 2 ? 1.0f + guard[axis] : 1.0f);
      const float a = row(axis, c);
      const float plus = axis == 2 ? a : w + a;
      out[at * 4 + c] = minus ? w - a : plus;
    }
    const float length =
        std::sqrt(out[static_cast<size_t>(at) * 4] * out[static_cast<size_t>(at) * 4] +
                  out[at * 4 + 1] * out[at * 4 + 1] + out[at * 4 + 2] * out[at * 4 + 2]);
    if (length < static_cast<float>(kParallelCross)) {
      for (int c = 0; c < 4; ++c) { out[at * 4 + c] = 0.0f; }
      continue;
    }
    for (int c = 0; c < 4; ++c) { out[at * 4 + c] /= length; }
  }
}

Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = Native::StorageBuffer{.Data = &buffer,
                                         .Range = {.Bytes = buffer.Description().Bytes}}};
}
}

Native::Result<void>
GeometryCullStage::EnsurePipeline(Native::Device &device,
                                  ComputeShaderId shader,
                                  std::unique_ptr<Native::ComputePipeline> &into) {
  if (into) { return {}; }
  auto made = CreateComputePipeline(device, shader);
  if (!made) { return std::unexpected(made.error()); }
  into = std::move(*made);
  return {};
}

Native::Result<void> GeometryCullStage::Configure(Source source, const Gpu &gpu) {
  if (gpu.Device == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "culling requires a device"});
  }
  const auto root = EnsurePipeline(*gpu.Device, RootShader, Root_);
  if (!root) { return root; }
  const auto cull = EnsurePipeline(*gpu.Device, CullShader, Cull_);
  if (!cull) { return cull; }
  const auto scan = EnsurePipeline(*gpu.Device, ScanShader, Scan_);
  if (!scan) { return scan; }
  const auto compact = EnsurePipeline(*gpu.Device, CompactShader, Compact_);
  if (!compact) { return compact; }
  Binds(source);
  return {};
}

void GeometryCullStage::Binds(Source source) noexcept {
  if (Source_.Resident != source.Resident || Source_.AnchorM != source.AnchorM ||
      Source_.Jobs != source.Jobs || Source_.BatchRows != source.BatchRows ||
      Source_.Hierarchy != source.Hierarchy) {
    Invalidate();
  }
  Source_ = source;
}

uint32_t GeometryCullStage::JobsAdmittedTaken() {
  return gJobsAdmitted.load(std::memory_order_relaxed);
}

void GeometryCullStage::Standing(const FrameContext &ctx, const SelectionProjection &projection) {
  auto &into = View_;
  into = CullView{};
  PlanesOf(projection.Clip, into.Planes);
  for (int axis = 0; axis < 3; ++axis) {
    into.Shift[axis] = static_cast<float>(Source_.AnchorM[axis] + ctx.PreViewTranslation[axis]);
  }
  into.Jobs = Source_.Jobs;
  into.Clip = ctx.Mvp;
  into.PyramidWide = Pyramid_.Wide;
  into.PyramidHigh = Pyramid_.High;
  into.PyramidAt = Pyramid_.At;
  into.Occludes = OccludeThisFrame_ ? 1u : 0u;
  into.SourceWide = Pyramid_.SourceWide;
  into.SourceHigh = Pyramid_.SourceHigh;
  const float *const up = into.Planes.data() + static_cast<size_t>(2U * 4U);
  const float *const down = into.Planes.data() + static_cast<size_t>(3U * 4U);
  const float between = up[0] * down[0] + up[1] * down[1] + up[2] * down[2];
  const float yfov = std::acos(std::fmin(std::fmax(-between, -1.0f), 1.0f));
  const float halfTangent = std::tan(0.5f * yfov);
  into.ErrorPerMetre = halfTangent > static_cast<float>(kLeastRunM) && HeightPx_ > 0.0f
                           ? HeightPx_ * 0.5f / halfTangent
                           : 0;
  if (projection.GuardNdc != Vec2f{}) {
    PlanesOf(projection.Clip, into.Planes, projection.GuardNdc);
  }
}

Native::Result<void> GeometryCullStage::EncodeRoots(const FrameContext &ctx,
                                                    const PassRecording &into,
                                                    const SelectionProjection &projection) {
  Swept_ = 0;
  const bool unchanged = Cache_.Submitted() && LastProjection_ == projection &&
                         LastPreViewTranslation_ == ctx.PreViewTranslation &&
                         LastGeometryGeneration_ == Source_.Generation &&
                         LastGroundGeneration_ == Source_.GroundGeneration;
  if (!unchanged) { DepthForSelection_ = false; }
  OccludeThisFrame_ = DepthForSelection_ && PyramidBuffer_ != nullptr && unchanged &&
                      projection.GuardNdc == Vec2f{} && projection.Clip == ctx.Mvp;
  CullThisFrame_ = Source_.Resident != nullptr && Source_.Jobs != 0 &&
                   (!unchanged || (OccludeThisFrame_ && !Refined_));
  if (!CullThisFrame_) {
    gJobsAdmitted.store(0, std::memory_order_relaxed);
    return {};
  }
  Cache_.Invalidate();
  if (!Root_ || Source_.Hierarchy == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "culling stage is not configured"});
  }
  const auto &resident = *Source_.Resident;
  using S = GeometryResidency::Stream;
  const std::array read{resident.Buffer(S::ClusterRoots).get(),
                        resident.Buffer(S::Placements).get(),
                        resident.Buffer(S::ClusterReset).get(),
                        resident.Buffer(S::ClusterChunks).get(),
                        resident.Buffer(S::ClusterChunkState).get(),
                        resident.Buffer(S::ClusterVisible).get()};
  for (const auto *buffer : read) {
    if (buffer == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "root culling buffer is missing"});
    }
  }
  auto *chunkState = resident.Buffer(S::ClusterChunkState).get();
  const auto reset =
      into.Commands->Copy(*read[2], *chunkState, {.Bytes = sizeof(GeometryQueueState)});
  if (!reset) { return reset; }
  Standing(ctx, projection);
  const auto &hierarchy = *Source_.Hierarchy;
  const std::array bindings{
      Storage(0, 0, *read[0]),
      Storage(0, 1, *read[1]),
      Storage(1, 0, *read[3]),
      Storage(1, 1, *read[4]),
      Storage(1, 2, *read[5]),
      Native::Binding{
          .Set = 2, .Slot = 0, .Value = Native::Uniform{std::as_bytes(std::span(&View_, 1))}},
      Native::Binding{.Set = 2,
                      .Slot = 1,
                      .Value = Native::Uniform{std::as_bytes(std::span(&hierarchy.View, 1))}}};
  const auto encoded = into.Commands->Dispatch(*Root_, bindings, hierarchy.RootGroups);
  if (!encoded) { return encoded; }
  LastProjection_ = projection;
  LastPreViewTranslation_ = ctx.PreViewTranslation;
  LastGeometryGeneration_ = Source_.Generation;
  LastGroundGeneration_ = Source_.GroundGeneration;
  Refined_ = OccludeThisFrame_;
  Swept_ = Source_.Jobs;
  gJobsAdmitted.store(Swept_, std::memory_order_relaxed);
  return {};
}

}
