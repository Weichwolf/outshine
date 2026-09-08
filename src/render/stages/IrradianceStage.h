#ifndef OUTSHINE_RENDER_STAGES_IRRADIANCESTAGE_H
#define OUTSHINE_RENDER_STAGES_IRRADIANCESTAGE_H

#include <string>

#include "ComputeShaders.h"
#include "StageSubmission.h"
#include "IrradianceLayout.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class IrradianceStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::Irradiance;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *transmittance,
                               SDL_GPUTexture *multiScatter,
                               SDL_GPUSampler *lut,
                               SDL_GPUBuffer *into,
                               std::string &error);

  void Declare(const Medium &medium, float cosSunZenith);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  struct Standing {
    Medium Declared;
    float CosSunZenith = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Standing &) const = default;
  };

  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Transmittance = nullptr;
  SDL_GPUTexture *MultiScatter = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  SDL_GPUBuffer *Into = nullptr;
  Standing Standing_{};
  StageCache Cache_;
};

}
#endif
