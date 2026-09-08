#ifndef OUTSHINE_RENDER_STAGES_MEDIUMTRANSMITTANCESTAGE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMTRANSMITTANCESTAGE_H

#include <string>

#include "ComputeShaders.h"
#include "StageSubmission.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumTransmittanceStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::MediumTransmittance;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *lut, std::string &error);

  void Declare(const Medium &medium);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Lut = nullptr;
  Medium Declared_;
  StageCache Cache_;
};

}
#endif
