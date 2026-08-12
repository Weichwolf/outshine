#include "SubjectDraw.h"

#include <string>

#include "SceneTargets.h"
#include "SurfaceState.h"

namespace outshine::Render {

namespace {

/* THE SAME DERIVATION TABLE EVERY OTHER DRAW USES. A glTF subject is opaque, and its winding is
 * TRUSTED because the format defines one: the front face is counter-clockwise and
 * `Gltf::Subject::Indices` has already restated a mirroring node's order, so a face that turns away
 * is a back face and not an accident of how the file was authored. This is the opposite case from an
 * OSM ring, which arrives wound either way and is `Winding::Unknown` for it. */
constexpr SurfaceState kSubjectState = StateOf(Material{});
static_assert(kSubjectState.CullsBack(), "an opaque subject culls its back faces");

/* WHICH SCREEN-SPACE ORIENTATION glTF's FRONT FACE ARRIVES IN. MEASURED, not derived: the chain is
 * two flips and not one -- clip-space +Y is up while the framebuffer's runs down, and the eye basis
 * puts the view along -Z so the projection's own w is -z_eye -- and they cancel, leaving glTF's
 * counter-clockwise front face counter-clockwise on the target. The derivation that counted one flip
 * was written here first and culled every pixel of `render/coverage/quad`. */
constexpr wgpu::FrontFace kGltfFrontFace = wgpu::FrontFace::CCW;

} // namespace

/* TWO SHADERS AND ONE UNIFORM. The textured one multiplies the sampled base colour into the same
 * declared radiance the plain one emits, which is what the metal-rough model reduces to for a
 * dielectric at metalness 0 under a uniform environment: `baseColour(u,v) * factor * L`, flat, no
 * direction. THE TAP IS ALREADY LINEAR -- the texture is bound through an sRGB VIEW, so the hardware
 * sampler undoes the transfer per texel BEFORE it filters, and nothing here decodes anything.
 *
 * THE RADIANCE IS FLAT-INTERPOLATED. Every vertex of one part carries the same value, and barycentric
 * interpolation of three equal floats is a weighted sum that need not return them; `flat` takes the
 * provoking vertex verbatim, so a declared value arrives as itself. */
static const char *kSubjectWGSL = R"(
struct S { mvp : mat4x4f, anc : vec4f };
@group(0) @binding(0) var<uniform> s : S;

struct SOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f,
              @location(1) @interpolate(flat) emitted : vec3f };

@vertex fn vs(@location(0) p : vec3f, @location(2) emitted : vec3f) -> SOut {
  var o : SOut;
  o.pos = s.mvp * vec4f(p + s.anc.xyz, 1.0);
  o.uv = vec2f(0.0);
  o.emitted = emitted;
  return o;
}

@vertex fn vsTextured(@location(0) p : vec3f, @location(1) uv : vec2f,
                      @location(2) emitted : vec3f) -> SOut {
  var o : SOut;
  o.pos = s.mvp * vec4f(p + s.anc.xyz, 1.0);
  o.uv = uv;
  o.emitted = emitted;
  return o;
}

struct SFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
/* The declared radiance, not shaded. alpha is the direct fraction a display transfer weights its
 * occlusion by, and for a surface that emits what it was declared to emit all of it is direct. */
@fragment fn fs(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

static const char *kSubjectTexturedWGSL = R"(
@group(0) @binding(1) var baseColour : texture_2d<f32>;
@group(0) @binding(2) var baseSampler : sampler;

@fragment fn fsTextured(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted * textureSample(baseColour, baseSampler, in.uv).rgb, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

namespace {

wgpu::AddressMode AddressOf(SubjectWrap wrap) {
  switch (wrap) {
    case SubjectWrap::ClampToEdge: return wgpu::AddressMode::ClampToEdge;
    case SubjectWrap::MirroredRepeat: return wgpu::AddressMode::MirrorRepeat;
    case SubjectWrap::Repeat: return wgpu::AddressMode::Repeat;
  }
  return wgpu::AddressMode::Repeat;
}

wgpu::FilterMode FilterOf(SubjectFilter filter) {
  return filter == SubjectFilter::Nearest ? wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear;
}

} // namespace

void SubjectDraw::Configure(const Gpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  const std::string src =
      std::string(kVelocityWGSL) + kSubjectWGSL + kSubjectTexturedWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute position{};
  position.format = wgpu::VertexFormat::Float32x3;
  position.offset = 0;
  position.shaderLocation = 0;
  wgpu::VertexAttribute coordinate{};
  coordinate.format = wgpu::VertexFormat::Float32x2;
  coordinate.offset = 0;
  coordinate.shaderLocation = 1;
  wgpu::VertexAttribute radiance{};
  radiance.format = wgpu::VertexFormat::Float32x3;
  radiance.offset = 0;
  radiance.shaderLocation = 2;
  /* ONE BUFFER PER ATTRIBUTE AND NOT ONE INTERLEAVED STRIDE, because the subject's positions, its
   * uvs and its declared radiance come out of the consumer as separate runs and interleaving them
   * here would be a copy nobody asked for. The untextured pipeline has no uv slot at all rather than
   * an empty one -- a declared slot must be bound, and binding a buffer nothing reads is the kind of
   * placeholder this unit refuses everywhere else. */
  wgpu::VertexBufferLayout position_[1] = {};
  position_[0].arrayStride = 3 * sizeof(float);
  position_[0].attributeCount = 1;
  position_[0].attributes = &position;
  wgpu::VertexBufferLayout radiance_ = {};
  radiance_.arrayStride = 3 * sizeof(float);
  radiance_.attributeCount = 1;
  radiance_.attributes = &radiance;
  wgpu::VertexBufferLayout uv_ = {};
  uv_.arrayStride = 2 * sizeof(float);
  uv_.attributeCount = 1;
  uv_.attributes = &coordinate;
  const wgpu::VertexBufferLayout plainLayouts[2] = {position_[0], radiance_};
  const wgpu::VertexBufferLayout texturedLayouts[3] = {position_[0], uv_, radiance_};

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthCompare = wgpu::CompareFunction::Greater;
  ds.depthWriteEnabled = true;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(true)};

  /* THE BIND GROUP LAYOUT IS WRITTEN DOWN AND NOT DERIVED, because the two pipelines must share
   * ONE: an auto-derived layout reflects the entry point's own uses, so the untextured shader would
   * get a one-entry layout and the bind group built for the other would be rejected against it. */
  wgpu::BindGroupLayoutEntry bindings[3] = {};
  bindings[0].binding = 0;
  bindings[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  bindings[0].buffer.type = wgpu::BufferBindingType::Uniform;
  bindings[0].buffer.minBindingSize = kUniFloats * sizeof(float);
  bindings[1].binding = 1;
  bindings[1].visibility = wgpu::ShaderStage::Fragment;
  bindings[1].texture.sampleType = wgpu::TextureSampleType::Float;
  bindings[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  bindings[2].binding = 2;
  bindings[2].visibility = wgpu::ShaderStage::Fragment;
  bindings[2].sampler.type = wgpu::SamplerBindingType::Filtering;
  wgpu::BindGroupLayoutDescriptor bgl{};
  bgl.entryCount = 3;
  bgl.entries = bindings;
  Layout = Device.CreateBindGroupLayout(&bgl);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &Layout;
  const wgpu::PipelineLayout pipeline = Device.CreatePipelineLayout(&pld);

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = pipeline;
  rp.vertex.module = m;
  rp.vertex.entryPoint = "vs";
  rp.vertex.bufferCount = 2;
  rp.vertex.buffers = plainLayouts;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.entryPoint = "fs";
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  rp.primitive.frontFace = kGltfFrontFace;
  rp.primitive.cullMode =
      CullsBackFaces(kSubjectState, Winding::Trusted) ? wgpu::CullMode::Back : wgpu::CullMode::None;
  Plain = Device.CreateRenderPipeline(&rp);

  rp.vertex.entryPoint = "vsTextured";
  rp.vertex.bufferCount = 3;
  rp.vertex.buffers = texturedLayouts;
  fs.entryPoint = "fsTextured";
  Textured = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  /* ONE TEXEL OF WHITE, so the textured pipeline's bind group is complete before any case declares
   * an image. It is never a stand-in for a missing texture: the PLAIN pipeline is what draws an
   * untextured subject, and this is only ever bound under the textured one. */
  static const uint8_t white[4] = {255, 255, 255, 255};
  SubjectTexture blank{};
  blank.Rgba = white;
  blank.Width = 1;
  blank.Height = 1;
  SetTexture(blank);
}

void SubjectDraw::SetTexture(const SubjectTexture &texture) {
  if (!Device) return;
  const uint32_t width = texture.Width > 0 ? texture.Width : 1;
  const uint32_t height = texture.Height > 0 ? texture.Height : 1;

  wgpu::TextureDescriptor td{};
  td.size = {width, height, 1};
  /* THE STORAGE IS UNORM AND THE VIEW IS sRGB. The decode is therefore the sampler's, which puts it
   * before the filter -- `TextureLinearInterpolationTest`'s whole criterion -- and a shader-side
   * decode after the tap has no spelling in this unit. */
  td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  BaseColour = Device.CreateTexture(&td);
  BaseColourView = BaseColour.CreateView();

  static const uint8_t white[4] = {255, 255, 255, 255};
  const uint8_t *texels = texture.Rgba ? texture.Rgba : white;
  wgpu::TexelCopyTextureInfo destination{};
  destination.texture = BaseColour;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = width * 4;
  layout.rowsPerImage = height;
  wgpu::Extent3D extent{width, height, 1};
  Queue.WriteTexture(&destination, texels, (size_t)width * height * 4, &layout, &extent);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = AddressOf(texture.WrapU);
  sd.addressModeV = AddressOf(texture.WrapV);
  sd.magFilter = FilterOf(texture.Magnify);
  sd.minFilter = FilterOf(texture.Magnify);
  Samp = Device.CreateSampler(&sd);
  Rebind();
}

void SubjectDraw::Rebind() {
  if (!Layout || !Uni || !BaseColourView || !Samp) return;
  wgpu::BindGroupEntry entries[3] = {};
  entries[0].binding = 0;
  entries[0].buffer = Uni;
  entries[0].size = kUniFloats * sizeof(float);
  entries[1].binding = 1;
  entries[1].textureView = BaseColourView;
  entries[2].binding = 2;
  entries[2].sampler = Samp;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Layout;
  bg.entryCount = 3;
  bg.entries = entries;
  Bind = Device.CreateBindGroup(&bg);
}

void SubjectDraw::SetMesh(const float *verts, const float *uv, const float *emitted,
                          uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                          const double anchor[3]) {
  NVerts = nverts;
  NIdx = nidx;
  HasUv = uv != nullptr;
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = anchor[axis]; }
  if (nverts == 0 || nidx == 0 || !Device || !emitted) return;

  wgpu::BufferDescriptor vd{};
  vd.size = (uint64_t)nverts * 3 * sizeof(float);
  vd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  Vtx = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Vtx, 0, verts, (size_t)nverts * 3 * sizeof(float));

  Emit = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Emit, 0, emitted, (size_t)nverts * 3 * sizeof(float));

  if (HasUv) {
    wgpu::BufferDescriptor ud{};
    ud.size = (uint64_t)nverts * 2 * sizeof(float);
    ud.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Uv = Device.CreateBuffer(&ud);
    Queue.WriteBuffer(Uv, 0, uv, (size_t)nverts * 2 * sizeof(float));
  } else {
    Uv = wgpu::Buffer();
  }

  /* WriteBuffer copies in 4-byte units, so an odd index count is padded to keep the queue's own
   * alignment rule -- the draw still submits exactly `nidx`. */
  const uint64_t indexBytes = ((uint64_t)nidx * sizeof(uint32_t) + 3u) & ~uint64_t{3};
  wgpu::BufferDescriptor id{};
  id.size = indexBytes;
  id.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
  Idx = Device.CreateBuffer(&id);
  Queue.WriteBuffer(Idx, 0, idx, (size_t)nidx * sizeof(uint32_t));
}

void SubjectDraw::Encode(const FrameContext &ctx, ClusterCut &, wgpu::RenderPassEncoder &pass) {
  if (NIdx == 0 || !Vtx || !Idx || !Emit) return;
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  for (int i = 0; i < 3; i++) u[16 + i] = (float)(Anchor[i] - ctx.Eye[i]);
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  const bool textured = HasUv && Uv;
  pass.SetPipeline(textured ? Textured : Plain);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx);
  if (textured) { pass.SetVertexBuffer(1, Uv); }
  pass.SetVertexBuffer(textured ? 2u : 1u, Emit);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(NIdx);
}

} // namespace outshine::Render
