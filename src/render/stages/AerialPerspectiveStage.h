#ifndef OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H
#define OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H

#include "Vec3.h"
#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class AerialPerspectiveStage {
public:
  [[nodiscard]] static std::string ShaderSource();
  [[nodiscard]] static std::string ShaderSource(std::string &error);
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 4, .FragmentUniformBuffers = 1};

  [[nodiscard]] bool Configure(const Gpu &gpu,
                               SDL_GPUTexture *scene,
                               SDL_GPUTexture *depth,
                               SDL_GPUTexture *skyView,
                               SDL_GPUTexture *transmittance,
                               SDL_GPUSampler *exact,
                               SDL_GPUSampler *lut,
                               SDL_GPUTextureFormat targetFormat,
                               std::string &error);

  void Declare(const Medium &medium,
               const float sunDir[3],
               const float up[3],
               float illuminanceLux,
               float eyeHeightM);

  void Eye(const Medium &medium, float eyeHeightM);

  void SetBasis(
      const Vec3f &right, const Vec3f &upAxis, const Vec3f &fwd, float tanHalfW, float tanHalfH);

  void SetNear(float nearM) { Pushed_.NearM = nearM; }

  [[nodiscard]] bool Stands() const { return Declared_; }

  void Encode(const FrameContext &ctx, const PassRecording &into);

private:
  struct Pushed {
    float Right[4];
    float Up[4];
    float Fwd[4];
    float WorldUp[4];
    float SunDir[4];
    float TanHalf[2];
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
