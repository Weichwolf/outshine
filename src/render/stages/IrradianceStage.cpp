#include "IrradianceStage.h"
#include "StagePipelines.h"
#include "math/Vec2.h"
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
struct alignas(16) Pushed {
  Medium Declared;
  float CosSunZenith;
  float GroundRadiusKm;
  Vec2f Pad;
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16 && alignof(Pushed) == 16);
static_assert(offsetof(Pushed, CosSunZenith) == sizeof(Medium));
static_assert(offsetof(Pushed, GroundRadiusKm) == sizeof(Medium) + sizeof(float));
static_assert(offsetof(Pushed, Pad) == sizeof(Medium) + 2 * sizeof(float));
}

Native::Result<void> IrradianceStage::Configure(const Gpu &gpu,
                                                Native::Texture *transmittance,
                                                Native::Texture *multiScatter,
                                                Native::Sampler *lut,
                                                Native::Buffer *into) {
  if (gpu.Device == nullptr || transmittance == nullptr || multiScatter == nullptr ||
      lut == nullptr || into == nullptr ||
      into->Description().Bytes < kIrradianceFloats * sizeof(float)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "irradiance needs complete feeds and storage"});
  }
  if (!Pipe) {
    auto made = CreateComputePipeline(*gpu.Device, Shader);
    if (!made) { return std::unexpected(made.error()); }
    Pipe = std::move(*made);
    Cache_.Invalidate();
  }
  if (Into != into || Transmittance != transmittance || MultiScatter != multiScatter ||
      Lut != lut) {
    Cache_.Invalidate();
  }
  Transmittance = transmittance;
  MultiScatter = multiScatter;
  Lut = lut;
  Into = into;
  return {};
}

void IrradianceStage::Declare(const Medium &medium, float cosSunZenith) {
  const Standing wanted = {.Declared = medium, .CosSunZenith = cosSunZenith};
  if (Cache_.Submitted() && Standing_ == wanted) { return; }
  Standing_ = wanted;
  Cache_.Invalidate();
}

Native::Result<void> IrradianceStage::Encode(const PassRecording &into) {
  if (!Cache_.NeedsRecording()) { return {}; }
  if (!Pipe || !Standing_ || Transmittance == nullptr || MultiScatter == nullptr ||
      Lut == nullptr || Into == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "irradiance is not configured"});
  }
  const Pushed pushed{.Declared = Standing_->Declared,
                      .CosSunZenith = Standing_->CosSunZenith,
                      .GroundRadiusKm = Standing_->Declared.BottomRadiusKm + kMediumGroundLiftKm,
                      .Pad = {}};
  const std::array bindings{
      Native::Binding{
          .Set = 0,
          .Slot = 0,
          .Value = Native::SampledTexture{.Image = {.Image = Transmittance}, .Sampling = Lut}},
      Native::Binding{
          .Set = 0,
          .Slot = 1,
          .Value = Native::SampledTexture{.Image = {.Image = MultiScatter}, .Sampling = Lut}},
      Native::Binding{
          .Set = 1,
          .Slot = 0,
          .Value =
              Native::StorageBuffer{.Data = Into, .Range = {.Bytes = Into->Description().Bytes}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&pushed, size_t{1}})}}};
  const auto encoded = into.Commands->Dispatch(*Pipe, bindings, {.X = 1u, .Y = 1u, .Z = 1u});
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::Irradiance, Cache_);
  return {};
}

}
