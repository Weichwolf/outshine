#ifndef OUTSHINE_RENDER_VISIBILITY_DEPTHPYRAMIDPASS_H
#define OUTSHINE_RENDER_VISIBILITY_DEPTHPYRAMIDPASS_H

#include <memory>

#include "DepthPyramid.h"
#include "Gpu.h"
#include "ComputeShaders.h"

#include <Extent.h>

namespace outshine::Render {

class DepthPyramidPass {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::DepthPyramid;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  [[nodiscard]] Native::Result<void> Allocate(Native::Device &device, Extent size);

  [[nodiscard]] Native::Result<void> Configure(Native::Texture *depth, Native::Sampler *held);

  [[nodiscard]] Native::Buffer *Buffer() const noexcept { return Buffer_.get(); }

  [[nodiscard]] const PyramidShape &Shape() const noexcept { return Shape_; }

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into);

  [[nodiscard]] bool Ready() const {
    return Pipeline_ && Buffer_ && SourceDepth_ != nullptr && Sampler_ != nullptr;
  }

private:
  std::unique_ptr<Native::ComputePipeline> Pipeline_;
  std::unique_ptr<Native::Buffer> Buffer_;
  Native::Device *Device_ = nullptr;
  Native::Texture *SourceDepth_ = nullptr;
  Native::Sampler *Sampler_ = nullptr;
  PyramidShape Shape_;
};

}
#endif
