#include "TonemapStage.h"
#include "StagePipelines.h"
#include "TransformMatrix.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {

namespace {

struct alignas(16) TemporalUniform {
  Vec2f JitterDelta;
  Vec2f Texel;
  float HistoryHeld;
  Vec3f Pad;
  Mat4f PreviousClipFromCurrentClip;
};

static_assert(sizeof(TemporalUniform) == 6 * sizeof(Vec4f) && alignof(TemporalUniform) == 16);
static_assert(offsetof(TemporalUniform, JitterDelta) == 0);
static_assert(offsetof(TemporalUniform, Texel) == 8);
static_assert(offsetof(TemporalUniform, HistoryHeld) == 16);
static_assert(offsetof(TemporalUniform, Pad) == 16 + sizeof(float));
static_assert(offsetof(TemporalUniform, PreviousClipFromCurrentClip) == 32);

TemporalUniform
TemporalParameters(const FrameContext &ctx, Vec2f jitter, int width, int height, bool historyHeld) {
  TemporalUniform result{
      .JitterDelta = jitter,
      .Texel = {{1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height)}},
      .HistoryHeld = historyHeld ? 1.0f : 0.0f,
      .Pad = {},
      .PreviousClipFromCurrentClip = {}};
  Mat4 current;
  Mat4 previous;
  Mat4 inverse;
  for (size_t i = 0; i < 16; ++i) {
    current[i] = ctx.Mvp[i];
    previous[i] = ctx.PrevMvp[i];
  }
  if (InverseMatrix(current, inverse)) {
    const Mat4 reprojected = previous * inverse;
    for (size_t i = 0; i < 16; ++i) {
      result.PreviousClipFromCurrentClip[i] = static_cast<float>(reprojected[i]);
    }
  } else {
    result.HistoryHeld = 0.0f;
  }
  return result;
}

Native::Binding
ImageBinding(uint32_t slot, const Native::Texture *image, const Native::Sampler *sampler) {
  return {.Set = 2,
          .Slot = slot,
          .Value = Native::SampledTexture{.Image = {.Image = image}, .Sampling = sampler}};
}

}

Native::Result<void>
TonemapStage::Configure(const Gpu &gpu, const Feeds &from, const DisplayOptions &options) {
  if (gpu.Device == nullptr || from.Scene == nullptr || from.Depth == nullptr ||
      from.Exact == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "display transfer requires complete feeds"});
  }
  const std::array targets{
      Native::ColourTarget{
          .Texels = options.Temporal ? from.Linear : gpu.SurfaceFormat, .Rgb = {}, .Alpha = {}},
      Native::ColourTarget{.Texels = gpu.SurfaceFormat, .Rgb = {}, .Alpha = {}}};
  auto pipeline =
      CreateFullscreenPipeline(*gpu.Device,
                               options.Temporal ? "build/shaders/temporalResolve.frag.spv"
                                                : "build/shaders/tonemap.frag.spv",
                               std::span(targets.data(), options.Temporal ? 2u : 1u));
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  Pipe = std::move(*pipeline);
  Scene = from.Scene;
  Depth = from.Depth;
  Exact = from.Exact;
  Temporal = options.Temporal;
  Display_ = options;
  return {};
}

Native::Result<void> TonemapStage::Encode(const FrameContext &ctx, const PassRecording &into) {
  if (!Pipe || Scene == nullptr || Depth == nullptr || Exact == nullptr ||
      into.Commands == nullptr ||
      (Temporal && (History == nullptr || Velocity == nullptr || Width <= 0 || Height <= 0))) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "display transfer is not configured"});
  }

  const struct {
    float Exposure;
    uint32_t Filmic;
    std::array<float, 2> Pad;
  } display{.Exposure = Display_.Exposure,
            .Filmic = Display_.Curve == Transfer::Filmic ? 1u : 0u,
            .Pad = {}};

  static_assert(sizeof(display) == 16);
  const Native::Uniform displayData{.Bytes = std::as_bytes(std::span{&display, size_t{1}})};
  std::array<Native::Binding, 6> bindings{};
  bindings[0] = ImageBinding(0, Scene, Exact);
  bindings[1] = ImageBinding(1, Depth, Exact);
  TemporalUniform temporal{};
  if (Temporal) {
    temporal = TemporalParameters(ctx, JitterDelta, Width, Height, HistoryHeld);
    bindings[2] = ImageBinding(2, History, Exact);
    bindings[3] = ImageBinding(3, Velocity, Exact);
    bindings[4] = {.Set = 3,
                   .Slot = 0,
                   .Value =
                       Native::Uniform{.Bytes = std::as_bytes(std::span{&temporal, size_t{1}})}};
    bindings[5] = {.Set = 3, .Slot = 1, .Value = displayData};
  } else {
    bindings[2] = {.Set = 3, .Slot = 0, .Value = displayData};
  }
  return into.Commands->Draw({.State = {.Pipeline = Pipe.get(),
                                        .Bindings = std::span(bindings.data(), Temporal ? 6u : 3u),
                                        .Vertices = {},
                                        .Area = into.Area,
                                        .Clip = into.Clip},
                              .Range = {.Count = 3}});
}

}
