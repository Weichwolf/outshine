#ifndef OUTSHINE_RENDER_VISIBILITY_GEOMETRYOCCLUSIONPASS_H
#define OUTSHINE_RENDER_VISIBILITY_GEOMETRYOCCLUSIONPASS_H

#include "DepthPyramidPass.h"
#include "FrameContext.h"
#include <array>
#include <memory>

namespace outshine::Render {
class GeometryRenderer;

class GeometryOcclusionPass {
public:
  [[nodiscard]] Native::Result<void> Allocate(Native::Device &device, Extent size) {
    return Pyramid_.Allocate(device, size);
  }

  [[nodiscard]] Native::Result<void>
  Configure(Native::Device &device, Native::Texture *depth, Native::Sampler *sampler);
  [[nodiscard]] Native::Result<void> EncodeDepth(GeometryRenderer &geometry,
                                                 const FrameContext &ctx,
                                                 const PassRecording &into,
                                                 bool hasSelection);

  [[nodiscard]] Native::Result<void> EncodePyramid(const PassRecording &into) {
    return Pyramid_.Encode(into);
  }

  [[nodiscard]] Native::Buffer *Buffer() const noexcept { return Pyramid_.Buffer(); }

  [[nodiscard]] const PyramidShape &Shape() const noexcept { return Pyramid_.Shape(); }

private:
  DepthPyramidPass Pyramid_;
  std::array<std::unique_ptr<Native::GraphicsPipeline>, 2> Pipelines_;
};
}
#endif
