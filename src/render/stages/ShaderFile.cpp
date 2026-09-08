#include "ShaderFile.h"
#include "KernelShape.h"
#include <SDL3_shadercross/SDL_shadercross.h>

#include <array>
#include <cstdio>
#include <string>
#include <expected>
#include <string_view>
#include <utility>
#include <vector>
#include <cstring>

namespace outshine::Render {

namespace {

constexpr size_t kReadBlockBytes = 1u << 14u;
constexpr Uint32 kSpirvMagic = 0x07230203u;

std::expected<std::string, std::string> ReadShaderFile(std::string_view treePath) {
  const std::string path(treePath);
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return std::unexpected("the shader artifact " + path +
                           " is not readable from here -- the engine reads its shaders from the "
                           "tree, so the process must start at the repository root");
  }
  std::string held;
  std::array<char, kReadBlockBytes> block;
  for (size_t read = std::fread(block.data(), 1, block.size(), file); read > 0;
       read = std::fread(block.data(), 1, block.size(), file)) {
    held.append(block.data(), read);
  }
  const bool failed = std::ferror(file) != 0;
  std::fclose(file);
  if (failed) { return std::unexpected("reading shader artifact " + path + " failed"); }
  if (held.empty()) {
    return std::unexpected("the shader artifact " + path +
                           " is empty, and an empty kernel is a picture refusal");
  }
  return held;
}

std::expected<std::vector<Uint32>, std::string> ReadSpirv(std::string_view path) {
  const auto bytes = ReadShaderFile(path);
  if (!bytes) { return std::unexpected(bytes.error()); }
  constexpr size_t headerWords = 5;
  if (bytes->size() < headerWords * sizeof(Uint32) || bytes->size() % sizeof(Uint32) != 0) {
    return std::unexpected(std::string(path) + " is not a complete SPIR-V word stream");
  }
  std::vector<Uint32> words(bytes->size() / sizeof(Uint32));
  std::memcpy(words.data(), bytes->data(), bytes->size());
  if (words.front() != kSpirvMagic) {
    return std::unexpected(std::string(path) + " has no SPIR-V header");
  }
  return words;
}

}

namespace Says {
constexpr auto kUnknownComputeShader = "unknown built-in compute shader ID";
constexpr auto kMissingComputeDevice = "compute pipeline creation requires a GPU device";
}

std::expected<OwnedComputePipeline, std::string> CreateComputePipeline(SDL_GPUDevice *device,
                                                                       ComputeShaderId shader) {
  const auto *const descriptor = FindComputeShader(shader);
  if (descriptor == nullptr) { return std::unexpected(Says::kUnknownComputeShader); }
  if (device == nullptr) { return std::unexpected(Says::kMissingComputeDevice); }
  std::string error;
  auto *const pipeline = CompileComputePipeline(device, descriptor->Path, descriptor->Shape, error);
  if (pipeline == nullptr) { return std::unexpected(std::move(error)); }
  return OwnedComputePipeline(device, pipeline);
}

SDL_GPUComputePipeline *CompileComputePipeline(SDL_GPUDevice *device,
                                               std::string_view path,
                                               const ComputeShape &shape,
                                               std::string &error) {
  const auto code = ReadSpirv(path);
  if (!code) {
    error = code.error();
    return nullptr;
  }
  const SDL_ShaderCross_SPIRV_Info info{.bytecode = reinterpret_cast<const Uint8 *>(code->data()),
                                        .bytecode_size = code->size() * sizeof(Uint32),
                                        .entrypoint = "main",
                                        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_COMPUTE,
                                        .props = 0};
  auto *metadata = SDL_ShaderCross_ReflectComputeSPIRV(info.bytecode, info.bytecode_size, 0);
  if (metadata == nullptr) {
    error = SDL_GetError();
    return nullptr;
  }
  const bool agrees = metadata->num_samplers == shape.Samplers &&
                      metadata->num_readonly_storage_textures == shape.ReadOnlyTextures &&
                      metadata->num_readwrite_storage_textures == shape.ReadWriteTextures &&
                      metadata->num_readonly_storage_buffers == shape.ReadOnlyBuffers &&
                      metadata->num_readwrite_storage_buffers == shape.ReadWriteBuffers &&
                      metadata->num_uniform_buffers == shape.UniformBuffers &&
                      metadata->threadcount_x == shape.GroupX &&
                      metadata->threadcount_y == shape.GroupY &&
                      metadata->threadcount_z == shape.GroupZ;
  SDL_GPUComputePipeline *made = nullptr;
  if (agrees) {
    made = SDL_ShaderCross_CompileComputePipelineFromSPIRV(device, &info, metadata, 0);
    if (made == nullptr) { error = SDL_GetError(); }
  } else {
    error = std::string(path) + " does not match its declared compute bindings or workgroup";
  }
  SDL_free(metadata);
  return made;
}

SDL_GPUShader *ShaderFrom(SDL_GPUDevice *device,
                          std::string_view path,
                          SDL_GPUShaderStage stage,
                          const DrawShape &shape,
                          std::string &error) {
  const auto code = ReadSpirv(path);
  if (!code) {
    error = code.error();
    return nullptr;
  }
  const bool fragment = stage == SDL_GPU_SHADERSTAGE_FRAGMENT;
  const SDL_ShaderCross_SPIRV_Info info{.bytecode = reinterpret_cast<const Uint8 *>(code->data()),
                                        .bytecode_size = code->size() * sizeof(Uint32),
                                        .entrypoint = "main",
                                        .shader_stage = fragment
                                                            ? SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT
                                                            : SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
                                        .props = 0};
  auto *metadata = SDL_ShaderCross_ReflectGraphicsSPIRV(info.bytecode, info.bytecode_size, 0);
  if (metadata == nullptr) {
    error = SDL_GetError();
    return nullptr;
  }
  const auto &resources = metadata->resource_info;
  const bool agrees =
      resources.num_samplers == (fragment ? shape.FragmentSamplers : shape.VertexSamplers) &&
      resources.num_storage_buffers ==
          (fragment ? shape.FragmentStorageBuffers : shape.VertexStorageBuffers) &&
      resources.num_uniform_buffers ==
          (fragment ? shape.FragmentUniformBuffers : shape.VertexUniformBuffers) &&
      resources.num_storage_textures == 0;
  SDL_GPUShader *made = nullptr;
  if (agrees) {
    made = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &info, &resources, 0);
    if (made == nullptr) { error = SDL_GetError(); }
  } else {
    error = std::string(path) + " does not match its declared graphics bindings";
  }
  SDL_free(metadata);
  return made;
}

}
