#include "AtmospherePasses.h"
#include "EnvironmentSpecularLayout.h"
#include "IrradianceLayout.h"
#include "TextureFormat.h"
#include <cstddef>
#include <expected>
#include <utility>

namespace outshine::Render {

AtmospherePasses::Lookup AtmospherePasses::LookupFor(Resource resource) noexcept {
  switch (resource) {
    case Resource::TransmittanceLut: return Lookup::Transmittance;
    case Resource::MultiScatterLut: return Lookup::MultiScatter;
    case Resource::SkyViewLut: return Lookup::SkyView;
    case Resource::EnvironmentSpecular: return Lookup::Environment;
    default: return Lookup::Count;
  }
}

bool AtmospherePasses::Owns(Resource resource) noexcept {
  return LookupFor(resource) != Lookup::Count || resource == Resource::LutSampler ||
         resource == Resource::IrradianceBuffer;
}

Native::Texture *AtmospherePasses::Texture(Resource resource) const noexcept {
  const auto lookup = LookupFor(resource);
  return lookup == Lookup::Count ? nullptr : Textures_[static_cast<size_t>(lookup)].get();
}

bool AtmospherePasses::Has(Resource resource) const noexcept {
  if (resource == Resource::LutSampler) { return Sampler_ != nullptr; }
  if (resource == Resource::IrradianceBuffer) { return Irradiance_ != nullptr; }
  return Texture(resource) != nullptr;
}

AtmospherePasses::AllocationCounts AtmospherePasses::Allocations() const noexcept {
  AllocationCounts counts;
  for (const auto &texture : Textures_) {
    if (texture) { ++counts.Textures; }
  }
  counts.Buffers = Irradiance_ ? 1u : 0u;
  counts.Samplers = Sampler_ ? 1u : 0u;
  return counts;
}

Native::Result<void>
AtmospherePasses::Create(Native::Device &device, const ExecutionPlan &plan, Resource resource) {
  if (resource == Resource::LutSampler) {
    auto made = device.CreateSampler({.Min = Native::Filter::Linear,
                                      .Mag = Native::Filter::Linear,
                                      .Mip = Native::Filter::Nearest});
    if (!made) { return std::unexpected(made.error()); }
    if (!*made) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                           .Message = "atmosphere sampler allocation is empty"});
    }
    Sampler_ = std::move(*made);
    return {};
  }
  if (resource == Resource::IrradianceBuffer) {
    auto made =
        device.CreateBuffer({.Bytes = kIrradianceFloats * sizeof(float),
                             .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource});
    if (!made) { return std::unexpected(made.error()); }
    if (!*made) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                           .Message = "atmosphere irradiance allocation is empty"});
    }
    Irradiance_ = std::move(*made);
    return {};
  }
  Native::TextureDescription shape;
  switch (resource) {
    case Resource::TransmittanceLut:
      shape.Width = kTransmittanceLutWidth;
      shape.Height = kTransmittanceLutHeight;
      break;
    case Resource::MultiScatterLut: shape.Width = shape.Height = kMultiScatterLutSize; break;
    case Resource::SkyViewLut:
      shape.Width = kSkyViewLutWidth;
      shape.Height = kSkyViewLutHeight;
      break;
    case Resource::EnvironmentSpecular:
      shape.Width = kEnvironmentWidth;
      shape.Height = kEnvironmentHeight;
      break;
    default:
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "resource has no atmosphere owner"});
  }
  shape.Texels = TextureFormatOf(plan.Format(resource));
  shape.Use =
      Native::TextureUse::Storage | Native::TextureUse::Sampled | Native::TextureUse::CopySource;
  auto made = device.CreateTexture(shape);
  if (!made) { return std::unexpected(made.error()); }
  if (!*made) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                         .Message = "atmosphere lookup allocation is empty"});
  }
  Textures_[static_cast<size_t>(LookupFor(resource))] = std::move(*made);
  return {};
}

Native::Result<void> AtmospherePasses::Configure(Stage stage, const Gpu &gpu) {
  switch (stage) {
    case Stage::MediumTransmittance:
      return Transmittance_.Configure(gpu, Texture(Resource::TransmittanceLut));
    case Stage::MediumMultiScatter:
      return MultiScatter_.Configure(
          gpu, Texture(Resource::TransmittanceLut), Sampler(), Texture(Resource::MultiScatterLut));
    case Stage::MediumRadiance:
      return Radiance_.Configure(gpu,
                                 Texture(Resource::TransmittanceLut),
                                 Texture(Resource::MultiScatterLut),
                                 Sampler(),
                                 Texture(Resource::SkyViewLut));
    case Stage::Irradiance:
      return SkyIrradiance_.Configure(gpu,
                                      Texture(Resource::TransmittanceLut),
                                      Texture(Resource::MultiScatterLut),
                                      Sampler(),
                                      Irradiance());
    case Stage::EnvironmentSpecular:
      return Environment_.Configure(gpu,
                                    {.Sky = Texture(Resource::SkyViewLut),
                                     .Irradiance = Irradiance(),
                                     .Sampler = Sampler(),
                                     .Target = Texture(Resource::EnvironmentSpecular)});
    default:
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "stage has no atmosphere owner"});
  }
}

void AtmospherePasses::Declare(const AtmosphereDeclaration &declaration) {
  Transmittance_.Declare(declaration.Air);
  MultiScatter_.Declare(declaration.Air);
  Radiance_.Declare(declaration.Air, declaration.CosSunZenith, declaration.EyeHeightM);
  SkyIrradiance_.Declare(declaration.Air, declaration.CosSunZenith);
  Environment_.Declare(
      declaration.Air, declaration.CosSunZenith, declaration.EyeHeightM, declaration.GroundAlbedo);
}

Native::Result<void> AtmospherePasses::Encode(Stage stage, const PassRecording &recording) {
  switch (stage) {
    case Stage::MediumTransmittance: return Transmittance_.Encode(recording);
    case Stage::MediumMultiScatter: return MultiScatter_.Encode(recording);
    case Stage::MediumRadiance: return Radiance_.Encode(recording);
    case Stage::Irradiance: return SkyIrradiance_.Encode(recording);
    case Stage::EnvironmentSpecular: return Environment_.Encode(recording);
    default:
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "stage has no atmosphere owner"});
  }
}

bool AtmospherePasses::Settled(Stage stage) const noexcept {
  switch (stage) {
    case Stage::MediumTransmittance: return Transmittance_.Settled();
    case Stage::MediumMultiScatter: return MultiScatter_.Settled();
    case Stage::MediumRadiance: return Radiance_.Settled();
    case Stage::Irradiance: return SkyIrradiance_.Settled();
    case Stage::EnvironmentSpecular: return Environment_.Settled();
    default: return false;
  }
}
}
