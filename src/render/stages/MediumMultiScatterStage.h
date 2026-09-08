#ifndef OUTSHINE_RENDER_STAGES_MEDIUMMULTISCATTERSTAGE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMMULTISCATTERSTAGE_H

#include <string>

#include "ComputeShaders.h"
#include "StageSubmission.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumMultiScatterStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::MediumMultiScatter;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;
  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *transmittance,
                               SDL_GPUSampler *lut,
                               SDL_GPUTexture *into,
                               std::string &error);

  void Declare(const Medium &medium);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Transmittance = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  SDL_GPUTexture *Into = nullptr;
  Medium Declared_;
  StageCache Cache_;
};

}
#endif
