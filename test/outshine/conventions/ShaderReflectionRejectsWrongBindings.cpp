#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <array>
#include <cstdint>
#include <string>
#include "Check.h"
#include "KernelShape.h"
#include "ShaderFile.h"

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  CHECK(SDL_ShaderCross_Init(), "shader translator initializes");
  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_ShaderCross_GetSPIRVShaderFormats(), true, nullptr);
  CHECK(device != nullptr, "a real GPU device is available");
  if (device == nullptr) {
    SDL_ShaderCross_Quit();
    SDL_Quit();
    return Report();
  }
  std::string error;
  DrawShape draw{};
  draw.FragmentSamplers = 1;
  for (const auto stage : {SDL_GPU_SHADERSTAGE_VERTEX, SDL_GPU_SHADERSTAGE_FRAGMENT}) {
    const char *path = stage == SDL_GPU_SHADERSTAGE_VERTEX ? "build/shaders/fullscreen.vert.spv"
                                                           : "build/shaders/overlay.frag.spv";
    auto *shader = ShaderFrom(device, path, stage, draw, error);
    CHECK(shader != nullptr, "known graphics declaration matches the real SPIR-V");
    SDL_ReleaseGPUShader(device, shader);
    const std::array fields = stage == SDL_GPU_SHADERSTAGE_VERTEX
                                  ? std::array{&DrawShape::VertexSamplers,
                                               &DrawShape::VertexUniformBuffers,
                                               &DrawShape::VertexStorageBuffers}
                                  : std::array{&DrawShape::FragmentSamplers,
                                               &DrawShape::FragmentUniformBuffers,
                                               &DrawShape::FragmentStorageBuffers};
    for (auto field : fields) {
      auto wrong = draw;
      ++(wrong.*field);
      error.clear();
      shader = ShaderFrom(device, path, stage, wrong, error);
      CHECK(shader == nullptr && error.find("declared graphics bindings") != std::string::npos,
            "each incorrect graphics binding count is refused before pipeline creation");
      SDL_ReleaseGPUShader(device, shader);
    }
  }
  for (const auto &descriptor : ComputeShaders()) {
    auto pipeline = CreateComputePipeline(device, descriptor.Id);
    CHECK(pipeline.has_value() && static_cast<bool>(*pipeline),
          "every builtin compute ID creates its declared real pipeline with an owned handle");
  }
  for (const auto id : {ComputeShaderId::Count, static_cast<ComputeShaderId>(~uint32_t{0})}) {
    const auto rejected = CreateComputePipeline(nullptr, id);
    CHECK(!rejected && rejected.error().find("unknown built-in") != std::string::npos,
          "invalid builtin IDs are rejected before touching the device or reading an artifact");
  }
  const auto noDevice = CreateComputePipeline(nullptr, ComputeShaderId::DepthPyramid);
  CHECK(!noDevice && noDevice.error().find("GPU device") != std::string::npos,
        "missing device is rejected without publishing an empty owned resource");
  ComputeShape compute{};
  compute.Samplers = 1;
  compute.ReadWriteBuffers = 1;
  compute.UniformBuffers = 1;
  compute.GroupX = 8;
  compute.GroupY = 8;
  constexpr auto path = "build/shaders/depthPyramid.comp.spv";
  auto *pipeline = CompileComputePipeline(device, path, compute, error);
  CHECK(pipeline != nullptr, "known compute declaration matches the real SPIR-V");
  SDL_ReleaseGPUComputePipeline(device, pipeline);
  for (auto field : {&ComputeShape::Samplers,
                     &ComputeShape::ReadOnlyTextures,
                     &ComputeShape::ReadWriteTextures,
                     &ComputeShape::ReadOnlyBuffers,
                     &ComputeShape::ReadWriteBuffers,
                     &ComputeShape::UniformBuffers,
                     &ComputeShape::GroupX,
                     &ComputeShape::GroupY,
                     &ComputeShape::GroupZ}) {
    auto wrong = compute;
    ++(wrong.*field);
    error.clear();
    pipeline = CompileComputePipeline(device, path, wrong, error);
    CHECK(pipeline == nullptr && error.find("declared compute bindings") != std::string::npos,
          "each incorrect compute binding count or workgroup dimension is refused");
    SDL_ReleaseGPUComputePipeline(device, pipeline);
  }
  SDL_DestroyGPUDevice(device);
  SDL_ShaderCross_Quit();
  SDL_Quit();
  Covers(
      "real graphics and compute compiler consumers accept known shaders and reject each altered "
      "binding count/workgroup dimension; not exhaustive shader or rendered binding coverage");
  return Report();
}
