#ifndef OUTSHINE_RENDER_STAGES_SKYSTAGE_H
#define OUTSHINE_RENDER_STAGES_SKYSTAGE_H

#include "math/Vec4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include <string>

#include "KernelShape.h"

#include "FrameContext.h"
#include "Gpu.h"
#include <memory>
#include <cstddef>
#include "Atmosphere.h"
#include "SkyPass.h"

namespace outshine::Render {

class SkyStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 2, .FragmentUniformBuffers = 1};

  struct Tables {
    Native::Texture *SkyView = nullptr;
    Native::Texture *Transmittance = nullptr;
    Native::Sampler *Lut = nullptr;
  };

  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu, Tables from);

  void Declare(const Medium &medium, SkyStanding stands);

  void Eye(const Medium &medium, float eyeHeightM);

  [[nodiscard]] bool Stands() const { return Declared_; }

  void SetBasis(const EyeBasis &eye);

  [[nodiscard]] Native::Result<void> Encode(const FrameContext &ctx, const PassRecording &into);

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

  static constexpr size_t kUniformBytes = 192;
  static constexpr size_t kMediumOffset = 112;
  static_assert(sizeof(Pushed) == kUniformBytes && alignof(Pushed) == 16);
  static_assert(offsetof(Pushed, Air) == kMediumOffset);
  std::unique_ptr<Native::GraphicsPipeline> Pipe;
  Native::Texture *SkyView = nullptr;
  Native::Texture *Veil = nullptr;
  Native::Sampler *Lut = nullptr;
  Pushed Pushed_{};
  bool Declared_ = false;
};

}
#endif
