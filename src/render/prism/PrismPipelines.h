#ifndef OUTSHINE_RENDER_PRISM_PRISMPIPELINES_H
#define OUTSHINE_RENDER_PRISM_PRISMPIPELINES_H

#include "SurfaceOutputs.h"
#include "device/native/Device.h"
#include <memory>
#include <span>

namespace outshine::Render {
class PrismPipelines {
public:
  [[nodiscard]] Native::Result<void> ConfigureColour(Native::Device &device,
                                                     const SurfaceOutputs &outputs,
                                                     std::span<const Native::ColourTarget> targets);
  [[nodiscard]] Native::Result<void> ConfigureDepth(Native::Device &device);

  [[nodiscard]] Native::GraphicsPipeline *Colour() const noexcept { return Colour_.get(); }

  [[nodiscard]] Native::GraphicsPipeline *Depth() const noexcept { return Depth_.get(); }

private:
  std::unique_ptr<Native::GraphicsPipeline> Colour_;
  std::unique_ptr<Native::GraphicsPipeline> Depth_;
};
}
#endif
