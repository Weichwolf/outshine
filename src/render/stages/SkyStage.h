#ifndef SKYSTAGE_H
#define SKYSTAGE_H

#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "ParticipatingMedium.h"

namespace outshine::Render {

class SkyStage {
public:
  [[nodiscard]] static std::string ShaderSource(void);
  [[nodiscard]] static std::string ShaderSource(std::string &error);
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 1, .FragmentUniformBuffers = 1};
  [[nodiscard]] bool Configure(const Gpu &gpu, SDL_GPUTexture *skyView, SDL_GPUSampler *lut,
                               std::string &error);

  void Declare(const Medium &medium, const float sunDir[3], const float up[3],
               float illuminanceLux, float eyeHeightM);

  void SetBasis(const float right[3], const float upAxis[3], const float fwd[3], float tanHalfW,
                float tanHalfH);

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
    float BottomRadiusKm;
    float TopRadiusKm;
    float Pad[2];
  };

  OwnedPipeline Pipe;
  SDL_GPUTexture *SkyView = nullptr;
  SDL_GPUSampler *Lut = nullptr;
  Pushed Pushed_{};
  bool Declared_ = false;
};

}
#endif
