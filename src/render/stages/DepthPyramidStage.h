#ifndef OUTSHINE_RENDER_STAGES_DEPTHPYRAMIDSTAGE_H
#define OUTSHINE_RENDER_STAGES_DEPTHPYRAMIDSTAGE_H

#include <string>

#include "DepthPyramid.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "ComputeShaders.h"

#include <Extent.h>

namespace outshine::Render {

class DepthPyramidStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::DepthPyramid;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *depth,
                               SDL_GPUSampler *held,
                               SDL_GPUBuffer *into,
                               Extent size,
                               std::string &error);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Stands() const { return Pipe && Into_ != nullptr; }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Depth_ = nullptr;
  SDL_GPUSampler *Held_ = nullptr;
  SDL_GPUBuffer *Into_ = nullptr;
  PyramidShape Shape_;
  uint32_t Wide_ = 0, High_ = 0;
};

}
#endif
