#ifndef OUTSHINE_RENDER_STAGES_COMPOSITETRANSMISSIONSTAGE_H
#define OUTSHINE_RENDER_STAGES_COMPOSITETRANSMISSIONSTAGE_H

#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include <memory>
#include <cstddef>

namespace outshine::Render {

class CompositeTransmissionStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 2};

  struct Feeds {
    Native::Texture *Opaque = nullptr;
    Native::Texture *Transmissive = nullptr;
    Native::Sampler *Exact = nullptr;
    Native::Format Target = Native::Format::Count;
  };

  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu, const Feeds &from);
  [[nodiscard]] Native::Result<void> Encode(const FrameContext &ctx, const PassRecording &into);

private:
  std::unique_ptr<Native::GraphicsPipeline> Pipe;
  Native::Texture *Opaque = nullptr;
  Native::Texture *Transmissive = nullptr;
  Native::Sampler *Exact = nullptr;
};

}
#endif
