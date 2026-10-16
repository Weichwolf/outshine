#ifndef OUTSHINE_RENDER_STAGES_DEPTHPYRAMIDSTAGE_H
#define OUTSHINE_RENDER_STAGES_DEPTHPYRAMIDSTAGE_H

#include <memory>

#include "DepthPyramid.h"
#include "Gpu.h"
#include "ComputeShaders.h"

#include <Extent.h>

namespace outshine::Render {

class DepthPyramidStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::DepthPyramid;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu,
                                               Native::Texture *depth,
                                               Native::Sampler *held,
                                               Native::Buffer *into,
                                               Extent size);

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into);

  [[nodiscard]] bool Stands() const { return Pipe && Into_ != nullptr; }

private:
  std::unique_ptr<Native::ComputePipeline> Pipe;
  Native::Texture *Depth_ = nullptr;
  Native::Sampler *Held_ = nullptr;
  Native::Buffer *Into_ = nullptr;
  PyramidShape Shape_;
  uint32_t Wide_ = 0, High_ = 0;
};

}
#endif
