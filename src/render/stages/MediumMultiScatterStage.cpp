#include "MediumMultiScatterStage.h"
#include "StagePipelines.h"
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {

Native::Result<void> MediumMultiScatterStage::Configure(const Gpu &gpu,
                                                        Native::Texture *transmittance,
                                                        Native::Sampler *lut,
                                                        Native::Texture *into) {
  if (gpu.Device == nullptr || transmittance == nullptr || lut == nullptr || into == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "multiple scattering needs complete feeds"});
  }
  if (!Pipe) {
    auto made = CreateComputePipeline(*gpu.Device, Shader);
    if (!made) { return std::unexpected(made.error()); }
    Pipe = std::move(*made);
    Cache_.Invalidate();
  }
  if (Into != into || Transmittance != transmittance || Lut != lut) { Cache_.Invalidate(); }
  Transmittance = transmittance;
  Lut = lut;
  Into = into;
  return {};
}

void MediumMultiScatterStage::Declare(const Medium &medium) {
  if (Cache_.Submitted() && Declared_ == medium) { return; }
  Declared_ = medium;
  Cache_.Invalidate();
}

Native::Result<void> MediumMultiScatterStage::Encode(const PassRecording &into) {
  if (!Cache_.NeedsRecording()) { return {}; }
  if (!Pipe || !Declared_ || Transmittance == nullptr || Lut == nullptr || Into == nullptr ||
      into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "multiple scattering is not configured"});
  }
  const std::array bindings{
      Native::Binding{
          .Set = 0,
          .Slot = 0,
          .Value = Native::SampledTexture{.Image = {.Image = Transmittance}, .Sampling = Lut}},
      Native::Binding{
          .Set = 1, .Slot = 0, .Value = Native::StorageTexture{.Image = {.Image = Into}}},
      Native::Binding{
          .Set = 2,
          .Slot = 0,
          .Value = Native::Uniform{.Bytes = std::as_bytes(std::span{&*Declared_, size_t{1}})}}};
  const auto encoded = into.Commands->Dispatch(
      *Pipe,
      bindings,
      {.X = (kMultiScatterLutSize + KernelShape.GroupX - 1u) / KernelShape.GroupX,
       .Y = (kMultiScatterLutSize + KernelShape.GroupY - 1u) / KernelShape.GroupY,
       .Z = 1u});
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::MediumMultiScatter, Cache_);
  return {};
}

}
