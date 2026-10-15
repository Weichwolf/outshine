#ifndef OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H
#define OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H

#include "math/Vec4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <string>
#include <cstddef>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Atmosphere.h"
#include "SkyPass.h"

namespace outshine::Render {

class AerialPerspectiveStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 4, .FragmentUniformBuffers = 1};

  struct Tables {
    SDL_GPUTexture *Scene = nullptr;
    SDL_GPUTexture *Depth = nullptr;
    SDL_GPUTexture *MultiScatter = nullptr;
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
  };

  static_assert(sizeof(Pushed) == 7 * sizeof(Vec4f) + sizeof(Medium) && alignof(Pushed) == 16);
  static_assert(offsetof(Pushed, Right) == 0 && offsetof(Pushed, Up) == sizeof(Vec4f) &&
                offsetof(Pushed, Fwd) == 2 * sizeof(Vec4f) &&
                offsetof(Pushed, WorldUp) == 3 * sizeof(Vec4f) &&
                offsetof(Pushed, SunDir) == 4 * sizeof(Vec4f) &&
                offsetof(Pushed, TanHalf) == 5 * sizeof(Vec4f) &&
                offsetof(Pushed, Illuminance) == 5 * sizeof(Vec4f) + sizeof(Vec2f) &&
                offsetof(Pushed, EyeRadiusKm) ==
                    5 * sizeof(Vec4f) + sizeof(Vec2f) + sizeof(float) &&
                offsetof(Pushed, DepthReconstruction) == 6 * sizeof(Vec4f) &&
                offsetof(Pushed, Air) == 7 * sizeof(Vec4f));

  OwnedPipeline Pipe;
  SDL_GPUTexture *Scene = nullptr;
  SDL_GPUTexture *Depth = nullptr;
  SDL_GPUTexture *MultiScatter = nullptr;
  SDL_GPUTexture *Veil = nullptr;
  SDL_GPUSampler *Exact = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  Pushed Pushed_{};
  bool Declared_ = false;
};

}
#endif
