#ifndef OUTSHINE_RENDER_STAGES_SKYSTAGE_H
#define OUTSHINE_RENDER_STAGES_SKYSTAGE_H

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

class SkyStage {
public:
  [[nodiscard]] static std::string ShaderSource();
  [[nodiscard]] static std::string ShaderSource(std::string &error);
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 2, .FragmentUniformBuffers = 1};

  struct Tables {
    SDL_GPUTexture *SkyView = nullptr;
    SDL_GPUTexture *Transmittance = nullptr;
    SDL_GPUSampler *Lut = nullptr;
  };

  [[nodiscard]] bool Configure(const Gpu &gpu, Tables from, std::string &error);

  void Declare(const Medium &medium, SkyStanding stands);

  void Eye(const Medium &medium, float eyeHeightM);

  [[nodiscard]] bool Stands() const { return Declared_; }

  void SetBasis(const EyeBasis &eye);

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
    float BottomRadiusKm;
    float TopRadiusKm;
    float SunHalfAngleRad;
    float Pad;
    Medium Air;
  };

  OwnedPipeline Pipe;
  SDL_GPUTexture *SkyView = nullptr;
  SDL_GPUTexture *Veil = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  Pushed Pushed_{};
  bool Declared_ = false;
};

} // namespace outshine::Render
#endif
