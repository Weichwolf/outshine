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
  [[nodiscard]] static std::string KernelSource();
  [[nodiscard]] static std::string KernelSource(std::string &error);
  static constexpr ComputeShape KernelShape{
      .Samplers = 2, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8};
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *transmittance,
                               SDL_GPUTexture *multiScatter, SDL_GPUSampler *lut,
                               SDL_GPUTexture *into, std::string &error);

  void Declare(const Medium &medium, float cosSunZenith, float eyeHeightM);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const { return Settled_; }

private:
  struct Standing {
    Medium Declared;
    float CosSunZenith = 2.0f;
    float EyeHeightM = -1.0f;
    // explicit tail: the settled-check memcmps this struct, and a padding byte the
    // compiler owns would turn the comparison into a hope
    float Pad[2] = {0.0f, 0.0f};
  };
  static_assert(sizeof(Standing) == sizeof(Medium) + 4 * sizeof(float),
                "every byte of the settled comparison is a member, none is padding");

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
