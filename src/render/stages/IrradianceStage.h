#ifndef OUTSHINE_RENDER_STAGES_IRRADIANCESTAGE_H
#define OUTSHINE_RENDER_STAGES_IRRADIANCESTAGE_H

#include <string>

#include "KernelShape.h"

#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

inline constexpr uint32_t kIrradianceFloats = 6u;

class IrradianceStage {
public:
  [[nodiscard]] static std::string KernelSource();
  [[nodiscard]] static std::string KernelSource(std::string &error);
  static constexpr ComputeShape KernelShape{
      .Samplers = 2, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 1};

  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *transmittance,
                               SDL_GPUTexture *multiScatter,
                               SDL_GPUSampler *lut,
                               SDL_GPUBuffer *into,
                               std::string &error);

  void Declare(const Medium &medium, float cosSunZenith);

  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const { return Settled_; }

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
  bool Settled_ = false;
};

} // namespace outshine::Render
#endif
