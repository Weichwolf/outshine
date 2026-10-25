#ifndef OUTSHINE_RENDER_ATMOSPHERE_ATMOSPHEREPASSES_H
#define OUTSHINE_RENDER_ATMOSPHERE_ATMOSPHEREPASSES_H

#include "Device.h"
#include "ExecutionPlan.h"
#include "Gpu.h"
#include "Vec3.h"
#include "stages/EnvironmentSpecularStage.h"
#include "stages/IrradianceStage.h"
#include "stages/MediumMultiScatterStage.h"
#include "stages/MediumRadianceStage.h"
#include "stages/MediumTransmittanceStage.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace outshine::Render {

struct AtmosphereDeclaration {
  Medium Air = kEarthAir;
  float CosSunZenith = 1;
  float EyeHeightM = 0;
  Vec3 GroundAlbedo;
};

class AtmospherePasses {
public:
  struct AllocationCounts {
    size_t Textures = 0;
    size_t Buffers = 0;
    size_t Samplers = 0;
  };

  [[nodiscard]] static bool Owns(Resource resource) noexcept;
  // The frame owner retires GPU users before replacing a published resource.
  [[nodiscard]] Native::Result<void>
  Create(Native::Device &device, const ExecutionPlan &plan, Resource resource);
  [[nodiscard]] Native::Result<void> Configure(Stage stage, const Gpu &gpu);
  void Declare(const AtmosphereDeclaration &declaration);
  [[nodiscard]] Native::Result<void> Encode(Stage stage, const PassRecording &recording);
  [[nodiscard]] bool Settled(Stage stage) const noexcept;
  [[nodiscard]] bool Has(Resource resource) const noexcept;
  [[nodiscard]] Native::Texture *Texture(Resource resource) const noexcept;

  [[nodiscard]] Native::Sampler *Sampler() const noexcept { return Sampler_.get(); }

  [[nodiscard]] Native::Buffer *Irradiance() const noexcept { return Irradiance_.get(); }

  [[nodiscard]] AllocationCounts Allocations() const noexcept;

private:
  enum class Lookup : uint8_t { Transmittance, MultiScatter, SkyView, Environment, Count };
  [[nodiscard]] static Lookup LookupFor(Resource resource) noexcept;

  std::array<std::unique_ptr<Native::Texture>, static_cast<size_t>(Lookup::Count)> Textures_;
  std::unique_ptr<Native::Sampler> Sampler_;
  std::unique_ptr<Native::Buffer> Irradiance_;
  MediumTransmittanceStage Transmittance_;
  MediumMultiScatterStage MultiScatter_;
  MediumRadianceStage Radiance_;
  IrradianceStage SkyIrradiance_;
  EnvironmentSpecularStage Environment_;
};
}
#endif
