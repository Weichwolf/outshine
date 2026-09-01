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

namespace outshine::Render {

class SkyStage {
public:
  [[nodiscard]] static std::string ShaderSource();
  [[nodiscard]] static std::string ShaderSource(std::string &error);
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 2, .FragmentUniformBuffers = 1};
  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *skyView,
                               SDL_GPUTexture *transmittance,
                               SDL_GPUSampler *lut,
                               std::string &error);

  void Declare(const Medium &medium,
               const Vec3f &sunDir,
               const Vec3f &up,
               float illuminanceLux,
               float eyeHeightM);

  void Eye(const Medium &medium, float eyeHeightM);

  [[nodiscard]] bool Stands() const { return Declared_; }

  void SetBasis(
      const Vec3f &right, const Vec3f &upAxis, const Vec3f &fwd, float tanHalfW, float tanHalfH);

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
