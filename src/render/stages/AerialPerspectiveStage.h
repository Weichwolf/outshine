#ifndef OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H
#define OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H

#include "math/Vec4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Atmosphere.h"
#include "SkyPass.h"
#include "CloudViewUniform.h"

#include <cstddef>

namespace outshine::Render {

inline constexpr size_t kAerialClearUniformBytes = 192;

class AerialPerspectiveStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 4, .FragmentUniformBuffers = 1};

  struct Tables {
    SDL_GPUTexture *Scene = nullptr;
    SDL_GPUTexture *Depth = nullptr;
    SDL_GPUTexture *SkyView = nullptr;
    SDL_GPUTexture *Transmittance = nullptr;
    SDL_GPUSampler *Exact = nullptr;
    SDL_GPUSampler *Lut = nullptr;
  };

  [[nodiscard]] bool
  Configure(const Gpu &gpu, Tables from, SDL_GPUTextureFormat targetFormat, std::string &error);

  void Declare(const Medium &medium, SkyStanding stands);

  void Eye(const Medium &medium, float eyeHeightM);

  void SetBasis(const EyeBasis &eye);

  void SetDepthReconstruction(Vec4f coefficients) { Pushed_.DepthReconstruction = coefficients; }

  void Clouds(CloudViewUniform clouds) { Pushed_.Clouds = clouds; }

  [[nodiscard]] bool Stands() const { return Declared_; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  struct Pushed {
    Vec4f Right;
    Vec4f Up;
    Vec4f Fwd;
    Vec4f WorldUp;
    Vec4f SunDir;
    Vec2f TanHalf;
    float Illuminance;
    float EyeRadiusKm;
    Vec4f DepthReconstruction;
    Medium Air;
    CloudViewUniform Clouds;
  };

  static_assert(sizeof(Pushed) == kAerialClearUniformBytes + sizeof(CloudViewUniform));
  static_assert(alignof(Pushed) == 16);
  static_assert(offsetof(Pushed, Air) == 7 * sizeof(Vec4f));
  static_assert(offsetof(Pushed, Clouds) == kAerialClearUniformBytes);

  OwnedPipeline Pipe;
  SDL_GPUTexture *Scene = nullptr;
  SDL_GPUTexture *Depth = nullptr;
  SDL_GPUTexture *SkyView = nullptr;
  SDL_GPUTexture *Veil = nullptr;
  SDL_GPUSampler *Exact = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  Pushed Pushed_{};
  bool Declared_ = false;
};

}
#endif
