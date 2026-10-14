#include "LightVisibilityStage.h"
#include "ShaderFile.h"
#include "SubjectDraw.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>

namespace outshine::Render {

SDL_GPUGraphicsPipeline *LightVisibilityStage::MaskedPipeline(VertexLayout layout) const {
  const size_t uv =
      static_cast<size_t>(CarriesUv(layout)) + static_cast<size_t>(CarriesUv1(layout));
  return Masked_[uv + (CarriesColour(layout) ? kMaskedUvLayouts : 0u)].Get();
}

bool LightVisibilityStage::ConfigureMasked(const Gpu &gpu, std::string &error) {
  SDL_GPUDevice *const device = gpu.Device;
  if (device == nullptr) { return false; }
  constexpr auto kUvLayouts = static_cast<uint32_t>(kMaskedUvLayouts);
  for (uint32_t at = 0; at < Masked_.size(); ++at) {
    if (Masked_[at]) { continue; }
    const uint32_t uv = at % kUvLayouts;
    const uint32_t tinted = at / kUvLayouts;
    const DrawShape shape{.VertexUniformBuffers = 1,
                          .VertexStorageBuffers = 1u + tinted,
                          .FragmentSamplers = uv > 0 ? 1u : 0u,
                          .FragmentUniformBuffers = 1};
    const OwnedShader vertex(
        device,
        ShaderFrom(device,
                   std::format("build/shaders/subjectShadow-{}{}.vert.spv", uv, tinted),
                   SDL_GPU_SHADERSTAGE_VERTEX,
                   shape,
                   error));
    const OwnedShader fragment(
        device,
        ShaderFrom(device,
                   std::format("build/shaders/subjectShadow-{}.frag.spv", uv),
                   SDL_GPU_SHADERSTAGE_FRAGMENT,
                   shape,
                   error));
    if (!vertex || !fragment) { return false; }
    std::array<SDL_GPUVertexBufferDescription, kUvLayouts> buffers{};
    std::array<SDL_GPUVertexAttribute, kUvLayouts> attributes{};
    for (uint32_t slot = 0; slot <= uv; ++slot) {
      buffers[slot] = {.slot = slot,
                       .pitch = static_cast<uint32_t>((slot == 0 ? 3u : 2u) * sizeof(float)),
                       .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                       .instance_step_rate = 0};
      attributes[slot] = {.location = slot,
                          .buffer_slot = slot,
                          .format = slot == 0 ? SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3
                                              : SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                          .offset = 0};
    }
    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_shader = vertex.Get();
    pipeline.fragment_shader = fragment.Get();
    pipeline.vertex_input_state.vertex_buffer_descriptions = buffers.data();
    pipeline.vertex_input_state.num_vertex_buffers = uv + 1;
    pipeline.vertex_input_state.vertex_attributes = attributes.data();
    pipeline.vertex_input_state.num_vertex_attributes = uv + 1;
    pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline.depth_stencil_state.enable_depth_test = true;
    pipeline.depth_stencil_state.enable_depth_write = true;
    pipeline.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER;
    pipeline.target_info.has_depth_stencil_target = true;
    pipeline.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    SDL_GPUGraphicsPipeline *made = SDL_CreateGPUGraphicsPipeline(device, &pipeline);
    if (made == nullptr) {
      error = std::string("masked shadow pipeline was refused: ") + SDL_GetError();
      return false;
    }
    Masked_[at] = OwnedPipeline(device, made);
  }
  return true;
}

}
