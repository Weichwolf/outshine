#ifndef OUTSHINE_RENDER_STAGES_MEDIUMRADIANCESTAGE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMRADIANCESTAGE_H

#include <memory>
#include <optional>

#include "math/Vec2.h"
#include "ComputeShaders.h"
#include "StageSubmission.h"

#include "Gpu.h"
#include "Atmosphere.h"

namespace outshine::Render {

class MediumRadianceStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::MediumRadiance;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;
  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu,
                                               Native::Texture *transmittance,
                                               Native::Texture *multiScatter,
                                               Native::Sampler *lut,
                                               Native::Texture *into);

  void Declare(const Medium &medium, float cosSunZenith, float eyeHeightM);

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  struct Standing {
    Medium Declared;
    float CosSunZenith = 2.0f;
    float EyeHeightM = -1.0f;

    [[nodiscard]] constexpr bool operator==(const Standing &) const = default;
  };

  std::unique_ptr<Native::ComputePipeline> Pipe;
  Native::Texture *Transmittance = nullptr;
  Native::Texture *MultiScatter = nullptr;
  Native::Sampler *Lut = nullptr;
  Native::Texture *Into = nullptr;
  std::optional<Standing> Standing_;
  StageCache Cache_;
};

}
#endif
