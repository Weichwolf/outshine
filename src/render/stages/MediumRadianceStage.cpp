#include "MediumRadianceStage.h"
#include "StagePipelines.h"
#include "math/Vec2.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
constexpr float kMPerKmF = 1000.0f;

struct alignas(16) Pushed {
  Medium Declared;
  float CosSunZenith;
  float EyeRadiusKm;
  Vec2f Pad;
};

static_assert(sizeof(Pushed) == sizeof(Medium) + 16 && alignof(Pushed) == 16);
static_assert(offsetof(Pushed, CosSunZenith) == sizeof(Medium));
static_assert(offsetof(Pushed, EyeRadiusKm) == sizeof(Medium) + sizeof(float));
static_assert(offsetof(Pushed, Pad) == sizeof(Medium) + 2 * sizeof(float));
}

Native::Result<void> MediumRadianceStage::Configure(const Gpu &gpu,
                                                    Native::Texture *transmittance,
                                                    Native::Texture *multiScatter,
                                                    Native::Sampler *lut,
                                                    Native::Texture *into) {
  if (gpu.Device == nullptr || transmittance == nullptr || multiScatter == nullptr ||
      lut == nullptr || into == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "sky radiance needs complete feeds"});
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

void MediumRadianceStage::Declare(const Medium &medium, float cosSunZenith, float eyeHeightM) {
  const Standing wanted = {
      .Declared = medium, .CosSunZenith = cosSunZenith, .EyeHeightM = eyeHeightM};
  if (Cache_.Submitted() && Standing_ == wanted) { return; }
  Standing_ = wanted;
  Cache_.Invalidate();
}

Native::Result<void> MediumRadianceStage::Encode(const PassRecording &into) {
  if (!Cache_.NeedsRecording()) { return {}; }
  if (!Pipe || !Standing_ || Transmittance == nullptr || MultiScatter == nullptr ||
      Lut == nullptr || Into == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "sky radiance is not configured"});
  }
  const Pushed pushed{.Declared = Standing_->Declared,
                      .CosSunZenith = Standing_->CosSunZenith,
                      .EyeRadiusKm = Standing_->Declared.BottomRadiusKm + kMediumGroundLiftKm +
                                     std::fmax(0.0f, Standing_->EyeHeightM) / kMPerKmF,
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
          .Set = 1, .Slot = 0, .Value = Native::StorageTexture{.Image = {.Image = Into}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value =
                          Native::Uniform{.Bytes = std::as_bytes(std::span{&pushed, size_t{1}})}}};
  const auto encoded = into.Commands->Dispatch(
      *Pipe,
      bindings,
      {.X = (kSkyViewLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
       .Y = (kSkyViewLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
       .Z = 1u});
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::MediumRadiance, Cache_);
  return {};
}

}
