#ifndef OUTSHINE_RENDER_STAGES_SURFACEREPROJECTIONSTAGE_H
#define OUTSHINE_RENDER_STAGES_SURFACEREPROJECTIONSTAGE_H

#include "Gpu.h"
#include <memory>
#include "KernelShape.h"
#include "Lens.h"
#include "Viewing.h"

#include <array>
#include <expected>
#include <string>

namespace outshine::Render {

class SurfaceReprojectionStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 5, .FragmentUniformBuffers = 1};

  struct Capture {
    CameraBasis Camera;
    Lens Projection;
    std::array<Native::Texture *, 5> DepthNormalIdentityBaseMetalRough{};
  };

  [[nodiscard]] Native::Result<void>
  Configure(const Gpu &gpu, Capture source, Native::Sampler *sampler);
  [[nodiscard]] Native::Result<void>
  Encode(const CameraBasis &camera, const Lens &projection, const PassRecording &into) const;

private:
  std::unique_ptr<Native::GraphicsPipeline> Pipe_;
  Capture Source_;
  Native::Sampler *Sampler_ = nullptr;
};

}
#endif
