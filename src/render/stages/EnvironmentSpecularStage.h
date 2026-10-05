#ifndef OUTSHINE_RENDER_STAGES_ENVIRONMENTSPECULARSTAGE_H
#define OUTSHINE_RENDER_STAGES_ENVIRONMENTSPECULARSTAGE_H

#include "Atmosphere.h"
#include "ComputeShaders.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "StageSubmission.h"
#include "math/Vec3.h"
#include <optional>
#include <string>

namespace outshine::Render {
class EnvironmentSpecularStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::EnvironmentSpecular;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  struct Sources {
    SDL_GPUTexture *Sky = nullptr;
    SDL_GPUBuffer *Irradiance = nullptr;
    SDL_GPUSampler *Sampler = nullptr;
    SDL_GPUTexture *Target = nullptr;
  };

  [[nodiscard]] bool Configure(const Gpu &gpu, Sources from, std::string &error);
  void Declare(const Medium &medium, float cosSunZenith, float eyeHeightM, Vec3 groundAlbedo);
  void Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  struct Standing {
    Medium Air;
    float CosSunZenith;
    float EyeHeightM;
    Vec3 GroundAlbedo;
    [[nodiscard]] constexpr bool operator==(const Standing &) const = default;
  };

  OwnedComputePipeline Pipe_;
  Sources Sources_;
  std::optional<Standing> Standing_;
  StageCache Cache_;
};
}

#endif
