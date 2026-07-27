#include "FBHudStage.h"
#include "FBHudFont.h"
#include <cstdio>
#include <cstring>

namespace FlightBox {

/* Two pipelines sharing one pixel->NDC map. The fragment LINEARISES the colour so the sRGB swapchain
 * view re-encodes it to the intended display green. TODO: the 8-tap glow. */
static const char *kHudStrokeWGSL = R"(
struct HU { scale : vec4f };
@group(0) @binding(0) var<uniform> h : HU;
struct VO { @builtin(position) pos : vec4f, @location(0) d : f32, @location(1) hw : f32,
            @location(2) col : vec3f };
@vertex fn vs(@location(0) p : vec2f, @location(1) d : f32, @location(2) hw : f32,
              @location(3) c : vec3f) -> VO {
  var o : VO;
  o.pos = vec4f(p.x * h.scale.x - 1.0, 1.0 - p.y * h.scale.y, 0.0, 1.0);
  o.d = d;
  o.hw = hw;
  o.col = c;
  return o;
}
/* Analytic box-filter coverage across the stroke's width: 1 at the centreline, ramping to 0 over the
 * 1px band straddling the nominal edge |d|==hw (see FBHudGeometry.cpp's AppendStroke). A pixel-aligned
 * 1px line (hw=0.5) renders exactly as the old hard LineList did; any other angle/width gets a smooth
 * edge instead of a staircase. Caps are the quad's own (butt) ends -- no separate longitudinal term. */
@fragment fn fs(in : VO) -> @location(0) vec4f {
  let alpha = clamp(in.hw + 0.5 - abs(in.d), 0.0, 1.0);
  if (alpha <= 0.0) { discard; }
  return vec4f(pow(in.col, vec3f(2.2)), alpha);
}
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
/* Coverage antialiasing: reconstruct the ROM bit's true screen-pixel coverage via a "sharp bilinear"
 * warp of the sample point (texel-space fraction rescaled to the fwidth() footprint, then clamped) --
 * a box-filter approximation of the ideal analytic edge, not a smoothing blur. At footprint == 1 texel
 * this is the identity (plain bilinear); magnified, it snaps flat except for a ~1-screen-pixel-wide
 * ramp right at each bit edge (the atlas gutter guarantees that ramp only ever sees this glyph's own
 * data). Straight alpha out -- no more hard alpha-test discard. */
@fragment fn fs(in : VO) -> @location(0) vec4f {
  let texSize = vec2f(textureDimensions(atlas));
  let t = in.uv * texSize;
  let fw = max(fwidth(t), vec2f(1e-4));
  let c = floor(t - 0.5) + 0.5;
  let f = clamp((t - c - 0.5) / fw + 0.5, vec2f(0.0), vec2f(1.0));
  let coverage = textureSampleLevel(atlas, samp, (c + f) / texSize, 0.0).r;
  if (coverage <= 0.0) { discard; }
  return vec4f(pow(in.col, vec3f(2.2)), coverage);
}
)";

void FBHudStage::Init(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  /* r8unorm, LINEAR — the fragment shader turns that into coverage AA. The gutter texels stay at
   * their zero-init value. */
  const uint32_t AW = (uint32_t)kFontAtlasW, AH = (uint32_t)kFontAtlasH;
  std::vector<uint8_t> atlas((size_t)AW * AH, 0);
  for (int gi = 0; gi < kFontGlyphs; gi++)
    for (int row = 0; row < kFontTile; row++)
      for (int c = 0; c < kFontTile; c++)
        atlas[(size_t)(row + 1) * AW + gi * kFontTilePad + 1 + c] = kFontGlyphRom[gi][row][c];
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
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  Samp = Device.CreateSampler(&sd);

  wgpu::BufferDescriptor bd{};
  bd.size = 16;   /* vec4 scale (xy used) */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);
  float scale[4] = {2.0f / gpu.Width, 2.0f / gpu.Height, 0, 0};
  Queue.WriteBuffer(Uni, 0, scale, sizeof scale);

  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  bd.size = kHudMaxStrokeFloats * sizeof(float);   /* stroke verts (x,y,d,hw,r,g,b) */
  StrokeVtx = Device.CreateBuffer(&bd);
  bd.size = kHudMaxTextFloats * sizeof(float);   /* glyph verts (x,y,u,v,r,g,b) */
  TextVtx = Device.CreateBuffer(&bd);
  LoadingGlyphs.reserve(kHudMaxTextFloats);

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

  /* pos2+d1+hw1+col3, stride 28. TriangleList only: every straight segment is one AA quad. */
  {
    wgpu::ShaderModule sm = mkmod(kHudStrokeWGSL);
    wgpu::VertexAttribute attrs[4] = {};
    attrs[0].format = wgpu::VertexFormat::Float32x2; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
    attrs[1].format = wgpu::VertexFormat::Float32;   attrs[1].offset = 8;  attrs[1].shaderLocation = 1;
    attrs[2].format = wgpu::VertexFormat::Float32;   attrs[2].offset = 12; attrs[2].shaderLocation = 2;
    attrs[3].format = wgpu::VertexFormat::Float32x3; attrs[3].offset = 16; attrs[3].shaderLocation = 3;
    wgpu::VertexBufferLayout vbl{};
    vbl.arrayStride = 28;
    vbl.attributeCount = 4;
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
    StrokePipe = Device.CreateRenderPipeline(&rp);
    wgpu::BindGroupEntry be{};
    be.binding = 0; be.buffer = Uni; be.size = 16;
    wgpu::BindGroupDescriptor bg{};
    bg.layout = StrokePipe.GetBindGroupLayout(0);
    bg.entryCount = 1;
    bg.entries = &be;
    StrokeBind = Device.CreateBindGroup(&bg);
  }
  /* pos2+uv2+col3, stride 28. */
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

void FBHudStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  Geometry.Reset();
  if (Disp) {
    FBHudEnv env{ctx.Width, ctx.Height, Agl, Have};
    Disp->BuildHud(State, env, Geometry);
  }
  const std::vector<float> &strokes = Geometry.Strokes();
  const std::vector<float> &glyphs = Geometry.Glyphs();

  if (!strokes.empty()) Queue.WriteBuffer(StrokeVtx, 0, strokes.data(), strokes.size() * sizeof(float));
  if (!glyphs.empty()) Queue.WriteBuffer(TextVtx, 0, glyphs.data(), glyphs.size() * sizeof(float));

  if (!strokes.empty()) {
    pass.SetPipeline(StrokePipe);
    pass.SetBindGroup(0, StrokeBind);
    pass.SetVertexBuffer(0, StrokeVtx);
    pass.Draw((uint32_t)(strokes.size() / 7));
  }
  if (!glyphs.empty()) {
    pass.SetPipeline(TextPipe);
    pass.SetBindGroup(0, TextBind);
    pass.SetVertexBuffer(0, TextVtx);
    pass.Draw((uint32_t)(glyphs.size() / 7));
  }
}

void FBHudStage::EncodeLoadingText(wgpu::RenderPassEncoder &pass, int width, int height, float pct,
                                   int ready, int total) {
  LoadingGlyphs.clear();
  char msg[64], cnt[64];
  snprintf(msg, sizeof msg, "LOADING TERRAIN %d PCT", (int)(pct * 100.0f + 0.5f));
  snprintf(cnt, sizeof cnt, "%d / %d TILES", ready, total);
  float s = 4.0f;
  FBHudFontAppendText(LoadingGlyphs, (float)width * 0.5f - (float)strlen(msg) * kFontAdvance * s * 0.5f,
                     (float)height * 0.5f - kFontQuadSize * s, s, 0.20f, 1.00f, 0.40f, msg);
  float cs = s * 0.6f;
  FBHudFontAppendText(LoadingGlyphs, (float)width * 0.5f - (float)strlen(cnt) * kFontAdvance * cs * 0.5f,
                     (float)height * 0.5f + kFontQuadSize * s * 1.4f, cs, 0.45f, 0.80f, 0.50f, cnt);
  if (!LoadingGlyphs.empty()) {
    Queue.WriteBuffer(TextVtx, 0, LoadingGlyphs.data(), LoadingGlyphs.size() * sizeof(float));
    pass.SetPipeline(TextPipe);
    pass.SetBindGroup(0, TextBind);
    pass.SetVertexBuffer(0, TextVtx);
    pass.Draw((uint32_t)(LoadingGlyphs.size() / 7));
  }
}

} // namespace FlightBox
