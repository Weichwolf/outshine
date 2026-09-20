#include <SDL3/SDL.h>
#include <import/GltfImporter.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <string>
#include <span>
#include <vector>

#include "Check.h"
#include "GpuOwned.h"
#include "KernelShape.h"
#include "Lens.h"
#include "PreparedRoot.h"
#include "ShaderFile.h"
#include "Shape.h"
#include "SubjectMaterialPacking.h"
#include "SubjectResidency.h"
#include "Surfacing.h"
#include "TexelChain.h"

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

constexpr uint32_t kSourceWidth = 2048;
constexpr uint32_t kTargetWidth = 1280;
constexpr uint32_t kTargetHeight = 720;
constexpr uint32_t kChannels = 4;
constexpr uint32_t kHalfBytes = 2;
constexpr float kTexelsPerPixel = 2.6f;
constexpr float kFractionalLodUvScaleX =
    kTexelsPerPixel * static_cast<float>(kTargetWidth) / static_cast<float>(kSourceWidth);
constexpr float kFractionalLodUvScaleY =
    kTexelsPerPixel * static_cast<float>(kTargetHeight) / static_cast<float>(kSourceWidth);

struct Fixture {
  OwnedDevice Device;
  OwnedTexture Source;
  OwnedTexture Target;
  OwnedTexture Depth;
  OwnedSampler Sampler;
  OwnedPipeline Pipeline;
  OwnedBuffer Vertices;
  OwnedBuffer Uvs;
  OwnedBuffer Emitted;
  OwnedBuffer Placements;
  OwnedBuffer Indices;
  OwnedBuffer Indirect;
  std::vector<float> VertexUniform;
  std::vector<float> FragmentUniform;
  uint32_t IndexCount = 3;
  bool Derivatives = false;
  bool DescriptorTable = false;

  explicit Fixture(SDL_GPUDevice *device) : Device(device) {}
};

enum class MipMode { None, Nearest, Linear };
enum class RasterShape { Fullscreen, SmallTriangle };

uint32_t Levels(MipMode mode);

bool UploadBuffer(Fixture &fixture,
                  std::span<const std::byte> bytes,
                  SDL_GPUBufferUsageFlags usage,
                  OwnedBuffer &destination) {
  SDL_GPUBufferCreateInfo wanted{.usage = usage, .size = static_cast<uint32_t>(bytes.size())};
  destination =
      OwnedBuffer(fixture.Device.Get(), SDL_CreateGPUBuffer(fixture.Device.Get(), &wanted));
  SDL_GPUTransferBufferCreateInfo stagingInfo{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                              .size = wanted.size};
  const OwnedTransfer staging(fixture.Device.Get(),
                              SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &stagingInfo));
  if (!destination || !staging) { return false; }
  void *mapped = SDL_MapGPUTransferBuffer(fixture.Device.Get(), staging.Get(), false);
  if (mapped == nullptr) { return false; }
  std::memcpy(mapped, bytes.data(), bytes.size());
  SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), staging.Get());
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
  if (commands == nullptr) { return false; }
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  if (copy == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return false;
  }
  const SDL_GPUBufferRegion into{.buffer = destination.Get(), .size = wanted.size};
  const SDL_GPUTransferBufferLocation from{.transfer_buffer = staging.Get()};
  SDL_UploadToGPUBuffer(copy, &from, &into, false);
  SDL_EndGPUCopyPass(copy);
  return SDL_SubmitGPUCommandBuffer(commands);
}

std::vector<float> Pixels(uint32_t width) {
  std::vector<float> pixels(static_cast<size_t>(width) * width * kChannels);
  for (uint32_t y = 0; y < width; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t at = (static_cast<size_t>(y) * width + x) * kChannels;
      const float checker = ((x ^ y) & 1u) == 0 ? 0.0f : 1.0f;
      pixels[at] = checker;
      pixels[at + 1] = 1.0f - checker;
      pixels[at + 2] = static_cast<float>(x) / static_cast<float>(width - 1u);
      pixels[at + 3] = 1.0f;
    }
  }
  return pixels;
}

std::vector<uint8_t> Encode(std::span<const float> linear) {
  std::vector<uint8_t> encoded(linear.size());
  for (size_t at = 0; at < linear.size(); ++at) {
    encoded[at] = static_cast<uint8_t>(std::lround(std::clamp(linear[at], 0.0f, 1.0f) * 255.0f));
  }
  return encoded;
}

std::vector<std::vector<uint8_t>> Chain(MipMode mode) {
  std::vector<float> level = Pixels(kSourceWidth);
  std::vector<std::vector<uint8_t>> chain;
  uint32_t width = kSourceWidth;
  for (uint32_t mip = 0; mip < Levels(mode); ++mip) {
    chain.push_back(Encode(level));
    if (width > 1) {
      std::vector<float> next;
      HalveInPlace(level, {.WidthPx = width, .HeightPx = width}, next, TexelKind::Value);
      level.swap(next);
      width /= 2;
    }
  }
  return chain;
}

uint32_t Levels(MipMode mode) {
  if (mode == MipMode::None) { return 1; }
  uint32_t levels = 1;
  for (uint32_t width = kSourceWidth; width > 1; width /= 2) { ++levels; }
  return levels;
}

bool UploadChain(Fixture &fixture, MipMode mode, bool separateSubmits, bool srgb) {
  const uint32_t levels = Levels(mode);
  const auto chain = Chain(mode);
  uint32_t bytes = 0;
  for (const auto &level : chain) { bytes += static_cast<uint32_t>(level.size()); }
  SDL_GPUTextureCreateInfo texture{};
  texture.type = SDL_GPU_TEXTURETYPE_2D;
  texture.format =
      srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  texture.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  texture.width = kSourceWidth;
  texture.height = kSourceWidth;
  texture.layer_count_or_depth = 1;
  texture.num_levels = levels;
  texture.sample_count = SDL_GPU_SAMPLECOUNT_1;
  fixture.Source =
      OwnedTexture(fixture.Device.Get(), SDL_CreateGPUTexture(fixture.Device.Get(), &texture));
  if (!fixture.Source) { return false; }
  if (separateSubmits) {
    uint32_t width = kSourceWidth;
    for (uint32_t mip = 0; mip < levels; ++mip) {
      SDL_GPUTransferBufferCreateInfo one{};
      one.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      one.size = static_cast<uint32_t>(chain[mip].size());
      const OwnedTransfer staging(fixture.Device.Get(),
                                  SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &one));
      if (!staging) { return false; }
      void *mapped = SDL_MapGPUTransferBuffer(fixture.Device.Get(), staging.Get(), false);
      if (mapped == nullptr) { return false; }
      std::memcpy(mapped, chain[mip].data(), chain[mip].size());
      SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), staging.Get());
      SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
      if (commands == nullptr) { return false; }
      SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
      if (copy == nullptr) {
        SDL_CancelGPUCommandBuffer(commands);
        return false;
      }
      SDL_GPUTextureTransferInfo source{
          .transfer_buffer = staging.Get(), .pixels_per_row = width, .rows_per_layer = width};
      SDL_GPUTextureRegion destination{
          .texture = fixture.Source.Get(), .mip_level = mip, .w = width, .h = width, .d = 1};
      SDL_UploadToGPUTexture(copy, &source, &destination, false);
      SDL_EndGPUCopyPass(copy);
      if (!SDL_SubmitGPUCommandBuffer(commands)) { return false; }
      width /= 2;
    }
    return true;
  }
  SDL_GPUTransferBufferCreateInfo transfer{};
  transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer.size = bytes;
  OwnedTransfer upload(fixture.Device.Get(),
                       SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &transfer));
  if (!upload) { return false; }
  auto *mapped =
      static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(fixture.Device.Get(), upload.Get(), false));
  if (mapped == nullptr) { return false; }
  uint32_t offset = 0;
  for (const auto &level : chain) {
    std::memcpy(mapped + offset, level.data(), level.size());
    offset += static_cast<uint32_t>(level.size());
  }
  SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), upload.Get());
  const auto record = [&](uint32_t first, uint32_t count) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
    if (commands == nullptr) { return false; }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    if (copy == nullptr) {
      SDL_CancelGPUCommandBuffer(commands);
      return false;
    }
    uint32_t levelOffset = 0;
    for (uint32_t prior = 0; prior < first; ++prior) {
      levelOffset += static_cast<uint32_t>(chain[prior].size());
    }
    uint32_t width = kSourceWidth >> first;
    for (uint32_t mip = first; mip < first + count; ++mip) {
      SDL_GPUTextureTransferInfo source{.transfer_buffer = upload.Get(),
                                        .offset = levelOffset,
                                        .pixels_per_row = width,
                                        .rows_per_layer = width};
      SDL_GPUTextureRegion destination{
          .texture = fixture.Source.Get(), .mip_level = mip, .w = width, .h = width, .d = 1};
      SDL_UploadToGPUTexture(copy, &source, &destination, false);
      levelOffset += static_cast<uint32_t>(chain[mip].size());
      width /= 2;
    }
    SDL_EndGPUCopyPass(copy);
    return SDL_SubmitGPUCommandBuffer(commands);
  };
  return record(0, levels);
}

bool Configure(Fixture &fixture,
               MipMode mode,
               bool separateSubmits,
               bool descriptorTable,
               bool derivatives,
               bool srgb,
               RasterShape rasterShape) {
  if (!UploadChain(fixture, mode, separateSubmits, srgb)) { return false; }
  fixture.DescriptorTable = descriptorTable;
  fixture.Derivatives = derivatives;
  SDL_GPUSamplerCreateInfo sampler{};
  sampler.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
  sampler.mipmap_mode = mode == MipMode::Nearest ? SDL_GPU_SAMPLERMIPMAPMODE_NEAREST
                                                 : SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sampler.max_lod = static_cast<float>(Levels(mode) - 1u);
  fixture.Sampler =
      OwnedSampler(fixture.Device.Get(), SDL_CreateGPUSampler(fixture.Device.Get(), &sampler));
  if (!fixture.Sampler) { return false; }
  std::string error;
  const char *vertexPath = derivatives ? "build/shaders/filteredMipDerivativeSample.vert.spv"
                                       : "build/shaders/fullscreen.vert.spv";
  const OwnedShader vertex(
      fixture.Device.Get(),
      ShaderFrom(fixture.Device.Get(), vertexPath, SDL_GPU_SHADERSTAGE_VERTEX, DrawShape{}, error));
  const DrawShape shape{.FragmentSamplers = descriptorTable ? 8u : 1u};
  const char *fragmentPath = descriptorTable ? "build/shaders/filteredMipDescriptorSample.frag.spv"
                             : derivatives   ? "build/shaders/filteredMipDerivativeSample.frag.spv"
                                             : "build/shaders/filteredMipSample.frag.spv";
  const OwnedShader fragment(
      fixture.Device.Get(),
      ShaderFrom(fixture.Device.Get(), fragmentPath, SDL_GPU_SHADERSTAGE_FRAGMENT, shape, error));
  if (!vertex || !fragment) {
    SDL_SetError("%s", error.c_str());
    return false;
  }
  SDL_GPUColorTargetDescription target{.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT};
  const std::array<SDL_GPUVertexBufferDescription, 2> buffers = {
      SDL_GPUVertexBufferDescription{.slot = 0, .pitch = 4u * sizeof(float)},
      SDL_GPUVertexBufferDescription{.slot = 1, .pitch = 2u * sizeof(float)}};
  const std::array<SDL_GPUVertexAttribute, 2> attributes = {
      SDL_GPUVertexAttribute{.location = 0,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                             .offset = 0},
      SDL_GPUVertexAttribute{.location = 1,
                             .buffer_slot = 1,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                             .offset = 0}};
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  if (derivatives) {
    pipeline.vertex_input_state.vertex_buffer_descriptions = buffers.data();
    pipeline.vertex_input_state.num_vertex_buffers = static_cast<uint32_t>(buffers.size());
    pipeline.vertex_input_state.vertex_attributes = attributes.data();
    pipeline.vertex_input_state.num_vertex_attributes = static_cast<uint32_t>(attributes.size());
  }
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.target_info.color_target_descriptions = &target;
  pipeline.target_info.num_color_targets = 1;
  pipeline.target_info.has_depth_stencil_target = true;
  pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  pipeline.depth_stencil_state.enable_depth_test = true;
  pipeline.depth_stencil_state.enable_depth_write = true;
  pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  fixture.Pipeline = OwnedPipeline(fixture.Device.Get(),
                                   SDL_CreateGPUGraphicsPipeline(fixture.Device.Get(), &pipeline));
  if (!fixture.Pipeline || !derivatives) { return static_cast<bool>(fixture.Pipeline); }
  constexpr std::array<float, 12> fullscreenPositions = {
      -1, -1, 0.5f, 1, 3, -1, 0.5f, 1, -1, 3, 0.5f, 1};
  constexpr std::array<float, 6> fullscreenUvs = {
      0, 0, 2 * kFractionalLodUvScaleX, 0, 0, 2 * kFractionalLodUvScaleY};
  constexpr float kSmallUvSpanX = 0.25f * kFractionalLodUvScaleX;
  constexpr float kSmallUvSpanY = 0.25f * kFractionalLodUvScaleY;
  constexpr std::array<float, 12> smallPositions = {
      -0.25f, -0.25f, 0.5f, 1, 0.5f, -0.5f, 1, 2, -0.375f, 0.375f, 0.75f, 1.5f};
  constexpr std::array<float, 6> smallUvs = {0, 0, kSmallUvSpanX, 0, 0, kSmallUvSpanY};
  constexpr std::array<uint32_t, 3> indices = {0, 1, 2};
  constexpr SDL_GPUIndexedIndirectDrawCommand indirect{.num_indices = 3,
                                                       .num_instances = 1,
                                                       .first_index = 0,
                                                       .vertex_offset = 0,
                                                       .first_instance = 0};
  const std::array<float, 12> &positions =
      rasterShape == RasterShape::Fullscreen ? fullscreenPositions : smallPositions;
  const std::array<float, 6> &uvs =
      rasterShape == RasterShape::Fullscreen ? fullscreenUvs : smallUvs;
  const auto bytesOf = [](const auto &values) { return std::as_bytes(std::span(values)); };
  return UploadBuffer(fixture, bytesOf(positions), SDL_GPU_BUFFERUSAGE_VERTEX, fixture.Vertices) &&
         UploadBuffer(fixture, bytesOf(uvs), SDL_GPU_BUFFERUSAGE_VERTEX, fixture.Uvs) &&
         UploadBuffer(fixture, bytesOf(indices), SDL_GPU_BUFFERUSAGE_INDEX, fixture.Indices) &&
         UploadBuffer(fixture,
                      std::as_bytes(std::span(&indirect, 1)),
                      SDL_GPU_BUFFERUSAGE_INDIRECT,
                      fixture.Indirect);
}

std::vector<uint8_t> Draw(Fixture &fixture) {
  if (!fixture.Target) {
    SDL_GPUTextureCreateInfo target{};
    target.type = SDL_GPU_TEXTURETYPE_2D;
    target.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    target.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    target.width = kTargetWidth;
    target.height = kTargetHeight;
    target.layer_count_or_depth = 1;
    target.num_levels = 1;
    target.sample_count = SDL_GPU_SAMPLECOUNT_1;
    fixture.Target =
        OwnedTexture(fixture.Device.Get(), SDL_CreateGPUTexture(fixture.Device.Get(), &target));
    target.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    target.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    fixture.Depth =
        OwnedTexture(fixture.Device.Get(), SDL_CreateGPUTexture(fixture.Device.Get(), &target));
  }
  if (!fixture.Target || !fixture.Depth) { return {}; }
  SDL_GPUTransferBufferCreateInfo transfer{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                           .size = kTargetWidth * kTargetHeight * kChannels *
                                                   kHalfBytes};
  OwnedTransfer download(fixture.Device.Get(),
                         SDL_CreateGPUTransferBuffer(fixture.Device.Get(), &transfer));
  if (!download) { return {}; }
  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(fixture.Device.Get());
  if (commands == nullptr) { return {}; }
  SDL_GPUColorTargetInfo attachment{.texture = fixture.Target.Get(),
                                    .clear_color = {0, 0, 0, 0},
                                    .load_op = SDL_GPU_LOADOP_CLEAR,
                                    .store_op = SDL_GPU_STOREOP_STORE};
  SDL_GPUDepthStencilTargetInfo depth{.texture = fixture.Depth.Get(),
                                      .clear_depth = 0.0f,
                                      .load_op = SDL_GPU_LOADOP_CLEAR,
                                      .store_op = SDL_GPU_STOREOP_STORE,
                                      .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                                      .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE};
  SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &attachment, 1, &depth);
  if (pass == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return {};
  }
  SDL_BindGPUGraphicsPipeline(pass, fixture.Pipeline.Get());
  if (!fixture.VertexUniform.empty()) {
    SDL_PushGPUVertexUniformData(
        commands,
        0,
        fixture.VertexUniform.data(),
        static_cast<uint32_t>(fixture.VertexUniform.size() * sizeof(float)));
  }
  if (!fixture.FragmentUniform.empty()) {
    SDL_PushGPUFragmentUniformData(
        commands,
        0,
        fixture.FragmentUniform.data(),
        static_cast<uint32_t>(fixture.FragmentUniform.size() * sizeof(float)));
  }
  if (fixture.Derivatives) {
    const std::array<SDL_GPUBufferBinding, 3> streams = {
        SDL_GPUBufferBinding{.buffer = fixture.Vertices.Get()},
        SDL_GPUBufferBinding{.buffer = fixture.Uvs.Get()},
        SDL_GPUBufferBinding{.buffer = fixture.Emitted.Get()}};
    SDL_BindGPUVertexBuffers(pass, 0, streams.data(), fixture.Emitted ? 3u : 2u);
    const SDL_GPUBufferBinding indices{.buffer = fixture.Indices.Get()};
    SDL_BindGPUIndexBuffer(pass, &indices, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  }
  if (fixture.Placements) {
    const std::array<SDL_GPUBuffer *, 1> storage = {fixture.Placements.Get()};
    SDL_BindGPUVertexStorageBuffers(pass, 0, storage.data(), 1);
  }
  const SDL_GPUTextureSamplerBinding binding{.texture = fixture.Source.Get(),
                                             .sampler = fixture.Sampler.Get()};
  const std::array<SDL_GPUTextureSamplerBinding, 8> descriptors = {
      binding, binding, binding, binding, binding, binding, binding, binding};
  SDL_BindGPUFragmentSamplers(pass, 0, descriptors.data(), fixture.DescriptorTable ? 8u : 1u);
  if (fixture.Derivatives) {
    SDL_DrawGPUIndexedPrimitivesIndirect(pass, fixture.Indirect.Get(), 0, 1);
  } else {
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
  }
  SDL_EndGPURenderPass(pass);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  if (copy == nullptr) {
    SDL_CancelGPUCommandBuffer(commands);
    return {};
  }
  SDL_GPUTextureRegion source{
      .texture = fixture.Target.Get(), .w = kTargetWidth, .h = kTargetHeight, .d = 1};
  SDL_GPUTextureTransferInfo destination{.transfer_buffer = download.Get(),
                                         .pixels_per_row = kTargetWidth,
                                         .rows_per_layer = kTargetHeight};
  SDL_DownloadFromGPUTexture(copy, &source, &destination);
  SDL_EndGPUCopyPass(copy);
  SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
  if (fence == nullptr) { return {}; }
  const bool ready = SDL_WaitForGPUFences(fixture.Device.Get(), true, &fence, 1);
  SDL_ReleaseGPUFence(fixture.Device.Get(), fence);
  if (!ready) { return {}; }
  const auto *mapped = static_cast<const uint8_t *>(
      SDL_MapGPUTransferBuffer(fixture.Device.Get(), download.Get(), false));
  if (mapped == nullptr) { return {}; }
  std::vector<uint8_t> pixels(mapped, mapped + transfer.size);
  SDL_UnmapGPUTransferBuffer(fixture.Device.Get(), download.Get());
  return pixels;
}

bool ConfigureImported(Fixture &fixture) {
  const std::string path = PreparedRoot() + "/test-khronos-glTF-ABeautifulGame/scene.gltf";
  outshine::GltfImporter imported;
  const auto loaded = imported.load(path);
  if (!loaded) {
    SDL_SetError("%s", loaded.error().c_str());
    return false;
  }
  const outshine::Geometry &native = imported.geometry();
  ShapeStore store;
  const auto shaped = PrepareShape(native, store);
  if (!shaped || shaped->Parts.empty()) {
    SDL_SetError("could not pack the imported chess part");
    return false;
  }
  const ShapePart &part = shaped->Parts.front();
  SubjectMaterial material;
  material.Row = native.surfaceAt(native.materialOf(0));
  std::string error;
  if (!ResolveNativeTextures(native, std::span(&material, 1), error)) {
    SDL_SetError("%s", error.c_str());
    return false;
  }
  SubjectResidency residency;
  residency.StandsOn(fixture.Device.Get());
  auto bound =
      residency.Upload(material.Colour, SubjectResidency::Transfer::Srgb, TexelKind::Value);
  if (!bound) {
    SDL_SetError("%s", bound.error().c_str());
    return false;
  }
  fixture.Source = std::move(bound->Image);
  fixture.Sampler = std::move(bound->Sample);

  const OwnedShader vertex(
      fixture.Device.Get(),
      ShaderFrom(fixture.Device.Get(),
                 "build/shaders/flat-10-0.vert.spv",
                 SDL_GPU_SHADERSTAGE_VERTEX,
                 DrawShape{.VertexUniformBuffers = 1, .VertexStorageBuffers = 1},
                 error));
  const OwnedShader fragment(
      fixture.Device.Get(),
      ShaderFrom(fixture.Device.Get(),
                 "build/shaders/flat-01-000.frag.spv",
                 SDL_GPU_SHADERSTAGE_FRAGMENT,
                 DrawShape{.FragmentSamplers = 1, .FragmentUniformBuffers = 1},
                 error));
  if (!vertex || !fragment) {
    SDL_SetError("%s", error.c_str());
    return false;
  }
  SDL_GPUColorTargetDescription target{.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT};
  const std::array<SDL_GPUVertexBufferDescription, 3> buffers = {
      SDL_GPUVertexBufferDescription{.slot = 0, .pitch = 3u * sizeof(float)},
      SDL_GPUVertexBufferDescription{.slot = 1, .pitch = 2u * sizeof(float)},
      SDL_GPUVertexBufferDescription{.slot = 2, .pitch = 3u * sizeof(float)}};
  const std::array<SDL_GPUVertexAttribute, 3> attributes = {
      SDL_GPUVertexAttribute{.location = 0,
                             .buffer_slot = 0,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                             .offset = 0},
      SDL_GPUVertexAttribute{.location = 1,
                             .buffer_slot = 1,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                             .offset = 0},
      SDL_GPUVertexAttribute{.location = 2,
                             .buffer_slot = 2,
                             .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                             .offset = 0}};
  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.vertex_input_state.vertex_buffer_descriptions = buffers.data();
  pipeline.vertex_input_state.num_vertex_buffers = static_cast<uint32_t>(buffers.size());
  pipeline.vertex_input_state.vertex_attributes = attributes.data();
  pipeline.vertex_input_state.num_vertex_attributes = static_cast<uint32_t>(attributes.size());
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = &target;
  pipeline.target_info.num_color_targets = 1;
  pipeline.target_info.has_depth_stencil_target = true;
  pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  pipeline.depth_stencil_state.enable_depth_test = true;
  pipeline.depth_stencil_state.enable_depth_write = true;
  pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
  fixture.Pipeline = OwnedPipeline(fixture.Device.Get(),
                                   SDL_CreateGPUGraphicsPipeline(fixture.Device.Get(), &pipeline));
  if (!fixture.Pipeline) { return false; }

  const auto indices = shaped->Indices.subspan(part.FirstIndex, part.IndexCount);
  const SDL_GPUIndexedIndirectDrawCommand indirect{.num_indices =
                                                       static_cast<uint32_t>(indices.size()),
                                                   .num_instances = 1,
                                                   .first_index = 0,
                                                   .vertex_offset = 0,
                                                   .first_instance = 0};
  std::vector<float> emitted(part.VertexCount * 3u);
  for (size_t vertexIndex = 0; vertexIndex < part.VertexCount; ++vertexIndex) {
    for (size_t channel = 0; channel < 3; ++channel) {
      emitted[vertexIndex * 3u + channel] = material.Row.BaseColour[channel];
    }
  }
  constexpr std::array<float, 32> placements = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                                                1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  if (!UploadBuffer(
          fixture, std::as_bytes(part.PositionsM), SDL_GPU_BUFFERUSAGE_VERTEX, fixture.Vertices) ||
      !UploadBuffer(fixture, std::as_bytes(part.Uv), SDL_GPU_BUFFERUSAGE_VERTEX, fixture.Uvs) ||
      !UploadBuffer(fixture,
                    std::as_bytes(std::span(emitted)),
                    SDL_GPU_BUFFERUSAGE_VERTEX,
                    fixture.Emitted) ||
      !UploadBuffer(fixture,
                    std::as_bytes(std::span(placements)),
                    SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                    fixture.Placements) ||
      !UploadBuffer(fixture, std::as_bytes(indices), SDL_GPU_BUFFERUSAGE_INDEX, fixture.Indices) ||
      !UploadBuffer(fixture,
                    std::as_bytes(std::span(&indirect, 1)),
                    SDL_GPU_BUFFERUSAGE_INDIRECT,
                    fixture.Indirect)) {
    return false;
  }
  fixture.Derivatives = true;

  constexpr outshine::Vec3 eye = {{2.781138576118416, 1.3202289998916963, 1.9473741958039332}};
  constexpr outshine::Vec3 aim = {{0, 0.08449789705936794, 0}};
  auto viewpoint = Viewpoint::LookAt({.EyeM = eye, .AimM = aim}, 0.0);
  if (!viewpoint) { return false; }
  viewpoint->YfovRad = 0.47108996144172666;
  viewpoint->ZNearM = 3.107125103623776;
  viewpoint->ZFarM = 4.118946968135317;
  const auto lens = Lens::From(*viewpoint, kTargetWidth, kTargetHeight);
  if (!lens) { return false; }
  const outshine::Mat4f viewProjection = lens->ViewProjection(*viewpoint);
  fixture.VertexUniform.assign(viewProjection.begin(), viewProjection.end());
  fixture.VertexUniform.insert(
      fixture.VertexUniform.end(), viewProjection.begin(), viewProjection.end());
  fixture.VertexUniform.insert(fixture.VertexUniform.end(), 16u, 0.0f);
  fixture.VertexUniform.push_back(-static_cast<float>(eye[0]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[1]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[2]));
  fixture.VertexUniform.push_back(0.0f);
  fixture.VertexUniform.push_back(-static_cast<float>(eye[0]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[1]));
  fixture.VertexUniform.push_back(-static_cast<float>(eye[2]));
  fixture.VertexUniform.push_back(0.0f);
  const PackedSubjectMaterial packed = PackSubjectMaterial(material, 1.0f);
  fixture.FragmentUniform.assign(packed.begin(), packed.end());
  return true;
}

std::vector<uint8_t> Render(MipMode mode,
                            bool separateSubmits,
                            bool descriptorTable,
                            bool derivatives,
                            bool srgb,
                            RasterShape rasterShape) {
  Fixture fixture(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL,
                                      false,
                                      nullptr));
  if (!fixture.Device ||
      !Configure(fixture, mode, separateSubmits, descriptorTable, derivatives, srgb, rasterShape)) {
    return {};
  }
  return Draw(fixture);
}

void CheckMode(MipMode mode,
               bool separateSubmits,
               bool descriptorTable,
               bool derivatives,
               bool srgb,
               const char *name,
               RasterShape rasterShape = RasterShape::Fullscreen) {
  Fixture fixture(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                          SDL_GPU_SHADERFORMAT_DXIL,
                                      false,
                                      nullptr));
  CHECK(fixture.Device &&
            Configure(
                fixture, mode, separateSubmits, descriptorTable, derivatives, srgb, rasterShape),
        "the raw SDL fixture configures");
  if (!fixture.Device || !fixture.Pipeline) { return; }
  const auto first = Draw(fixture);
  const auto repeated = Draw(fixture);
  const auto fresh = Render(mode, separateSubmits, descriptorTable, derivatives, srgb, rasterShape);
  CHECK(!first.empty(), "the raw SDL fixture renders its first filtered frame");
  CHECK(first == repeated, name);
  CHECK(first == fresh, "a fresh SDL device has the same first filtered frame");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes for raw filtered sampling");
  CheckMode(MipMode::None, false, false, false, false, "base-only linear sampling repeats exactly");
  CheckMode(
      MipMode::Nearest, false, false, false, false, "batched nearest-mip sampling repeats exactly");
  CheckMode(
      MipMode::Linear, false, false, false, false, "batched linear-mip sampling repeats exactly");
  CheckMode(MipMode::Nearest,
            true,
            false,
            false,
            false,
            "per-level nearest-mip sampling repeats exactly");
  CheckMode(
      MipMode::Linear, true, false, false, false, "per-level linear-mip sampling repeats exactly");
  CheckMode(
      MipMode::Linear, true, true, false, false, "eight material sampler bindings repeat exactly");
  CheckMode(
      MipMode::Linear, true, false, true, false, "interpolated UV derivatives repeat exactly");
  CheckMode(
      MipMode::Linear, true, false, true, true, "sRGB interpolated UV derivatives repeat exactly");
  CheckMode(MipMode::Linear,
            true,
            false,
            true,
            true,
            "small sRGB primitive edges repeat exactly",
            RasterShape::SmallTriangle);
  Fixture imported(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                           SDL_GPU_SHADERFORMAT_DXIL,
                                       false,
                                       nullptr));
  CHECK(imported.Device && ConfigureImported(imported),
        "the exact imported chess draw configures through raw SDL");
  if (imported.Device && imported.Pipeline) {
    const auto first = Draw(imported);
    const auto repeated = Draw(imported);
    CHECK(!first.empty(), "the exact imported chess draw produces pixels");
    CHECK(std::ranges::any_of(first, [](uint8_t byte) { return byte != 0; }),
          "the exact imported chess draw reaches the target");
    CHECK(first == repeated, "the exact imported chess draw repeats every half channel exactly");
  }
  SDL_Quit();
  return Report();
}
