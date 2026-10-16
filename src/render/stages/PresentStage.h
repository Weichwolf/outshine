#ifndef OUTSHINE_RENDER_STAGES_PRESENTSTAGE_H
#define OUTSHINE_RENDER_STAGES_PRESENTSTAGE_H

#include "Gpu.h"
#include <memory>
#include <array>
#include <cstddef>

namespace outshine::Render {

class PresentStage {
public:
  [[nodiscard]] Native::Result<void> Configure(const Native::Texture *frame,
                                               const Native::Sampler *exact);

  [[nodiscard]] Native::Result<void> For(Native::Device &device, Native::Format surfaceFormat);
  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into) const;

private:
  std::array<std::unique_ptr<Native::GraphicsPipeline>, static_cast<size_t>(Native::Format::Count)>
      Pipelines;
  Native::GraphicsPipeline *Pipe = nullptr;
  Native::Device *Device = nullptr;
  const Native::Texture *Frame = nullptr;
  const Native::Sampler *Exact = nullptr;
};

}
#endif
