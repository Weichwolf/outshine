#ifndef OUTSHINE_RENDER_STAGES_ENVIRONMENTSPECULARSTAGE_H
#define OUTSHINE_RENDER_STAGES_ENVIRONMENTSPECULARSTAGE_H

#include "Atmosphere.h"
#include "ComputeShaders.h"
#include "Gpu.h"
#include "StageSubmission.h"
#include "math/Vec3.h"
#include <optional>
#include <memory>

namespace outshine::Render {
class EnvironmentSpecularStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::EnvironmentSpecular;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  struct Sources {
    Native::Texture *Sky = nullptr;
    Native::Buffer *Irradiance = nullptr;
    Native::Sampler *Sampler = nullptr;
    Native::Texture *Target = nullptr;
  };

  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu, Sources from);
  void Declare(const Medium &medium, float cosSunZenith, float eyeHeightM, Vec3 groundAlbedo);
  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  struct Standing {
    Medium Air;
    float CosSunZenith;
    float EyeHeightM;
    Vec3 GroundAlbedo;
    [[nodiscard]] constexpr bool operator==(const Standing &) const = default;
  };

  std::unique_ptr<Native::ComputePipeline> Pipe_;
  Sources Sources_;
  std::optional<Standing> Standing_;
  StageCache Cache_;
};
}

#endif
