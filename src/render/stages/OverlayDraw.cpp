#include "OverlayDraw.h"

#include <cstring>

#include "ShaderPrelude.h"

namespace outshine::Render {

namespace {

/* THE INTERFACE IS ONE DRAW OF ONE INSTANCED QUAD, and the instance IS the rectangle. Six vertices
 * come out of `vertex_id` so there is no vertex geometry to own; everything that differs between
 * rectangles arrives per instance, which is what lets a whole HUD be one bind and one draw.
 *
 * THE CLIP AND THE CORNER ARE FRAGMENT WORK, NOT PASS WORK. A scissor is a pass-level state change,
 * so honouring a per-rectangle clip with one would mean as many passes as clips; discarding outside
 * the region costs the fragments that were going to be thrown away anyway and keeps the draw single.
 *
 * THE ROUNDED CORNER IS THE ROUNDED-BOX DISTANCE and it is antialiased over ONE pixel: a hard cut
 * would stair-step on every panel, and a wider ramp would make a small radius look blurred. */
const char *kOverlayMsl = R"(
struct Frame { float2 targetPx; float2 pad; };

struct VIn {
  float4 rect     [[attribute(0)]];
  float4 uv       [[attribute(1)]];
  float4 colour   [[attribute(2)]];
  float4 clip     [[attribute(3)]];
  float4 shape    [[attribute(4)]];
};

struct VOut {
  float4 pos [[position]];
  float2 uv;
  float4 colour;
  float4 clip;
  float2 px;
  float2 centre;
  float2 halfSize;
  float radius;
  float hasPatch;
};

vertex VOut vs(uint vertexId [[vertex_id]], VIn in [[stage_in]], constant Frame &f [[buffer(0)]]) {
  float2 corner[6] = { float2(0,0), float2(1,0), float2(0,1),
                       float2(0,1), float2(1,0), float2(1,1) };
  float2 at = corner[vertexId];
  float2 px = in.rect.xy + at * in.rect.zw;
  VOut o;
  o.pos = float4(px.x / f.targetPx.x * 2.0 - 1.0, 1.0 - px.y / f.targetPx.y * 2.0, 0.0, 1.0);
  o.uv = mix(in.uv.xy, in.uv.zw, at);
  o.colour = in.colour;
  o.clip = in.clip;
  o.px = px;
  o.centre = in.rect.xy + in.rect.zw * 0.5;
  o.halfSize = in.rect.zw * 0.5;
  o.radius = min(in.shape.x, min(o.halfSize.x, o.halfSize.y));
  o.colour.a *= in.shape.y;
  o.hasPatch = (in.uv.z > in.uv.x && in.uv.w > in.uv.y) ? 1.0 : 0.0;
  return o;
}

fragment float4 fs(VOut in [[stage_in]],
                   texture2d<float> atlas [[texture(0)]], sampler smooth [[sampler(0)]]) {
  if (in.px.x < in.clip.x || in.px.x > in.clip.x + in.clip.z ||
      in.px.y < in.clip.y || in.px.y > in.clip.y + in.clip.w) {
    discard_fragment();
  }
  float4 out = in.colour;
  if (in.hasPatch > 0.5) { out *= atlas.sample(smooth, in.uv); }
  if (in.radius > 0.0) {
    float2 q = abs(in.px - in.centre) - (in.halfSize - in.radius);
    float d = length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - in.radius;
    out.a *= saturate(0.5 - d);
  }
  if (out.a <= 0.0) { discard_fragment(); }
  return float4(out.rgb * out.a, out.a);
}
)";

constexpr uint32_t kAttributes = 5;

} // namespace

bool OverlayDraw::Configure(const Gpu &gpu, SDL_GPUSampler *smooth,
                            SDL_GPUTextureFormat targetFormat, std::string &error) {
  Smooth = smooth;

  const std::string source = std::string(kMslPrelude) + kOverlayMsl;
  SDL_GPUShaderCreateInfo wanted{};
  wanted.code = reinterpret_cast<const Uint8 *>(source.c_str());
  wanted.code_size = source.size();
  wanted.format = SDL_GPU_SHADERFORMAT_MSL;
  wanted.entrypoint = "vs";
  wanted.stage = SDL_GPU_SHADERSTAGE_VERTEX;
  wanted.num_uniform_buffers = 1u;
  const OwnedShader vertex(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  wanted.entrypoint = "fs";
  wanted.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  wanted.num_uniform_buffers = 0u;
  wanted.num_samplers = 1u;
  const OwnedShader fragment(gpu.Device, SDL_CreateGPUShader(gpu.Device, &wanted));
  if (!vertex || !fragment) {
    error = std::string("the overlay did not compile: ") + SDL_GetError();
    return false;
  }

  SDL_GPUVertexBufferDescription buffer{};
  buffer.slot = 0;
  buffer.pitch = sizeof(OverlayQuad);
  /* PER INSTANCE AND NOT PER VERTEX: the six corners are arithmetic, the rectangle is data. */
  buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
  SDL_GPUVertexAttribute attributes[kAttributes] = {};
  for (uint32_t i = 0; i < kAttributes; ++i) {
    attributes[i].location = i;
    attributes[i].buffer_slot = 0;
    attributes[i].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[i].offset = i * 4u * (uint32_t)sizeof(float);
  }

  /* THE INTERFACE IS COMPOSITED OVER THE FRAME, PREMULTIPLIED. The fragment multiplies its own colour
   * by its alpha, so the blend is `one, one-minus-source-alpha` -- which is the state that composes
   * correctly when quads overlap, where straight alpha darkens every seam. */
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

  /* ONE WHITE TEXEL SO *NO ATLAS* IS A VALUE AND NOT A BRANCH. A binding left empty is a validation
   * error on some drivers and a black interface on others, and both are worse than four bytes. */
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
  /* THE BUFFER GROWS AND NEVER SHRINKS, up to the bound. A frame with fewer rectangles than the last
   * reuses what is there, so a HUD whose length varies allocates during the first frames of a run and
   * on no frame after that -- which is what *the frame path does not allocate* costs to be true. */
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
    float Pad[2];
  } frame{{(float)WidthPx, (float)HeightPx}, {0.0f, 0.0f}};
  SDL_PushGPUVertexUniformData(into.Commands, 0, &frame, sizeof frame);

  SDL_BindGPUGraphicsPipeline(into.Pass, Pipe.Get());
  SDL_GPUBufferBinding binding{Verts.Get(), 0};
  SDL_BindGPUVertexBuffers(into.Pass, 0, &binding, 1);
  SDL_GPUTextureSamplerBinding sampled{Atlas.Get(), Smooth};
  SDL_BindGPUFragmentSamplers(into.Pass, 0, &sampled, 1);
  SDL_DrawGPUPrimitives(into.Pass, 6, Count, 0, 0);
}

} // namespace outshine::Render
