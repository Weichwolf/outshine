#ifndef MEDIUMMULTISCATTERSTAGE_H
#define MEDIUMMULTISCATTERSTAGE_H

#include <string>

#include "KernelShape.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class MediumMultiScatterStage {
public:
  [[nodiscard]] static std::string KernelSource(void);
  [[nodiscard]] static std::string KernelSource(std::string &error);
  static constexpr ComputeShape KernelShape{
      .Samplers = 1, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8};
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *transmittance, SDL_GPUSampler *lut,
                               SDL_GPUTexture *into, std::string &error);

  void Declare(const Medium &medium);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled(void) const { return Settled_; }

private:
  OwnedComputePipeline Pipe;
  SDL_GPUTexture *Transmittance = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  SDL_GPUTexture *Into = nullptr;
  Medium Declared_;
  bool Settled_ = false;
};

}
#endif
