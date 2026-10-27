#ifndef OUTSHINE_RENDER_PIPELINES_COMPUTESHADERS_H
#define OUTSHINE_RENDER_PIPELINES_COMPUTESHADERS_H

#include "PipelineShape.h"
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
  EnvironmentSpecular,
  GeometryCull,
  GeometryScan,
  GeometryCompact,
  TerrainPatchCull,
  TerrainPatchScan,
  TerrainPatchCompact,
  CalibrationFma,
  CalibrationTriad,
  GeometryRootCull,
  GeometryLeafCull,
  GeometryRootScan,
  GeometrySelectedCompact,
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
        .Path = "depthPyramid.comp.spv",
        .Shape =
            {.Samplers = 1, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::Irradiance,
                            .Path = "irradiance.comp.spv",
                            .Shape = {.Samplers = 2, .ReadWriteBuffers = 1, .UniformBuffers = 1}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::MediumTransmittance,
        .Path = "mediumTransmittance.comp.spv",
        .Shape = {.ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::MediumMultiScatter,
        .Path = "mediumMultiScatter.comp.spv",
        .Shape =
            {.Samplers = 1, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::MediumRadiance,
        .Path = "mediumRadiance.comp.spv",
        .Shape =
            {.Samplers = 2, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::EnvironmentSpecular,
                            .Path = "environmentSpecular.comp.spv",
                            .Shape = {.Samplers = 1,
                                      .ReadWriteTextures = 1,
                                      .ReadOnlyBuffers = 1,
                                      .UniformBuffers = 1,
                                      .GroupX = 8,
                                      .GroupY = 8}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::GeometryCull,
        .Path = "geometryCull.comp.spv",
        .Shape = {.ReadOnlyBuffers = 5, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::GeometryScan,
                            .Path = "geometryScan.comp.spv",
                            .Shape = {.ReadOnlyBuffers = 2, .ReadWriteBuffers = 2, .GroupX = 32}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::GeometryCompact,
        .Path = "geometryCompact.comp.spv",
        .Shape = {.ReadOnlyBuffers = 4, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::TerrainPatchCull,
        .Path = "groundPatchCull.comp.spv",
        .Shape = {.ReadOnlyBuffers = 2, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 64}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::TerrainPatchScan,
                            .Path = "groundPatchScan.comp.spv",
                            .Shape = {.ReadOnlyBuffers = 2, .ReadWriteBuffers = 2, .GroupX = 256}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::TerrainPatchCompact,
        .Path = "groundPatchCompact.comp.spv",
        .Shape = {.ReadOnlyBuffers = 3, .ReadWriteBuffers = 1, .UniformBuffers = 1, .GroupX = 128}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::CalibrationFma,
                            .Path = "calibrationFma.comp.spv",
                            .Shape = {.ReadOnlyBuffers = 1, .ReadWriteBuffers = 1, .GroupX = 256}},
    ComputeShaderDescriptor{.Id = ComputeShaderId::CalibrationTriad,
                            .Path = "calibrationTriad.comp.spv",
                            .Shape = {.ReadOnlyBuffers = 2, .ReadWriteBuffers = 1, .GroupX = 256}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::GeometryRootCull,
        .Path = "geometryRootCull.comp.spv",
        .Shape = {.ReadOnlyBuffers = 2, .ReadWriteBuffers = 3, .UniformBuffers = 2, .GroupX = 64}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::GeometryLeafCull,
        .Path = "geometryLeafCull.comp.spv",
        .Shape = {.ReadOnlyBuffers = 8, .ReadWriteBuffers = 3, .UniformBuffers = 2, .GroupX = 64}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::GeometryRootScan,
        .Path = "geometryRootScan.comp.spv",
        .Shape = {.ReadOnlyBuffers = 3, .ReadWriteBuffers = 2, .UniformBuffers = 1, .GroupX = 32}},
    ComputeShaderDescriptor{
        .Id = ComputeShaderId::GeometrySelectedCompact,
        .Path = "geometrySelectedCompact.comp.spv",
        .Shape = {.ReadOnlyBuffers = 8, .ReadWriteBuffers = 1, .UniformBuffers = 2, .GroupX = 64}}};

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
