#ifndef MEDIUMRADIANCESTAGE_H
#define MEDIUMRADIANCESTAGE_H

#include <string>

#include "KernelShape.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumRadianceStage {
public:
  [[nodiscard]] static std::string KernelSource(void);
  [[nodiscard]] static std::string KernelSource(std::string &error);
  static constexpr ComputeShape KernelShape{
      .Samplers = 2, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8};
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *transmittance,
                               SDL_GPUTexture *multiScatter, SDL_GPUSampler *lut,
                               SDL_GPUTexture *into, std::string &error);

  void Declare(const Medium &medium, float cosSunZenith, float eyeHeightM);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled(void) const { return Settled_; }

private:
  struct Standing {
    Medium Declared;
    float CosSunZenith = 2.0f;
    float EyeHeightM = -1.0f;
  };

  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Transmittance = nullptr;
  SDL_GPUTexture *MultiScatter = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  SDL_GPUTexture *Into = nullptr;
  Standing Standing_;
  bool Settled_ = false;
};

}
#endif
