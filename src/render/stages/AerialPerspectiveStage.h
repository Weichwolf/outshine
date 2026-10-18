#ifndef OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H
#define OUTSHINE_RENDER_STAGES_AERIALPERSPECTIVESTAGE_H

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

class AerialPerspectiveStage {
public:
  static constexpr DrawShape ShaderShape{.FragmentSamplers = 4, .FragmentUniformBuffers = 1};

  struct Tables {
    Native::Texture *Scene = nullptr;
    Native::Texture *Depth = nullptr;
    Native::Texture *SkyView = nullptr;
    Native::Texture *Transmittance = nullptr;
    Native::Sampler *Exact = nullptr;
    Native::Sampler *Lut = nullptr;
  };

  [[nodiscard]] Native::Result<void>
  Configure(const Gpu &gpu, Tables from, Native::Format targetFormat);

  void Declare(const Medium &medium, SkyStanding stands);

  void Eye(const Medium &medium, float eyeHeightM);

  void SetBasis(const EyeBasis &eye);

  void SetDepthReconstruction(Vec4f coefficients) { Pushed_.DepthReconstruction = coefficients; }

  [[nodiscard]] bool Stands() const { return Declared_; }

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
    Vec4f DepthReconstruction;
    Medium Air;
  };

  static constexpr size_t kUniformBytes = 192;
  static constexpr size_t kMediumOffset = 112;
  static_assert(sizeof(Pushed) == kUniformBytes && alignof(Pushed) == 16);
  static_assert(offsetof(Pushed, Air) == kMediumOffset);
  std::unique_ptr<Native::GraphicsPipeline> Pipe;
  Native::Texture *Scene = nullptr;
  Native::Texture *Depth = nullptr;
  Native::Texture *SkyView = nullptr;
  Native::Texture *Veil = nullptr;
  Native::Sampler *Exact = nullptr;
  Native::Sampler *Lut = nullptr;
  Pushed Pushed_{};
  bool Declared_ = false;
};

}
#endif
