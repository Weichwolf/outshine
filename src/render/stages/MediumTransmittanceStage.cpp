#include "MediumTransmittanceStage.h"
#include "StagePipelines.h"
#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {

Native::Result<void> MediumTransmittanceStage::Configure(const Gpu &gpu, Native::Texture *lut) {
  if (gpu.Device == nullptr || lut == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "transmittance needs a device and target"});
  }
  if (!Pipe) {
    auto made = CreateComputePipeline(*gpu.Device, Shader);
    if (!made) { return std::unexpected(made.error()); }
    Pipe = std::move(*made);
    Cache_.Invalidate();
  }
  if (Lut != lut) { Cache_.Invalidate(); }
  Lut = lut;
  return {};
}

void MediumTransmittanceStage::Declare(const Medium &medium) {
  if (Cache_.Submitted() && Declared_ == medium) { return; }
  Declared_ = medium;
  Cache_.Invalidate();
}

Native::Result<void> MediumTransmittanceStage::Encode(const PassRecording &into) {
  if (!Cache_.NeedsRecording()) { return {}; }
  if (!Pipe || !Declared_ || Lut == nullptr || into.Commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "transmittance stage is not configured"});
  }
  const std::array bindings{
      Native::Binding{
          .Set = 1, .Slot = 0, .Value = Native::StorageTexture{.Image = {.Image = Lut}}},
      Native::Binding{
          .Set = 2,
          .Slot = 0,
          .Value = Native::Uniform{.Bytes = std::as_bytes(std::span{&*Declared_, size_t{1}})}}};
  const auto encoded = into.Commands->Dispatch(
      *Pipe,
      bindings,
      {.X = (kTransmittanceLutWidth + KernelShape.GroupX - 1u) / KernelShape.GroupX,
       .Y = (kTransmittanceLutHeight + KernelShape.GroupY - 1u) / KernelShape.GroupY,
       .Z = 1u});
  if (!encoded) { return encoded; }
  into.Submission.Record(Stage::MediumTransmittance, Cache_);
  return {};
}

}
