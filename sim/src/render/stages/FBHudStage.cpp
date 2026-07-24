#include "FBHudStage.h"
#include "FBHud.h"   /* reused HUD symbology (w3_build_hud) + MAX7456 atlas; GL backend stubbed. THE
                      * one TU that includes this — see the header banner. */
#include <cstdio>
#include <cstring>

namespace FlightBox {

/* HUD overlay: geometry comes from the REUSED symbology (FBHud.h -> w3_build_hud), which fills
 * w3_hud (lines, x,y,r,g,b), w3_hudT (AA triangles), and mx_v (textured glyphs, x,y,u,v,r,g,b) in 2D
 * pixel coords. Three pipelines share the pixel->NDC map; the fragment linearises the colour so the
 * sRGB swapchain view re-encodes it to the intended display green. TODO: the 8-tap present.h glow. */
static const char *kHudSolidWGSL = R"(
struct HU { scale : vec4f };
@group(0) @binding(0) var<uniform> h : HU;
struct VO { @builtin(position) pos : vec4f, @location(0) col : vec3f };
@vertex fn vs(@location(0) p : vec2f, @location(1) c : vec3f) -> VO {
  var o : VO;
  o.pos = vec4f(p.x * h.scale.x - 1.0, 1.0 - p.y * h.scale.y, 0.0, 1.0);
  o.col = c;
  return o;
}
@fragment fn fs(in : VO) -> @location(0) vec4f { return vec4f(pow(in.col, vec3f(2.2)), 1.0); }
)";
static const char *kHudTextWGSL = R"(
struct HU { scale : vec4f };
@group(0) @binding(0) var<uniform> h : HU;
@group(0) @binding(1) var samp : sampler;
@group(0) @binding(2) var atlas : texture_2d<f32>;
struct VO { @builtin(position) pos : vec4f, @location(0) uv : vec2f, @location(1) col : vec3f };
@vertex fn vs(@location(0) p : vec2f, @location(1) uv : vec2f, @location(2) c : vec3f) -> VO {
  var o : VO;
  o.pos = vec4f(p.x * h.scale.x - 1.0, 1.0 - p.y * h.scale.y, 0.0, 1.0);
  o.uv = uv;
  o.col = c;
  return o;
}
@fragment fn fs(in : VO) -> @location(0) vec4f {
  if (textureSampleLevel(atlas, samp, in.uv, 0.0).r < 0.5) { discard; }
  return vec4f(pow(in.col, vec3f(2.2)), 1.0);
}
)";

void FBHudStage::Init(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  /* MAX7456 font atlas: MX_NGLYPH tiles of 8x8, one byte per row, bit7 = leftmost. r8unorm, NEAREST
   * (crisp blocky chip look). Built from the SAME MX_FONT ROM the WebGL path uses. */
  const uint32_t AW = (uint32_t)MX_ATLAS_W, AH = (uint32_t)MX_TILE;
  std::vector<uint8_t> atlas((size_t)AW * AH, 0);
  for (int gi = 0; gi < MX_NGLYPH; gi++)
    for (int row = 0; row < MX_TILE; row++)
      for (int c = 0; c < MX_TILE; c++)
        atlas[(size_t)row * AW + gi * MX_TILE + c] = (MX_FONT[gi][row] & (0x80 >> c)) ? 255 : 0;
  wgpu::TextureDescriptor td{};
  td.size = {AW, AH, 1};
  td.format = wgpu::TextureFormat::R8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Atlas = Device.CreateTexture(&td);
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = Atlas;
  wgpu::TexelCopyBufferLayout lay{};
  lay.bytesPerRow = AW;
  lay.rowsPerImage = AH;
  wgpu::Extent3D ext{AW, AH, 1};
  Queue.WriteTexture(&dst, atlas.data(), atlas.size(), &lay, &ext);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = wgpu::FilterMode::Nearest;
  sd.minFilter = wgpu::FilterMode::Nearest;
  Samp = Device.CreateSampler(&sd);

  wgpu::BufferDescriptor bd{};
  bd.size = 16;   /* vec4 scale (xy used) */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);
  float scale[4] = {2.0f / gpu.Width, 2.0f / gpu.Height, 0, 0};
  Queue.WriteBuffer(Uni, 0, scale, sizeof scale);

  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  bd.size = sizeof w3_hud;   /* line verts (x,y,r,g,b) */
  LineVtx = Device.CreateBuffer(&bd);
  bd.size = sizeof w3_hudT;  /* AA triangle verts */
  TriVtx = Device.CreateBuffer(&bd);
  bd.size = sizeof mx_v;     /* glyph verts (x,y,u,v,r,g,b) */
  TextVtx = Device.CreateBuffer(&bd);

  wgpu::BlendState blend{};
  blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;
  blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;
  ct.blend = &blend;

  auto mkmod = [&](const char *code) {
    wgpu::ShaderSourceWGSL w{};
    w.code = code;
    wgpu::ShaderModuleDescriptor smd{};
    smd.nextInChain = &w;
    return Device.CreateShaderModule(&smd);
  };

  /* solid pipeline (pos2+col3, stride 20) — one module, two topologies (tris + lines). An EXPLICIT
   * shared layout so one bind group serves both pipelines (auto layouts are per-pipeline objects). */
  {
    wgpu::ShaderModule sm = mkmod(kHudSolidWGSL);
    wgpu::BindGroupLayoutEntry ble{};
    ble.binding = 0;
    ble.visibility = wgpu::ShaderStage::Vertex;
    ble.buffer.type = wgpu::BufferBindingType::Uniform;
    wgpu::BindGroupLayoutDescriptor bld{};
    bld.entryCount = 1;
    bld.entries = &ble;
    wgpu::BindGroupLayout bgl = Device.CreateBindGroupLayout(&bld);
    wgpu::PipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 1;
    pld.bindGroupLayouts = &bgl;
    wgpu::PipelineLayout pl = Device.CreatePipelineLayout(&pld);
    wgpu::VertexAttribute attrs[2] = {};
    attrs[0].format = wgpu::VertexFormat::Float32x2; attrs[0].offset = 0; attrs[0].shaderLocation = 0;
    attrs[1].format = wgpu::VertexFormat::Float32x3; attrs[1].offset = 8; attrs[1].shaderLocation = 1;
    wgpu::VertexBufferLayout vbl{};
    vbl.arrayStride = 20;
    vbl.attributeCount = 2;
    vbl.attributes = attrs;
    wgpu::RenderPipelineDescriptor rp{};
    rp.layout = pl;
    rp.vertex.module = sm;
    rp.vertex.bufferCount = 1;
    rp.vertex.buffers = &vbl;
    wgpu::FragmentState fs{};
    fs.module = sm;
    fs.targetCount = 1;
    fs.targets = &ct;
    rp.fragment = &fs;
    rp.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    SolidPipe = Device.CreateRenderPipeline(&rp);
    rp.primitive.topology = wgpu::PrimitiveTopology::LineList;
    LinePipe = Device.CreateRenderPipeline(&rp);
    wgpu::BindGroupEntry be{};
    be.binding = 0; be.buffer = Uni; be.size = 16;
    wgpu::BindGroupDescriptor bg{};
    bg.layout = bgl;
    bg.entryCount = 1;
    bg.entries = &be;
    SolidBind = Device.CreateBindGroup(&bg);
  }
  /* text pipeline (pos2+uv2+col3, stride 28) — samples the atlas, alpha-tests. */
  {
    wgpu::ShaderModule sm = mkmod(kHudTextWGSL);
    wgpu::VertexAttribute attrs[3] = {};
    attrs[0].format = wgpu::VertexFormat::Float32x2; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
    attrs[1].format = wgpu::VertexFormat::Float32x2; attrs[1].offset = 8;  attrs[1].shaderLocation = 1;
    attrs[2].format = wgpu::VertexFormat::Float32x3; attrs[2].offset = 16; attrs[2].shaderLocation = 2;
    wgpu::VertexBufferLayout vbl{};
    vbl.arrayStride = 28;
    vbl.attributeCount = 3;
    vbl.attributes = attrs;
    wgpu::RenderPipelineDescriptor rp{};
    rp.vertex.module = sm;
    rp.vertex.bufferCount = 1;
    rp.vertex.buffers = &vbl;
    wgpu::FragmentState fs{};
    fs.module = sm;
    fs.targetCount = 1;
    fs.targets = &ct;
    rp.fragment = &fs;
    rp.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    TextPipe = Device.CreateRenderPipeline(&rp);
    wgpu::BindGroupEntry be[3] = {};
    be[0].binding = 0; be[0].buffer = Uni; be[0].size = 16;
    be[1].binding = 1; be[1].sampler = Samp;
    be[2].binding = 2; be[2].textureView = Atlas.CreateView();
    wgpu::BindGroupDescriptor bg{};
    bg.layout = TextPipe.GetBindGroupLayout(0);
    bg.entryCount = 3;
    bg.entries = be;
    TextBind = Device.CreateBindGroup(&bg);
  }
}

void FBHudStage::SetGroundMode(int photo) {
  w3_ground.mode = photo ? W3_GROUND_PHOTO : W3_GROUND_OSM;
}

void FBHudStage::SetAgl(float agl) { w3_agl = agl; }

void FBHudStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  w3_build_hud(&State, ctx.Width, ctx.Height, Have ? 1 : 0);
  if (w3_hudTN > 0) Queue.WriteBuffer(TriVtx, 0, w3_hudT, (size_t)w3_hudTN * sizeof(float));
  if (w3_hudN > 0) Queue.WriteBuffer(LineVtx, 0, w3_hud, (size_t)w3_hudN * sizeof(float));
  if (mx_vN > 0) Queue.WriteBuffer(TextVtx, 0, mx_v, (size_t)mx_vN * sizeof(float));

  if (w3_hudTN > 0) {
    pass.SetPipeline(SolidPipe);
    pass.SetBindGroup(0, SolidBind);
    pass.SetVertexBuffer(0, TriVtx);
    pass.Draw((uint32_t)(w3_hudTN / 5));
  }
  if (w3_hudN > 0) {
    pass.SetPipeline(LinePipe);
    pass.SetBindGroup(0, SolidBind);
    pass.SetVertexBuffer(0, LineVtx);
    pass.Draw((uint32_t)(w3_hudN / 5));
  }
  if (mx_vN > 0) {
    pass.SetPipeline(TextPipe);
    pass.SetBindGroup(0, TextBind);
    pass.SetVertexBuffer(0, TextVtx);
    pass.Draw((uint32_t)(mx_vN / 7));
  }
}

void FBHudStage::EncodeLoadingText(wgpu::RenderPassEncoder &pass, int width, int height, float pct,
                                   int ready, int total) {
  mx_reset();
  char msg[64], cnt[64];
  snprintf(msg, sizeof msg, "LOADING TERRAIN %d PCT", (int)(pct * 100.0f + 0.5f));
  snprintf(cnt, sizeof cnt, "%d / %d TILES", ready, total);
  float s = 4.0f;
  mx_text((float)width * 0.5f - (float)strlen(msg) * MX_ADV * s * 0.5f, (float)height * 0.5f - MX_QS * s,
          s, 0.20f, 1.00f, 0.40f, msg);
  float cs = s * 0.6f;
  mx_text((float)width * 0.5f - (float)strlen(cnt) * MX_ADV * cs * 0.5f, (float)height * 0.5f + MX_QS * s * 1.4f,
          cs, 0.45f, 0.80f, 0.50f, cnt);
  if (mx_vN > 0) Queue.WriteBuffer(TextVtx, 0, mx_v, (size_t)mx_vN * sizeof(float));
  if (mx_vN > 0) {
    pass.SetPipeline(TextPipe);
    pass.SetBindGroup(0, TextBind);
    pass.SetVertexBuffer(0, TextVtx);
    pass.Draw((uint32_t)(mx_vN / 7));
  }
}

} // namespace FlightBox
