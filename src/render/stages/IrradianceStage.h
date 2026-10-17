#ifndef OUTSHINE_RENDER_STAGES_IRRADIANCESTAGE_H
#define OUTSHINE_RENDER_STAGES_IRRADIANCESTAGE_H

#include <memory>
#include <optional>

#include "ComputeShaders.h"
#include "StageSubmission.h"
#include "IrradianceLayout.h"

#include "Gpu.h"
#include "Atmosphere.h"

namespace outshine::Render {

class IrradianceStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::Irradiance;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;

  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu,
                                               Native::Texture *transmittance,
                                               Native::Texture *multiScatter,
                                               Native::Sampler *lut,
                                               Native::Buffer *into);

  void Declare(const Medium &medium, float cosSunZenith);

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  struct Standing {
    Medium Declared;
    float CosSunZenith = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Standing &) const = default;
  };

  std::unique_ptr<Native::ComputePipeline> Pipe;
  Native::Texture *Transmittance = nullptr;
  Native::Texture *MultiScatter = nullptr;
  Native::Sampler *Lut = nullptr;
  Native::Buffer *Into = nullptr;
  std::optional<Standing> Standing_;
  StageCache Cache_;
};

}
#endif
