#ifndef OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H
#define OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H

#include "MediumViewParameters.h"
#include "KernelShape.h"
#include "Gpu.h"
#include <memory>

namespace outshine::Render {

class AerialPerspectiveStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 4, .FragmentUniformBuffers = 1};

  struct Tables {
    Native::Texture *Scene = nullptr;
    Native::Texture *Depth = nullptr;
    Native::Texture *SkyView = nullptr;
    Native::Texture *Transmittance = nullptr;
    Native::Sampler *Exact = nullptr;
    Native::Sampler *Lut = nullptr;
  };

  [[nodiscard]] Native::Result<void>
  Configure(const Gpu &gpu, Tables from, Native::Format targetFormat);

  [[nodiscard]] Native::Result<void> Encode(const MediumViewParameters &view,
                                            const PassRecording &into);

private:
  std::unique_ptr<Native::GraphicsPipeline> Pipe;
  Native::Texture *Scene = nullptr;
  Native::Texture *Depth = nullptr;
  Native::Texture *SkyView = nullptr;
  Native::Texture *Veil = nullptr;
  Native::Sampler *Exact = nullptr;
  Native::Sampler *Lut = nullptr;
};

}
#endif
