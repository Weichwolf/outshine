#include "SubjectCullStage.h"
#include "StagePipelines.h"
#include "math/Units.h"
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
std::atomic<float> gErrorPerMetre{0};
std::atomic<uint32_t> gJobsSwept{0};

void PlanesOf(const Mat4f &mvp, std::span<float, kPlaneFloats> out) {
  const auto row = [mvp](int r, int c) { return mvp[c * 4 + r]; };
  for (int at = 0; at < 6; ++at) {
    const int axis = at / 2;
    const bool minus = at % 2 == 1;
    for (int c = 0; c < 4; ++c) {
      const float w = row(3, c);
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
SubjectCullStage::EnsurePipeline(Native::Device &device,
                                 ComputeShaderId shader,
                                 std::unique_ptr<Native::ComputePipeline> &into) {
  if (into) { return {}; }
  auto made = CreateComputePipeline(device, shader);
  if (!made) { return std::unexpected(made.error()); }
  into = std::move(*made);
  return {};
}

Native::Result<void> SubjectCullStage::Configure(Source source, const Gpu &gpu) {
  if (gpu.Device == nullptr) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "culling requires a device"});
  }
  const auto cull = EnsurePipeline(*gpu.Device, CullShader, Cull_);
  if (!cull) { return cull; }
  const auto scan = EnsurePipeline(*gpu.Device, ScanShader, Scan_);
  if (!scan) { return scan; }
  const auto compact = EnsurePipeline(*gpu.Device, CompactShader, Compact_);
  if (!compact) { return compact; }
  Binds(source);
  return {};
}

void SubjectCullStage::Binds(Source source) noexcept {
  if (Source_.Resident != source.Resident || Source_.AnchorM != source.AnchorM ||
      Source_.Jobs != source.Jobs || Source_.BatchRows != source.BatchRows) {
    Cache_.Invalidate();
  }
  Source_ = source;
}

uint32_t SubjectCullStage::JobsSweptTaken() {
  return gJobsSwept.load(std::memory_order_relaxed);
}

float SubjectCullStage::ErrorPerMetreTaken() {
  return gErrorPerMetre.load(std::memory_order_relaxed);
}

void SubjectCullStage::Standing(const FrameContext &ctx) {
  auto &into = View_;
  into = CullView{};
  PlanesOf(ctx.Mvp, into.Planes);
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
  gErrorPerMetre.store(into.ErrorPerMetre, std::memory_order_relaxed);
}

Native::Result<void> SubjectCullStage::EncodeCull(const FrameContext &ctx,
                                                  const PassRecording &into) {
  Swept_ = 0;
  const bool unchanged = Cache_.Submitted() && LastMvp_ == ctx.Mvp &&
                         LastPreViewTranslation_ == ctx.PreViewTranslation &&
                         LastSubjectGeneration_ == Source_.Generation &&
                         LastGroundGeneration_ == Source_.GroundGeneration;
  OccludeThisFrame_ = PyramidBuffer_ != nullptr && unchanged;
  CullThisFrame_ = Source_.Resident != nullptr && Source_.Jobs != 0 &&
                   (!unchanged || (OccludeThisFrame_ && !Refined_));
  if (!CullThisFrame_) {
    gJobsSwept.store(0, std::memory_order_relaxed);
    return {};
  }
  Cache_.Invalidate();
  if (!Cull_ || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "culling stage is not configured"});
  }
  const auto &resident = *Source_.Resident;
  using S = SubjectResidency::Stream;
  const std::array read{resident.Buffer(S::ClusterSpheres).get(),
                        resident.Buffer(S::ClusterJobs).get(),
                        resident.Buffer(S::Placements).get(),
                        resident.Buffer(S::DrawArguments).get(),
                        PyramidBuffer_ != nullptr ? PyramidBuffer_
                                                  : resident.Buffer(S::ClusterSpheres).get()};
  for (const auto *buffer : read) {
    if (buffer == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "culling source buffer is missing"});
    }
  }
  const auto *kept = resident.Buffer(S::ClusterKept).get();
  if (kept == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "culling output buffer is missing"});
  }
  Standing(ctx);
  const std::array bindings{
      Storage(0, 0, *read[0]),
      Storage(0, 1, *read[1]),
      Storage(0, 2, *read[2]),
      Storage(0, 3, *read[3]),
      Storage(0, 4, *read[4]),
      Storage(1, 0, *kept),
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&View_, size_t{1}})}}};
  const auto encoded = into.Commands->Dispatch(
      *Cull_,
      bindings,
      {.X = (Source_.Jobs + CullShape.GroupX - 1u) / CullShape.GroupX, .Y = 1, .Z = 1});
  if (!encoded) { return encoded; }
  LastMvp_ = ctx.Mvp;
  LastPreViewTranslation_ = ctx.PreViewTranslation;
  LastSubjectGeneration_ = Source_.Generation;
  LastGroundGeneration_ = Source_.GroundGeneration;
  Refined_ = OccludeThisFrame_;
  Swept_ = Source_.Jobs;
  gJobsSwept.store(Swept_, std::memory_order_relaxed);
  return {};
}

Native::Result<void> SubjectCullStage::EncodeScan(const PassRecording &into) {
  if (!CullThisFrame_) { return {}; }
  if (!Scan_ || Source_.Resident == nullptr || into.Commands == nullptr || Source_.BatchRows == 0) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "cull scan is not configured"});
  }
  const auto &resident = *Source_.Resident;
  using S = SubjectResidency::Stream;
  const std::array buffers{resident.Buffer(S::ClusterKept).get(),
                           resident.Buffer(S::ClusterBatches).get(),
                           resident.Buffer(S::ClusterSlot).get(),
                           resident.Buffer(S::DrawArguments).get()};
  for (const auto *buffer : buffers) {
    if (buffer == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "cull scan buffer is missing"});
    }
  }
  const std::array bindings{Storage(0, 0, *buffers[0]),
                            Storage(0, 1, *buffers[1]),
                            Storage(1, 0, *buffers[2]),
                            Storage(1, 1, *buffers[3])};
  return into.Commands->Dispatch(*Scan_, bindings, {.X = Source_.BatchRows, .Y = 1, .Z = 1});
}

Native::Result<void> SubjectCullStage::EncodeCompact(const PassRecording &into) {
  if (!CullThisFrame_) { return {}; }
  if (!Compact_ || Source_.Resident == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "cull compaction is not configured"});
  }
  const auto &resident = *Source_.Resident;
  using S = SubjectResidency::Stream;
  const std::array buffers{resident.Buffer(S::ClusterJobs).get(),
                           resident.Buffer(S::Index).get(),
                           resident.Buffer(S::ClusterSlot).get(),
                           resident.Buffer(S::DrawArguments).get(),
                           resident.Buffer(S::DrawIndex).get()};
  for (const auto *buffer : buffers) {
    if (buffer == nullptr) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "cull compaction buffer is missing"});
    }
  }
  const std::array bindings{
      Storage(0, 0, *buffers[0]),
      Storage(0, 1, *buffers[1]),
      Storage(0, 2, *buffers[2]),
      Storage(0, 3, *buffers[3]),
      Storage(1, 0, *buffers[4]),
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&View_, size_t{1}})}}};
  const auto encoded =
      into.Commands->Dispatch(*Compact_, bindings, {.X = Source_.Jobs, .Y = 1, .Z = 1});
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::SubjectCompact, Cache_);
  return {};
}

}
