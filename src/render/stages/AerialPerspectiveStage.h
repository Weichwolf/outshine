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
#include "ParticipatingMedium.h"
#include "SkyPass.h"

namespace outshine::Render {

class AerialPerspectiveStage {
public:
  [[nodiscard]] static std::string ShaderSource();
  [[nodiscard]] static std::string ShaderSource(std::string &error);
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

  void SetNear(float nearM) { Pushed_.NearM = nearM; }

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
    float NearM;
    float Pad0;
    float Pad1;
    float Pad2;
    Medium Air;
  };

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

} // namespace outshine::Render
#endif
