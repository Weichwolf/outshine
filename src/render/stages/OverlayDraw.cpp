#include "OverlayDraw.h"

#include <cstring>

#include "ShaderFile.h"
#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

constexpr uint32_t kAttributes = 5;

}

bool OverlayDraw::Configure(const Gpu &gpu, SDL_GPUSampler *smooth,
                            SDL_GPUTextureFormat targetFormat, std::string &error) {
  Smooth = smooth;

  Encodes = targetFormat == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB ||
            targetFormat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;

  const std::string source = ShaderSource(error);
  if (source.empty()) { return false; }
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  wanted.num_samplers = ShaderShape.VertexSamplers;
  wanted.num_uniform_buffers = ShaderShape.VertexUniformBuffers;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_samplers = ShaderShape.FragmentSamplers;
  wanted.num_uniform_buffers = ShaderShape.FragmentUniformBuffers;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the overlay did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUVertexBufferDescription buffer{};
  buffer.slot = 0;
  buffer.pitch = sizeof(OverlayQuad);

  buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
  SDL_GPUVertexAttribute attributes[kAttributes] = {};
  for (uint32_t i = 0; i < kAttributes; ++i) {
    attributes[i].location = i;
    attributes[i].buffer_slot = 0;
    attributes[i].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[i].offset = i * 4u * (uint32_t)sizeof(float);
  }

  SDL_GPUColorTargetDescription target{};
  target.format = targetFormat;
  target.blend_state.enable_blend = true;
  target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

  SDL_GPUGraphicsPipelineCreateInfo pipeline{};
  pipeline.vertex_shader = vertex.Get();
  pipeline.fragment_shader = fragment.Get();
  pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline.vertex_input_state.vertex_buffer_descriptions = &buffer;
  pipeline.vertex_input_state.num_vertex_buffers = 1;
  pipeline.vertex_input_state.vertex_attributes = attributes;
  pipeline.vertex_input_state.num_vertex_attributes = kAttributes;
  pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  pipeline.target_info.color_target_descriptions = &target;
  pipeline.target_info.num_color_targets = 1;
  SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(gpu.Device, &pipeline);
  if (!made) {
    error = std::string("the overlay's pipeline was refused: ") + SDL_GetError();
    return false;
  }
  Pipe = OwnedPipeline(gpu.Device, made);

  static const uint8_t kWhite[4] = {255, 255, 255, 255};
  return SetAtlas(gpu, kWhite, 1, 1, error);
}

bool OverlayDraw::SetAtlas(const Gpu &gpu, const uint8_t *rgba, int width, int height,
                           std::string &error) {
  if (rgba == nullptr || width <= 0 || height <= 0) {
    error = "the overlay atlas has no texels, and a texture of nothing is not a texture";
    return false;
  }
  SDL_GPUTextureCreateInfo wanted{};
  wanted.type = SDL_GPU_TEXTURETYPE_2D;
  wanted.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  wanted.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  wanted.width = (Uint32)width;
  wanted.height = (Uint32)height;
  wanted.layer_count_or_depth = 1;
  wanted.num_levels = 1;
  OwnedTexture made(gpu.Device, SDL_CreateGPUTexture(gpu.Device, &wanted));
  if (!made) {
    error = std::string("the overlay atlas was refused: ") + SDL_GetError();
    return false;
  }

  const uint32_t bytes = (uint32_t)width * (uint32_t)height * 4u;
  SDL_GPUTransferBufferCreateInfo wantedTransfer{};
  wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  wantedTransfer.size = bytes;
  SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(gpu.Device, &wantedTransfer);
  if (!staging) {
    error = std::string("the overlay atlas has no staging buffer: ") + SDL_GetError();
    return false;
  }
  std::memcpy(SDL_MapGPUTransferBuffer(gpu.Device, staging, false), rgba, bytes);
  SDL_UnmapGPUTransferBuffer(gpu.Device, staging);

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(gpu.Device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = staging;
  SDL_GPUTextureRegion into{};
  into.texture = made.Get();
  into.w = (Uint32)width;
  into.h = (Uint32)height;
  into.d = 1;
  SDL_UploadToGPUTexture(copy, &source, &into, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(gpu.Device, staging);
  Atlas = std::move(made);
  return true;
}

bool OverlayDraw::SetQuads(const Gpu &gpu, const OverlayQuad *quads, size_t count,
                           std::string &error) {
  if (count > kMaxOverlayQuads) {
    error = "the overlay was given " + std::to_string(count) + " rectangles and holds " +
            std::to_string(kMaxOverlayQuads) + ", which is " + std::to_string(count - kMaxOverlayQuads) +
            " past the bound -- a list cut without a word draws a picture nobody declared";
    return false;
  }
  Count = (uint32_t)count;
  if (count == 0) { return true; }

  const uint32_t bytes = (uint32_t)(count * sizeof(OverlayQuad));

  if (!Verts || Capacity < count) {
    SDL_GPUBufferCreateInfo wanted{};
    wanted.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    wanted.size = (Uint32)(kMaxOverlayQuads * sizeof(OverlayQuad));
    OwnedBuffer made(gpu.Device, SDL_CreateGPUBuffer(gpu.Device, &wanted));
    if (!made) {
      error = std::string("the overlay's rectangles have no buffer: ") + SDL_GetError();
      return false;
    }
    Verts = std::move(made);
    Capacity = (uint32_t)kMaxOverlayQuads;
  }

  SDL_GPUTransferBufferCreateInfo wantedTransfer{};
  wantedTransfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  wantedTransfer.size = bytes;
  SDL_GPUTransferBuffer *staging = SDL_CreateGPUTransferBuffer(gpu.Device, &wantedTransfer);
  if (!staging) {
    error = std::string("the overlay's rectangles have no staging buffer: ") + SDL_GetError();
    return false;
  }
  std::memcpy(SDL_MapGPUTransferBuffer(gpu.Device, staging, false), quads, bytes);
  SDL_UnmapGPUTransferBuffer(gpu.Device, staging);

  SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(gpu.Device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
  SDL_GPUTransferBufferLocation from{staging, 0};
  SDL_GPUBufferRegion region{Verts.Get(), 0, bytes};
  SDL_UploadToGPUBuffer(copy, &from, &region, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(gpu.Device, staging);
  return true;
}

void OverlayDraw::Encode(const FrameContext &ctx, const PassRecording &into) {
  (void)ctx;
  if (!Pipe || Count == 0 || !Verts || WidthPx <= 0 || HeightPx <= 0) { return; }
  struct Frame {
    float TargetPx[2];
    float EncodesSrgb;
    float Pad;
  } frame{{(float)WidthPx, (float)HeightPx}, Encodes ? 1.0f : 0.0f, 0.0f};
  SDL_PushGPUVertexUniformData(into.Commands, 0, &frame, sizeof frame);

  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  SDL_GPUBufferBinding binding{Verts.Get(), 0};
  SDL_BindGPUVertexBuffers(into.Pass, 0, &binding, 1);
  SDL_GPUTextureSamplerBinding sampled{Atlas.Get(), Smooth};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, &sampled, 1);
  SDL_DrawGPUPrimitives(into.Pass, 6, Count, 0, 0);
}

std::string OverlayDraw::ShaderSource(void) {
  std::string ignored;
  return ShaderSource(ignored);
}

std::string OverlayDraw::ShaderSource(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/overlay.msl", body, error)) { return std::string(); }
  return std::string(kMslPrelude) + body;
}

}
