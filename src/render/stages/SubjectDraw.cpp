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

/* SOURCE-OVER WITH STRAIGHT ALPHA, which is the one convention both sides of the comparison are
 * stated in. The alpha channel accumulates coverage the same way -- `a + dst*(1-a)` -- so a stack of
 * blended surfaces reports how much of the pixel they cover between them rather than only the last
 * one's contribution. */
wgpu::BlendState OverBlend() {
  wgpu::BlendState blend{};
  blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
  blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.color.operation = wgpu::BlendOperation::Add;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;
  blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.alpha.operation = wgpu::BlendOperation::Add;
  return blend;
}

/* The one place a surface kind becomes a shader entry point. `SetMaterials` refuses the two kinds
 * this unit builds no pipeline for, so the arms below cover every kind that reaches a draw. */
const char *FragmentEntryPoint(SurfaceKind kind, bool textured) {
  switch (kind) {
    case SurfaceKind::Masked: return textured ? "fsMaskedTextured" : "fsMasked";
    case SurfaceKind::Blended: return textured ? "fsBlendedTextured" : "fsBlended";
    case SurfaceKind::Opaque:
    case SurfaceKind::ThinTransmissive:
    case SurfaceKind::Refractive: break;
  }
  return textured ? "fsTextured" : "fs";
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

/* TWO VERTEX ENTRY POINTS AND SIX FRAGMENT ONES: two layouts times the three answers glTF's
 * `alphaMode` can give. The textured arms multiply the sampled colour into the same declared radiance
 * the plain ones emit, which is what the metal-rough model reduces to for a dielectric at metalness 0
 * under a uniform environment: `colour(u,v) * factor * L`, flat, no direction. THE TAP IS ALREADY
 * LINEAR -- the texture holds linear f32 decoded on the CPU, so the sampler filters exact linear
 * values and nothing here decodes anything.
 *
 * THE RADIANCE IS FLAT-INTERPOLATED. Every vertex of one part carries the same value, and barycentric
 * interpolation of three equal floats is a weighted sum that need not return them; `flat` takes the
 * provoking vertex verbatim, so a declared value arrives as itself.
 *
 * THE OPAQUE ARMS READ `.rgb` AND NEVER `.a`, which is glTF's rule stated in the only place that can
 * hold it (`Specification.adoc:2178`: "the rendered output is fully opaque and any alpha value is
 * ignored"). Multiplying the sampled alpha into the colour there is the premultiplication
 * `AlphaBlendModeTest` puts a black box beside the word "Opaque" for. */
static const char *kSubjectWGSL = R"(
struct S { mvp : mat4x4f, anc : vec4f };
@group(0) @binding(0) var<uniform> s : S;
/* The two per-slot numbers: glTF's `baseColorFactor.a` and, under MASK, its `alphaCutoff`. */
struct A { factor : f32, cut : f32, pad0 : f32, pad1 : f32 };
@group(0) @binding(3) var<uniform> alpha : A;

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
/* The declared radiance, not shaded. On an opaque arm the fourth channel is the direct fraction a
 * display transfer weights its occlusion by, and for a surface that emits what it was declared to
 * emit all of it is direct. */
@fragment fn fs(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsMasked(in : SOut) -> SFrag {
  if (alpha.factor < alpha.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(in.emitted, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

@fragment fn fsBlended(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted, alpha.factor);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

static const char *kSubjectTexturedWGSL = R"(
@group(0) @binding(1) var colourMap : texture_2d<f32>;
@group(0) @binding(2) var colourSampler : sampler;

@fragment fn fsTextured(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(in.emitted * textureSample(colourMap, colourSampler, in.uv).rgb, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

/* Greater-or-equal is kept, less-than discards: glTF states the surviving side, not the cut side
 * ("If the alpha value is greater than or equal to the alphaCutoff value then it is rendered as
 * fully opaque"), and the two differ on exactly the texels a linear ramp puts at the cutoff. */
@fragment fn fsMaskedTextured(in : SOut) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  if (alpha.factor * tap.a < alpha.cut) { discard; }
  var o : SFrag;
  o.col = vec4f(in.emitted * tap.rgb, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}

/* STRAIGHT, NOT PREMULTIPLIED: the colour goes out unweighted and the blend state applies the
 * weight, so the same fragment function would be correct under either convention only by accident.
 * The one convention this whole comparison is stated in is straight alpha. */
@fragment fn fsBlendedTextured(in : SOut) -> SFrag {
  let tap = textureSample(colourMap, colourSampler, in.uv);
  var o : SFrag;
  o.col = vec4f(in.emitted * tap.rgb, alpha.factor * tap.a);
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

  /* THE BIND GROUP LAYOUT IS WRITTEN DOWN AND NOT DERIVED, because every pipeline must share ONE:
   * an auto-derived layout reflects the entry point's own uses, so the untextured shader would get
   * a two-entry layout and the bind group built for the others would be rejected against it. */
  wgpu::BindGroupLayoutEntry bindings[4] = {};
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
  bindings[3].binding = 3;
  bindings[3].visibility = wgpu::ShaderStage::Fragment;
  bindings[3].buffer.type = wgpu::BufferBindingType::Uniform;
  bindings[3].buffer.minBindingSize = kAlphaFloats * sizeof(float);
  wgpu::BindGroupLayoutDescriptor bgl{};
  bgl.entryCount = 4;
  bgl.entries = bindings;
  Layout = Device.CreateBindGroupLayout(&bgl);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &Layout;
  const wgpu::PipelineLayout pipeline = Device.CreatePipelineLayout(&pld);

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = pipeline;
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 2;
  rp.fragment = &fs;
  rp.primitive.frontFace = kGltfFrontFace;

  /* TWELVE PIPELINES: two vertex layouts, two facings, three alpha modes. The facing is the SLOT's
   * because glTF states it per material -- `TextureSettingsTest` hides a green checkmark behind a
   * polygon facing the wrong way and lets the flag decide which of the two is seen, and one cull
   * mode for the whole subject draws the wrong cell whichever way it is set. */
  for (const SurfaceKind kind : {SurfaceKind::Opaque, SurfaceKind::Masked, SurfaceKind::Blended}) {
    const bool blends = kind == SurfaceKind::Blended;
    wgpu::ColorTargetState ct{};
    ct.format = gpu.HdrFormat;
    const wgpu::BlendState over = OverBlend();
    if (blends) { ct.blend = &over; }
    /* A BLENDED SURFACE WRITES NO VELOCITY, and that follows from its writing no depth rather than
     * being a second decision: the temporal resolve reprojects a pixel through the depth that was
     * written there, so a surface that left none has no motion to claim and would overwrite the
     * motion of whatever did. */
    wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(!blends)};
    wgpu::DepthStencilState ds{};
    ds.format = wgpu::TextureFormat::Depth32Float;
    ds.depthCompare = wgpu::CompareFunction::Greater;
    /* `MASK` writes depth like any solid surface -- Khronos says so in the asset's own words,
     * "Depth buffering with writes enabled may be used in MASK mode" -- because a kept fragment is
     * fully opaque. `BLEND` cannot: it composites with what is behind it, so writing the depth that
     * would hide that is the one thing it must not do (`core/SurfaceState.h` states the same pair). */
    ds.depthWriteEnabled = !blends;
    rp.depthStencil = &ds;
    fs.targets = cts;

    for (const VertexLayout layout : {VertexLayout::Position, VertexLayout::PositionUv}) {
      const bool textured = layout == VertexLayout::PositionUv;
      rp.vertex.entryPoint = textured ? "vsTextured" : "vs";
      rp.vertex.bufferCount = textured ? 3 : 2;
      rp.vertex.buffers = textured ? texturedLayouts : plainLayouts;
      fs.entryPoint = FragmentEntryPoint(kind, textured);
      for (const bool cullsBack : {false, true}) {
        rp.primitive.cullMode = cullsBack ? wgpu::CullMode::Back : wgpu::CullMode::None;
        Pipelines[PipelineAt(layout, kind, cullsBack)] = Device.CreateRenderPipeline(&rp);
      }
    }
  }

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);
}

size_t SubjectDraw::PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack) {
  const size_t at = static_cast<size_t>(kind);
  return ((layout == VertexLayout::PositionUv ? 1u : 0u) * 2u + (cullsBack ? 1u : 0u)) *
             kSurfaceKinds +
         at;
}

/* THE COLOUR IMAGE, DECODED TO LINEAR f32 ON THE CPU. Alpha is not sRGB-encoded in glTF and crosses
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
  const uint32_t width = material.Colour.Width > 0 ? material.Colour.Width : 1;
  const uint32_t height = material.Colour.Height > 0 ? material.Colour.Height : 1;
  std::vector<float> linear;
  LinearRgba(material.Colour, width, height, linear);

  SurfaceSlot slot;
  slot.Kind = material.Surface.Kind();
  slot.CullsBack = CullsBackFaces(material.Surface, kSubjectWinding);

  wgpu::TextureDescriptor td{};
  td.size = {width, height, 1};
  td.format = wgpu::TextureFormat::RGBA32Float;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  slot.Image = Device.CreateTexture(&td);
  wgpu::TexelCopyTextureInfo destination{};
  destination.texture = slot.Image;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = width * 4u * sizeof(float);
  layout.rowsPerImage = height;
  wgpu::Extent3D extent{width, height, 1};
  Queue.WriteTexture(&destination, linear.data(), linear.size() * sizeof(float), &layout, &extent);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = AddressOf(material.Colour.WrapU);
  sd.addressModeV = AddressOf(material.Colour.WrapV);
  sd.magFilter = FilterOf(material.Colour.Magnify);
  sd.minFilter = FilterOf(material.Colour.Magnify);
  slot.Sample = Device.CreateSampler(&sd);
  slot.View = slot.Image.CreateView();

  const float alpha[kAlphaFloats] = {material.Coverage, material.Surface.CoverageCut(), 0.0f, 0.0f};
  wgpu::BufferDescriptor ad{};
  ad.size = sizeof alpha;
  ad.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  slot.Alpha = Device.CreateBuffer(&ad);
  Queue.WriteBuffer(slot.Alpha, 0, alpha, sizeof alpha);

  wgpu::BindGroupEntry entries[4] = {};
  entries[0].binding = 0;
  entries[0].buffer = Uni;
  entries[0].size = kUniFloats * sizeof(float);
  entries[1].binding = 1;
  entries[1].textureView = slot.View;
  entries[2].binding = 2;
  entries[2].sampler = slot.Sample;
  entries[3].binding = 3;
  entries[3].buffer = slot.Alpha;
  entries[3].size = sizeof alpha;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Layout;
  bg.entryCount = 4;
  bg.entries = entries;
  slot.Bind = Device.CreateBindGroup(&bg);

  Slots.push_back(std::move(slot));
}

bool SubjectDraw::SetMaterials(const std::vector<SubjectMaterial> &materials, std::string &error) {
  /* A NEW TABLE RETIRES THE OLD LIST, because a batch names a slot INDEX and the indices it names
   * belonged to the table being replaced. `SetMesh` is what re-validates them, so until it runs
   * there is no list -- which is what makes "encode a batch against a table that no longer holds its
   * slot" unspellable rather than a bounds check nobody reaches. */
  Slots.clear();
  Batches.clear();
  NIdx = 0;
  if (!Device) {
    error = "the subject unit has no device, so no surface can be bound";
    return false;
  }
  if (!FiltersFloat32) {
    error = "the device did not grant float32-filterable, and this unit's colour image is linear "
            "f32 so that the filter runs on exact linear values";
    return false;
  }
  for (size_t slot = 0; slot < materials.size(); ++slot) {
    const SurfaceKind kind = materials[slot].Surface.Kind();
    if (kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive) {
      error = "surface slot " + std::to_string(slot) + " is " + KindName(kind) +
              ", and this unit draws OPAQUE, MASK and BLEND -- what is transmitted through a sheet "
              "or refracted by a volume is the scene behind it, which is a read of the frame this "
              "unit does not have, not a blend factor it could substitute";
      Slots.clear();
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
    if (batch.MaterialSlot >= Slots.size()) {
      NIdx = 0;
      error = "a draw names surface slot " + std::to_string(batch.MaterialSlot) +
              " over a table of " + std::to_string(Slots.size()) + " surfaces";
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

  /* THE LIST IS ALREADY IN ORDER -- opaque, then masked, then blended back to front, which is
   * `DrawKey`'s own ordering -- so the encoder only notices where the state changes: the batcher
   * merged what shared a pipeline and a slot, and what is left is exactly the changes that had to
   * happen. */
  size_t bound = kPipelines;
  size_t boundSlot = 0;
  bool slotBound = false;
  for (const DrawBatch &batch : Batches) {
    const SurfaceSlot &surface = Slots[batch.MaterialSlot];
    const bool textured = batch.Layout == VertexLayout::PositionUv && HasUv && Uv;
    const VertexLayout wanted = textured ? VertexLayout::PositionUv : VertexLayout::Position;
    /* THE FACING FOLLOWS THE SLOT, AND THE SORT KEY DOES NOT CARRY IT: two batches of one layout
     * that disagree about `doubleSided` cost a pipeline change here rather than being merged. That
     * is a batching cost and not a correctness one, and it is the honest place to pay it -- putting
     * the flag in the key would sort a subject's parts by a device state. */
    const size_t wantedPipeline = PipelineAt(wanted, surface.Kind, surface.CullsBack);
    if (wantedPipeline != bound) {
      pass.SetPipeline(Pipelines[wantedPipeline]);
      /* The two layouts put the radiance in different slots, so every slot the incoming layout
       * declares is rebound: leaving the other one's buffer where it was is how a uv slot ends up
       * holding radiance. */
      if (textured) { pass.SetVertexBuffer(1, Uv); }
      pass.SetVertexBuffer(textured ? 2u : 1u, Emit);
      bound = wantedPipeline;
    }
    if (!slotBound || boundSlot != batch.MaterialSlot) {
      pass.SetBindGroup(0, surface.Bind);
      boundSlot = batch.MaterialSlot;
      slotBound = true;
    }
    pass.DrawIndexed(batch.IndexCount, 1, batch.FirstIndex, 0, 0);
  }
}

} // namespace outshine::Render
