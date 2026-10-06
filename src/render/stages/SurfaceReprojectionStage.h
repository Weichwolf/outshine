#ifndef OUTSHINE_RENDER_STAGES_SURFACEREPROJECTIONSTAGE_H
#define OUTSHINE_RENDER_STAGES_SURFACEREPROJECTIONSTAGE_H

#include "Gpu.h"
#include "GpuOwned.h"
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
    std::array<SDL_GPUTexture *, 5> DepthNormalIdentityBaseMetalRough{};
  };

  [[nodiscard]] std::expected<void, std::string>
  Configure(const Gpu &gpu, Capture source, SDL_GPUSampler *sampler);
  [[nodiscard]] std::expected<void, std::string>
  Encode(const CameraBasis &camera, const Lens &projection, const PassRecording &into) const;

private:
  OwnedPipeline Pipe_;
  Capture Source_;
  SDL_GPUSampler *Sampler_ = nullptr;
};

}
#endif
