#include "SubjectDraw.h"

#include <cmath>
#include <string>

#include "SceneTargets.h"
#include "SurfaceState.h"

namespace outshine::Render {

namespace {

/* WHICH SCREEN-SPACE ORIENTATION glTF's FRONT FACE ARRIVES IN. MEASURED, not derived: the chain is
 * two flips and not one -- clip-space +Y is up while the framebuffer's runs down, and the eye basis
 * puts the view along -Z so the projection's own w is -z_eye -- and they cancel, leaving glTF's
 * counter-clockwise front face counter-clockwise on the target. The derivation that counted one flip
 * was written here first and culled every pixel of `render/coverage/quad`. */
constexpr wgpu::FrontFace kGltfFrontFace = wgpu::FrontFace::CCW;

/* A glTF subject's winding is TRUSTED because the format defines one: the front face is
 * counter-clockwise and `Gltf::Subject` has already restated a mirroring node's order, so a face
 * that turns away is a back face and not an accident of how the file was authored. This is the
 * opposite case from an OSM ring, which arrives wound either way and is `Winding::Unknown` for it. */
constexpr Winding kSubjectWinding = Winding::Trusted;

/* THE TRANSFER FUNCTION THE FORMAT DECLARES ITS BASE COLOUR IN (glTF 2.0, `baseColorTexture`:
 * "the values are sRGB encoded"), evaluated in double and stored as f32. The hardware sRGB view it
 * replaces carries about twelve bits of this curve where the formula carries twenty-four: at texel
 * code 1 the sampler returned 1/4096 = 2.4414e-4 against the exact 3.0353e-4, and that is what
 * `simple-texture` measured as 12 833 differing pixels. */
float LinearFromSrgb8(uint8_t code) {
  const float encoded = static_cast<float>(code) * (1.0f / 255.0f);
  if (encoded < 0.04045f) { return encoded * (1.0f / 12.92f); }
  return std::pow((encoded + 0.055f) * (1.0f / 1.055f), 2.4f);
}

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

const char *KindName(SurfaceKind kind) {
  switch (kind) {
    case SurfaceKind::Opaque: return "OPAQUE";
    case SurfaceKind::Masked: return "MASK";
    case SurfaceKind::Blended: return "BLEND";
    case SurfaceKind::ThinTransmissive: return "a thin transmissive sheet";
    case SurfaceKind::Refractive: return "a refracting volume";
  }
  return "an undeclared surface";
}

} // namespace

/* TWO SHADERS AND ONE UNIFORM. The textured one multiplies the sampled base colour into the same
 * declared radiance the plain one emits, which is what the metal-rough model reduces to for a
 * dielectric at metalness 0 under a uniform environment: `baseColour(u,v) * factor * L`, flat, no
 * direction. THE TAP IS ALREADY LINEAR -- the texture holds linear f32 decoded on the CPU, so the
 * sampler filters exact linear values and nothing here decodes anything.
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

void SubjectDraw::Configure(const Gpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  FiltersFloat32 = gpu.FiltersFloat32;

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
  rp.primitive.cullMode = CullsBackFaces(StateOf(Material{}), kSubjectWinding)
                              ? wgpu::CullMode::Back
                              : wgpu::CullMode::None;
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
}

/* THE BASE COLOUR, DECODED TO LINEAR f32 ON THE CPU. Alpha is not sRGB-encoded in glTF and crosses
 * unchanged; only the three colour channels carry the transfer. One texel of white where the surface
 * declares no image, so the textured pipeline's layout is satisfiable for every slot -- it is never
 * a stand-in for a missing texture, because the PLAIN pipeline is what draws a surface that declared
 * none and this is only ever sampled under the other. */
static void LinearRgba(const SubjectTexture &texture, uint32_t width, uint32_t height,
                       std::vector<float> &out) {
  static const uint8_t white[4] = {255, 255, 255, 255};
  const uint8_t *texels = texture.Rgba ? texture.Rgba : white;
  out.assign(static_cast<size_t>(width) * height * 4u, 0.0f);
  for (size_t texel = 0; texel < out.size() / 4u; ++texel) {
    for (size_t channel = 0; channel < 3; ++channel) {
      out[texel * 4u + channel] = LinearFromSrgb8(texels[texel * 4u + channel]);
    }
    out[texel * 4u + 3u] = static_cast<float>(texels[texel * 4u + 3u]) / 255.0f;
  }
}

void SubjectDraw::BindSurface(const SubjectMaterial &material) {
  const uint32_t width = material.BaseColour.Width > 0 ? material.BaseColour.Width : 1;
  const uint32_t height = material.BaseColour.Height > 0 ? material.BaseColour.Height : 1;
  std::vector<float> linear;
  LinearRgba(material.BaseColour, width, height, linear);

  wgpu::TextureDescriptor td{};
  td.size = {width, height, 1};
  td.format = wgpu::TextureFormat::RGBA32Float;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  wgpu::Texture image = Device.CreateTexture(&td);
  wgpu::TexelCopyTextureInfo destination{};
  destination.texture = image;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = width * 4u * sizeof(float);
  layout.rowsPerImage = height;
  wgpu::Extent3D extent{width, height, 1};
  Queue.WriteTexture(&destination, linear.data(), linear.size() * sizeof(float), &layout, &extent);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = AddressOf(material.BaseColour.WrapU);
  sd.addressModeV = AddressOf(material.BaseColour.WrapV);
  sd.magFilter = FilterOf(material.BaseColour.Magnify);
  sd.minFilter = FilterOf(material.BaseColour.Magnify);
  wgpu::Sampler sampler = Device.CreateSampler(&sd);

  wgpu::TextureView view = image.CreateView();
  wgpu::BindGroupEntry entries[3] = {};
  entries[0].binding = 0;
  entries[0].buffer = Uni;
  entries[0].size = kUniFloats * sizeof(float);
  entries[1].binding = 1;
  entries[1].textureView = view;
  entries[2].binding = 2;
  entries[2].sampler = sampler;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Layout;
  bg.entryCount = 3;
  bg.entries = entries;

  Binds.push_back(Device.CreateBindGroup(&bg));
  Images.push_back(image);
  Views.push_back(view);
  Samplers.push_back(sampler);
}

bool SubjectDraw::SetMaterials(const std::vector<SubjectMaterial> &materials, std::string &error) {
  Binds.clear();
  Images.clear();
  Views.clear();
  Samplers.clear();
  if (!Device) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  if (!FiltersFloat32) {
    error = "the device did not grant float32-filterable, and this unit's base colour is linear f32 "
            "so that the filter runs on exact linear values";
    return false;
  }
  for (size_t slot = 0; slot < materials.size(); ++slot) {
    if (materials[slot].Surface.Kind() != SurfaceKind::Opaque) {
      error = "surface slot " + std::to_string(slot) + " is " +
              KindName(materials[slot].Surface.Kind()) +
              ", and this unit has an opaque pipeline only -- the blended and masked pipelines are "
              "the next thing it owes, not something it silently draws opaque";
      Binds.clear();
      return false;
    }
    BindSurface(materials[slot]);
  }
  return true;
}

bool SubjectDraw::SetMesh(const SubjectMesh &mesh, std::string &error) {
  NVerts = mesh.VertexCount;
  NIdx = mesh.IndexCount;
  HasUv = mesh.Uv != nullptr;
  Batches.clear();
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = mesh.Anchor[axis]; }
  if (NVerts == 0 || NIdx == 0 || !Device || !mesh.Emitted || !mesh.Verts || !mesh.Indices ||
      !mesh.Draws) {
    NIdx = 0;
    return true;
  }
  for (const DrawBatch &batch : mesh.Draws->Batches()) {
    if (batch.MaterialSlot >= Binds.size()) {
      NIdx = 0;
      error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
              " over a table of " + std::to_string(Binds.size()) + " surfaces";
      return false;
    }
    if (batch.FirstIndex + batch.IndexCount > NIdx) {
      NIdx = 0;
      error = "a draw covers indices " + std::to_string(batch.FirstIndex) + " to " +
              std::to_string(batch.FirstIndex + batch.IndexCount) + " over a run of " +
              std::to_string(mesh.IndexCount);
      return false;
    }
  }
  Batches = mesh.Draws->Batches();

  wgpu::BufferDescriptor vd{};
  vd.size = (uint64_t)NVerts * 3 * sizeof(float);
  vd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  Vtx = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Vtx, 0, mesh.Verts, (size_t)NVerts * 3 * sizeof(float));

  Emit = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Emit, 0, mesh.Emitted, (size_t)NVerts * 3 * sizeof(float));

  if (HasUv) {
    wgpu::BufferDescriptor ud{};
    ud.size = (uint64_t)NVerts * 2 * sizeof(float);
    ud.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Uv = Device.CreateBuffer(&ud);
    Queue.WriteBuffer(Uv, 0, mesh.Uv, (size_t)NVerts * 2 * sizeof(float));
  } else {
    Uv = wgpu::Buffer();
  }

  /* WriteBuffer copies in 4-byte units, so an odd index count is padded to keep the queue's own
   * alignment rule -- the draws still submit exactly what their batches say. */
  const uint64_t indexBytes = ((uint64_t)NIdx * sizeof(uint32_t) + 3u) & ~uint64_t{3};
  wgpu::BufferDescriptor id{};
  id.size = indexBytes;
  id.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
  Idx = Device.CreateBuffer(&id);
  Queue.WriteBuffer(Idx, 0, mesh.Indices, (size_t)NIdx * sizeof(uint32_t));
  return true;
}

uint32_t SubjectDraw::DrawCount() const {
  uint32_t drawn = 0;
  for (const DrawBatch &batch : Batches) { drawn += batch.Draws; }
  return drawn;
}

void SubjectDraw::Encode(const FrameContext &ctx, ClusterCut &, wgpu::RenderPassEncoder &pass) {
  if (NIdx == 0 || Batches.empty() || !Vtx || !Idx || !Emit) { return; }
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  for (int i = 0; i < 3; i++) u[16 + i] = (float)(Anchor[i] - ctx.Eye[i]);
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  pass.SetVertexBuffer(0, Vtx);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);

  /* THE LIST IS ALREADY IN ORDER, so the encoder only notices where the state changes: the batcher
   * merged what shared a pipeline and a slot, and what is left is exactly the changes that had to
   * happen. */
  VertexLayout bound = VertexLayout::Position;
  bool anyBound = false;
  size_t boundSlot = 0;
  bool slotBound = false;
  for (const DrawBatch &batch : Batches) {
    const bool textured = batch.Layout == VertexLayout::PositionUv && HasUv && Uv;
    const VertexLayout wanted = textured ? VertexLayout::PositionUv : VertexLayout::Position;
    if (!anyBound || wanted != bound) {
      pass.SetPipeline(textured ? Textured : Plain);
      /* The two layouts put the radiance in different slots, so every slot the incoming layout
       * declares is rebound: leaving the other one's buffer where it was is how a uv slot ends up
       * holding radiance. */
      if (textured) { pass.SetVertexBuffer(1, Uv); }
      pass.SetVertexBuffer(textured ? 2u : 1u, Emit);
      bound = wanted;
      anyBound = true;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      pass.SetBindGroup(0, Binds[batch.MaterialSlot]);
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    pass.DrawIndexed(batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

} // namespace outshine::Render
