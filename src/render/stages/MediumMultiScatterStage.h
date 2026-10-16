#ifndef OUTSHINE_RENDER_STAGES_MEDIUMMULTISCATTERSTAGE_H
#define OUTSHINE_RENDER_STAGES_MEDIUMMULTISCATTERSTAGE_H

#include <memory>
#include <optional>

#include "ComputeShaders.h"
#include "StageSubmission.h"

#include "Gpu.h"
#include "Atmosphere.h"

namespace outshine::Render {

class MediumMultiScatterStage {
public:
  static constexpr ComputeShaderId Shader = ComputeShaderId::MediumMultiScatter;
  static constexpr ComputeShape KernelShape = FindComputeShader(Shader)->Shape;
  [[nodiscard]] Native::Result<void> Configure(const Gpu &gpu,
                                               Native::Texture *transmittance,
                                               Native::Sampler *lut,
                                               Native::Texture *into);

  void Declare(const Medium &medium);

  [[nodiscard]] Native::Result<void> Encode(const PassRecording &into);

  [[nodiscard]] bool Settled() const noexcept { return Cache_.Submitted(); }

private:
  std::unique_ptr<Native::ComputePipeline> Pipe;
  Native::Texture *Transmittance = nullptr;
  Native::Sampler *Lut = nullptr;
  Native::Texture *Into = nullptr;
  std::optional<Medium> Declared_;
  StageCache Cache_;
};

}
#endif
