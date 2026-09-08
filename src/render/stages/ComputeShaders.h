#ifndef OUTSHINE_RENDER_STAGES_COMPUTESHADERS_H
#define OUTSHINE_RENDER_STAGES_COMPUTESHADERS_H

#include "KernelShape.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace outshine::Render {

enum class ComputeShaderId : uint32_t {
  DepthPyramid,
  Irradiance,
  MediumTransmittance,
  MediumMultiScatter,
  MediumRadiance,
  SubjectCull,
  SubjectScan,
  SubjectCompact,
  Count
};

struct ComputeShaderDescriptor {
  ComputeShaderId Id;
  std::string_view Path;
  ComputeShape Shape;
};

namespace Detail {
inline constexpr std::array kComputeShaders = {
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::DepthPyramid,
        .Path = "build/shaders/depthPyramid.comp.spv",
        .Shape =
            {.Samplers = 1, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::Irradiance,
                            .Path = "build/shaders/irradiance.comp.spv",
                            .Shape = {.Samplers = 2, .ReadWriteBuffers = 1, .UniformBuffers = 1}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::MediumTransmittance,
        .Path = "build/shaders/mediumTransmittance.comp.spv",
        .Shape = {.ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::MediumMultiScatter,
        .Path = "build/shaders/mediumMultiScatter.comp.spv",
        .Shape =
            {.Samplers = 1, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::MediumRadiance,
        .Path = "build/shaders/mediumRadiance.comp.spv",
        .Shape =
            {.Samplers = 2, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::SubjectCull,
        .Path = "build/shaders/subjectCullKernel.comp.spv",
        .Shape = {.ReadOnlyBuffers = 5, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::SubjectScan,
                            .Path = "build/shaders/subjectScanKernel.comp.spv",
                            .Shape = {.ReadOnlyBuffers = 2, .ReadWriteBuffers = 2, .GroupX = 256}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::SubjectCompact,
        .Path = "build/shaders/subjectCompactKernel.comp.spv",
        .Shape = {
            .ReadOnlyBuffers = 4, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 128}}};

[[nodiscard]] constexpr bool ValidComputeShaderCatalog() noexcept {
  if (kComputeShaders.size() != static_cast<std::size_t>(ComputeShaderId::Count)) { return false; }
  for (std::size_t at = 0; at < kComputeShaders.size(); ++at) {
    const auto &shader = kComputeShaders[at];
    if (static_cast<std::size_t>(shader.Id) != at || shader.Path.empty() ||
        shader.Shape.GroupX == 0 || shader.Shape.GroupY == 0 || shader.Shape.GroupZ == 0) {
      return false;
    }
    for (std::size_t other = at + 1; other < kComputeShaders.size(); ++other) {
      if (shader.Path == kComputeShaders[other].Path) { return false; }
    }
  }
  return true;
}

static_assert(ValidComputeShaderCatalog());
}

[[nodiscard]] constexpr std::span<const ComputeShaderDescriptor> ComputeShaders() noexcept {
  return Detail::kComputeShaders;
}

[[nodiscard]] constexpr const ComputeShaderDescriptor *
FindComputeShader(ComputeShaderId id) noexcept {
  const auto index = static_cast<std::size_t>(id);
  return index < Detail::kComputeShaders.size() ? &Detail::kComputeShaders[index] : nullptr;
}

}
#endif
