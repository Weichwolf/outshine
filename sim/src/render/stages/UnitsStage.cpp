#include "UnitsStage.h"
#include "SceneTargets.h"

#include <cmath>
#include <cstring>
#include <string>
#include "AtmoCommon.h"
#include "AtmoHaze.h"
#include "AtmoSample.h"
#include "SceneScale.h"
#include "Log.h"
#include "Mips.h"

namespace outshine::Render {

/* The airframe shader. Its LIGHT is the terrain's, term for term — same 0.4 ambient + 3.0 N.L, same
 * overcast lift, same day gate, same weather haze out of AtmoHaze.h — because a jet lit by a second
 * model would sit in front of the world rather than in it. What differs is only where the base colour
 * comes from (a baked texture or a material factor) and that the vertex carries a PART index. */
static const char *kUnitsWGSL = R"(
struct U { mvp : mat4x4f, sun : vec4f, haze : vec4f, dk0 : vec4f, dk1 : vec4f, dk2 : vec4f };
@group(0) @binding(0) var<uniform> u : U;
/* One 3x4 affine per part, three vec4 rows each: part 0 is body->camera-relative-ECEF, every other is
 * that chain with its hinge angle folded in. UnitsStage::Encode computes them; the shader only reads. */
struct UnitParts { rows : array<vec4f, 96> };
@group(0) @binding(1) var<storage, read> units : array<UnitParts>;
@group(0) @binding(2) var samp : sampler;
@group(0) @binding(3) var lsamp : sampler;
@group(0) @binding(4) var svLUT : texture_2d<f32>;
@group(0) @binding(5) var<uniform> A : Atmo;
struct Mats { c : array<vec4f, 16> };
@group(1) @binding(0) var<uniform> mats : Mats;
@group(1) @binding(1) var baseTex : texture_2d<f32>;

const kUnitAmb : f32 = 0.4;
const kUnitDir : f32 = 3.0;
const kUnitOvercastAmb : f32 = 0.15;

struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f, @location(1) nrm : vec3f,
              @location(2) wpos : vec3f, @location(3) @interpolate(flat) mat : u32 };

@vertex fn vs(@builtin(instance_index) inst : u32,
              @location(0) p : vec3f, @location(1) n : vec3f, @location(2) uv : vec2f,
              @location(3) tag : u32) -> VOut {
  let part = tag & 0xffffu;
  let r0 = units[inst].rows[part * 3u + 0u];
  let r1 = units[inst].rows[part * 3u + 1u];
  let r2 = units[inst].rows[part * 3u + 2u];
  let p4 = vec4f(p, 1.0);
  let rel = vec3f(dot(r0, p4), dot(r1, p4), dot(r2, p4));
  var o : VOut;
  o.pos = u.mvp * vec4f(rel, 1.0);
  o.uv = uv;
  o.nrm = vec3f(dot(r0.xyz, n), dot(r1.xyz, n), dot(r2.xyz, n));
  o.wpos = rel;
  o.mat = tag >> 16u;
  return o;
}

struct UFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
@fragment fn fs(in : VOut) -> UFrag {
  let nrmN = normalize(in.nrm);
  let mc = mats.c[in.mat];
  // Sampled unconditionally: a textureSample under non-uniform control flow is undefined in WGSL, and
  // a model without a base-colour map binds a 1x1 white so the fetch is free either way.
  let tex = textureSample(baseTex, samp, in.uv).rgb;
  let base = select(mc.rgb, tex, mc.w > 0.5);
  let vdir = normalize(in.wpos);
  let upR = normalize(A.camPosMm.xyz);
  let diffGate = select(1.0, A.skyExtra.x, A.skyExtra.y > 0.5);
  let fragAltM = (length(A.camPosMm.xyz + in.wpos * 1.0e-6) - u.haze.z) * 1.0e6;
  let sunThru = deckSunThru(u.dk0, fragAltM) * deckSunThru(u.dk1, fragAltM) * deckSunThru(u.dk2, fragAltM);
  let diff = max(dot(nrmN, normalize(u.sun.xyz)), 0.0) * diffGate * sunThru;
  let light = select(1.0, 0.08 + 0.92 * A.skyExtra.x, A.skyExtra.y > 0.5);
  let ambW = kUnitAmb + kUnitOvercastAmb * (1.0 - sunThru);
  var c = base * (ambW + kUnitDir * diff) * light;
  let distM = length(in.wpos);
  let hz = hazeTransmittance3(u.haze.x, 0.5 * (u.haze.y + fragAltM), distM);
  c = c * hz + hazeInscatter(svLUT, lsamp, A, vdir) * (vec3f(1.0) - hz);
  var o : UFrag;
  o.col = vec4f(c, 1.0);
  /* A unit's own motion is not published to this stage yet; what the sentinel guarantees today is
   * that an airframe drawn over a blade does not inherit the blade's velocity. */
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

static_assert(kMaxUnitParts * 3 == 96, "the WGSL `UnitParts.rows` length is kMaxUnitParts * 3");
static_assert(kMaxUnitMaterials == 16, "the WGSL `Mats.c` length is kMaxUnitMaterials");

/* 3x4 affine, row-major, implicit fourth row (0,0,0,1) — the same convention UnitModel bakes with. */
static void Mul34(const float a[12], const float b[12], float o[12]) {
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++)
      o[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c];
    o[r * 4 + 3] = a[r * 4 + 0] * b[3] + a[r * 4 + 1] * b[7] + a[r * 4 + 2] * b[11] + a[r * 4 + 3];
  }
}

bool UnitsStage::AddModel(const char *typeName, const char *dir) {
  auto cpu = std::make_unique<UnitModel>();
  if (!cpu->LoadDir(typeName, dir)) return false;
  GpuModel m;
  m.Cpu = std::move(cpu);
  Models.push_back(std::move(m));
  if (Ready) UploadModel(Models.back());
  return true;
}

const UnitsStage::GpuModel *UnitsStage::Find(const char *type) const {
  if (!type || !type[0]) return nullptr;
  for (const GpuModel &m : Models)
    if (m.Cpu->TypeName() == type) return &m;
  return nullptr;
}

bool UnitsStage::Nozzle(const char *type, float off[3], float &radiusM) const {
  const GpuModel *m = Find(type);
  if (!m || !m->Cpu || m->Cpu->LodCount() <= 0) return false;
  const UnitModel::Lod &l = m->Cpu->GetLod(0);   /* the geometry is the same aeroplane at every level */
  if (!l.HasNozzle) return false;
  for (int i = 0; i < 3; i++) off[i] = l.NozzleOff[i];
  radiusM = l.NozzleRadiusM;
  return true;
}

void UnitsStage::UploadModel(GpuModel &m) {
  m.Lods.clear();
  for (int i = 0; i < m.Cpu->LodCount(); i++) {
    const UnitModel::Lod &src = m.Cpu->GetLod(i);
    GpuLod g;
    g.IndexCount = (uint32_t)src.Idx.size();

    wgpu::BufferDescriptor bd{};
    bd.size = src.Verts.size() * sizeof(UnitVertex);
    bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    g.Vtx = Device.CreateBuffer(&bd);
    Queue.WriteBuffer(g.Vtx, 0, src.Verts.data(), bd.size);

    bd.size = ((src.Idx.size() * 4) + 3) & ~(size_t)3;
    bd.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    g.Idx = Device.CreateBuffer(&bd);
    Queue.WriteBuffer(g.Idx, 0, src.Idx.data(), src.Idx.size() * 4);

    bd.size = kMaxUnitMaterials * 4 * sizeof(float);
    bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    g.Mats = Device.CreateBuffer(&bd);
    Queue.WriteBuffer(g.Mats, 0, src.MatColor, bd.size);

    /* A 1x1 white stands in when a level declares no base-colour map, so the fragment shader has one
     * unconditional fetch instead of a branch it is not allowed to take. */
    const int ts = src.TexW > 0 ? src.TexW : 1;
    const int mips = fb_mip_count(ts);
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)ts, (uint32_t)ts, 1};
    td.mipLevelCount = (uint32_t)mips;
    td.format = wgpu::TextureFormat::RGBA8UnormSrgb;   /* baked base colour is sRGB; the view decodes */
    td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    g.Tex = Device.CreateTexture(&td);
    {
      std::vector<uint8_t> pyr((size_t)fb_pyramid_bytes(ts));
      if (src.TexW > 0) {
        fb_build_pyramid(src.TexRgba.data(), ts, pyr.data());
      } else {
        std::memset(pyr.data(), 0xFF, pyr.size());
      }
      size_t off = 0;
      for (int lv = 0; lv < mips; lv++) {
        const uint32_t d = (uint32_t)(ts >> lv) ? (uint32_t)(ts >> lv) : 1u;
        wgpu::TexelCopyTextureInfo dst{};
        dst.texture = g.Tex;
        dst.mipLevel = (uint32_t)lv;
        wgpu::TexelCopyBufferLayout lay{};
        lay.bytesPerRow = d * 4;
        lay.rowsPerImage = d;
        wgpu::Extent3D ext{d, d, 1};
        Queue.WriteTexture(&dst, pyr.data() + off, (size_t)d * d * 4, &lay, &ext);
        off += (size_t)d * d * 4;
      }
    }

    wgpu::BindGroupEntry be[2] = {};
    be[0].binding = 0; be[0].buffer = g.Mats; be[0].size = kMaxUnitMaterials * 4 * sizeof(float);
    be[1].binding = 1; be[1].textureView = g.Tex.CreateView();
    wgpu::BindGroupDescriptor bgd{};
    bgd.layout = Pipe.GetBindGroupLayout(1);
    bgd.entryCount = 2;
    bgd.entries = be;
    g.Bind = Device.CreateBindGroup(&bgd);
    m.Lods.push_back(std::move(g));
  }
}

void UnitsStage::Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::Sampler lutSamp,
                             wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  HdrFormat = gpu.HdrFormat;
  Samp = samp;
  LutSamp = lutSamp;
  SkyLutView = skyLutView;
  AtmoBuf = atmoBuf;

  const std::string src = std::string(kSceneScaleWGSL) + kAtmoCommon + kAtmoSample + HazeConstsWGSL() + kHazeWGSL
                        + kVelocityWGSL + kUnitsWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attrs[4] = {};
  attrs[0].format = wgpu::VertexFormat::Float32x3; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
  attrs[1].format = wgpu::VertexFormat::Float32x3; attrs[1].offset = 12; attrs[1].shaderLocation = 1;
  attrs[2].format = wgpu::VertexFormat::Float32x2; attrs[2].offset = 24; attrs[2].shaderLocation = 2;
  attrs[3].format = wgpu::VertexFormat::Uint32;    attrs[3].offset = 32; attrs[3].shaderLocation = 3;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = sizeof(UnitVertex);
  vbl.attributeCount = 4;
  vbl.attributes = attrs;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;   /* [0,1] reversed-Z, as the whole scene pass */

  wgpu::ColorTargetState ct{};
  ct.format = HdrFormat;

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = sm;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(true)};
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  /* The asset culls back faces on every material — with them drawn, inverted bodies hide inside the
   * mesh. The body->ECEF basis has determinant +1, so the glTF counter-clockwise winding survives into
   * clip space unchanged. */
  rp.primitive.cullMode = wgpu::CullMode::Back;
  rp.primitive.frontFace = wgpu::FrontFace::CCW;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  bd.size = (uint64_t)kMaxUnits * kPartRows * 4 * sizeof(float);
  bd.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  PartBuf = Device.CreateBuffer(&bd);
  PartScratch.assign((size_t)kMaxUnits * kPartRows * 4, 0.0f);

  wgpu::BindGroupEntry be[6] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = PartBuf; be[1].size = PartBuf.GetSize();
  be[2].binding = 2; be[2].sampler = Samp;
  be[3].binding = 3; be[3].sampler = LutSamp;
  be[4].binding = 4; be[4].textureView = SkyLutView;
  be[5].binding = 5; be[5].buffer = AtmoBuf; be[5].size = kAtmoUniformBytes;
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 6;
  bgd.entries = be;
  Frame = Device.CreateBindGroup(&bgd);

  Ready = true;
  for (GpuModel &m : Models) UploadModel(m);
}

void UnitsStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  LastDraws_ = 0;
  /* THE EMPTY-WORLD PATH, and it is the first thing here: no device, no cast or no registered model
   * records nothing at all, so the frame is bit-identical to one taken before this stage drew. */
  if (!Ready || Count <= 0 || Models.empty()) { LogCast(ctx); return; }

  /* The visible cast, its LOD choice and its part matrices, in one walk. */
  Items.clear();

  int slot = 0;
  for (int i = 0; i < Count && slot < kMaxUnits; i++) {
    const UnitDraw &d = Draws[i];
    const GpuModel *m = Find(d.Type);
    if (!m || m->Lods.empty()) continue;

    const double rel[3] = {d.Ecef[0] - ctx.Eye[0], d.Ecef[1] - ctx.Eye[1], d.Ecef[2] - ctx.Eye[2]};
    const double range = std::sqrt(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
    /* BEHIND THE EYE is the one cull worth doing here: the cast is a handful of units, so a full
     * frustum test would cost more to maintain than the draws it saves. 20 m covers the airframe's
     * own half-length, so a jet the camera sits inside is still drawn. */
    if (rel[0] * ctx.Fwd[0] + rel[1] * ctx.Fwd[1] + rel[2] * ctx.Fwd[2] < -20.0) continue;

    const int li = m->Cpu->PickLod(range);
    if (li < 0 || li >= (int)m->Lods.size()) continue;
    const UnitModel::Lod &cpu = m->Cpu->GetLod(li);

    float *rows = PartScratch.data() + (size_t)slot * kPartRows * 4;
    /* Part 0: body -> camera-relative ECEF. Rot is column-major (col0 = +X right, col1 = +Y up,
     * col2 = +Z aft), the rows below are row-major — hence the transposed read. */
    float root[12];
    for (int r = 0; r < 3; r++) {
      root[r * 4 + 0] = d.Rot[0 * 3 + r];
      root[r * 4 + 1] = d.Rot[1 * 3 + r];
      root[r * 4 + 2] = d.Rot[2 * 3 + r];
      root[r * 4 + 3] = (float)rel[r];
    }
    float mats[kMaxUnitParts * 12];   /* stack, not heap: the loop runs per unit per frame */
    std::memcpy(mats, root, sizeof root);
    const size_t nParts = cpu.Parts.size() < (size_t)kMaxUnitParts ? cpu.Parts.size() : (size_t)kMaxUnitParts;
    for (size_t p = 1; p < nParts; p++) {
      const UnitModel::Part &part = cpu.Parts[p];
      const float a = UnitModel::PartAngleDeg(part, d.Art) * 0.017453292519943295f;
      const float ca = std::cos(a), sa = std::sin(a);
      const float rx[12] = {1, 0, 0, 0, 0, ca, -sa, 0, 0, sa, ca, 0};
      float hinge[12], out[12];
      Mul34(part.Base, rx, hinge);
      /* Parents are allocated before their children in the load walk, so this reads a finished matrix. */
      Mul34(mats + (size_t)part.Parent * 12, hinge, out);
      std::memcpy(mats + p * 12, out, sizeof out);
    }
    std::memcpy(rows, mats, nParts * 12 * sizeof(float));

    LogUnit(ctx, d, cpu, rel, range, li, (int)(m->Lods[(size_t)li].IndexCount / 3));

    Items.push_back(Item{&m->Lods[(size_t)li], slot});
    slot++;
  }
  if (Items.empty()) { LogCast(ctx); return; }

  Queue.WriteBuffer(PartBuf, 0, PartScratch.data(), (size_t)slot * kPartRows * 4 * sizeof(float));

  /* The frame block, filled exactly as TilesStage fills its own — the two shaders read one `U`. */
  float uni[kUniFloats];
  std::memcpy(uni, ctx.Mvp20, sizeof ctx.Mvp20);
  const double eyeLen = std::sqrt(ctx.Eye[0] * ctx.Eye[0] + ctx.Eye[1] * ctx.Eye[1] + ctx.Eye[2] * ctx.Eye[2]);
  uni[20] = HazeSigma0(Sky.VisibilityM);
  uni[21] = ctx.AltM;
  uni[22] = (float)((eyeLen - (double)ctx.AltM) / 1.0e6);
  uni[23] = 0.0f;
  const float sunUp = (float)(ctx.SunDir[0] * ctx.Up[0] + ctx.SunDir[1] * ctx.Up[1] + ctx.SunDir[2] * ctx.Up[2]);
  for (int i = 0; i < 3; i++) {
    const CloudDeckParams &dk = Sky.Deck[i];
    uni[24 + i * 4 + 0] = dk.BaseM;
    uni[24 + i * 4 + 1] = dk.TopM;
    uni[24 + i * 4 + 2] = DeckSunOpticalDepth(dk, sunUp);
    uni[24 + i * 4 + 3] = dk.Cover;
  }
  Queue.WriteBuffer(Uni, 0, uni, sizeof uni);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Frame);
  const GpuLod *bound = nullptr;
  for (const Item &it : Items) {
    if (it.Lod != bound) {
      pass.SetBindGroup(1, it.Lod->Bind);
      pass.SetVertexBuffer(0, it.Lod->Vtx);
      pass.SetIndexBuffer(it.Lod->Idx, wgpu::IndexFormat::Uint32);
      bound = it.Lod;
    }
    pass.DrawIndexed(it.Lod->IndexCount, 1, 0, 0, (uint32_t)it.Slot);
  }
  LastDraws_ = (int)Items.size();
  LogCast(ctx);
}

/* WHERE THIS UNIT LANDED ON THE FRAME, through the SAME Mvp the draw uses — so a screenshot can be held
 * against the published pose without trusting a second projection — plus what its HINGES were set to, so
 * a drawn deflection is checkable against the published channel and not only against the eye. Pixel
 * centre of the model origin, top-left convention; z_ndc is reversed-Z, so > 0 is in front of near. */
void UnitsStage::LogUnit(const FrameContext &ctx, const UnitDraw &d, const UnitModel::Lod &lod,
                           const double rel[3], double range, int li, int tris) const {
  if (ctx.FrameNo % kUnitLogEvery != 1) return;
  const float *M = ctx.Mvp20;
  const float pr[4] = {(float)rel[0], (float)rel[1], (float)rel[2], 1.0f};
  float clip[4] = {0, 0, 0, 0};
  for (int r = 0; r < 4; r++)
    for (int k = 0; k < 4; k++) clip[r] += M[k * 4 + r] * pr[k];
  const double w = clip[3] != 0.0f ? (double)clip[3] : 1.0;
  double aileron = 0.0, elevon = 0.0, rudder = 0.0, brake = 0.0, gear = 0.0;
  for (const UnitModel::Part &part : lod.Parts) {
    const double a = UnitModel::PartAngleDeg(part, d.Art);
    if (part.Node == "ctl.aileron.l") aileron = a;
    else if (part.Node == "ctl.elevon.l") elevon = a;
    else if (part.Node == "ctl.rudder") rudder = a;
    else if (part.Node == "ctl.speedbrake.0") brake = a;
    else if (part.Node == "gear.main.l") gear = a;
  }
  Log::Debug("render", "unit_draw",
               {{"type", std::string(d.Type)}, {"rangeM", range}, {"lod", li}, {"tris", tris},
                {"px", 0.5 * (clip[0] / w + 1.0) * ctx.Width},
                {"py", 0.5 * (1.0 - clip[1] / w) * ctx.Height},
                {"zndc", clip[2] / w},
                {"ailLDeg", aileron}, {"dhtLDeg", elevon}, {"rudDeg", rudder},
                {"sbDeg", brake}, {"gearLDeg", gear}});
}

/* How many units the world offered and how many reached the pass — the "an empty registry draws zero
 * calls" contract as a number rather than as a claim. */
void UnitsStage::LogCast(const FrameContext &ctx) const {
  if (ctx.FrameNo % kUnitLogEvery != 1) return;
  Log::Debug("render", "units", {{"cast", Count}, {"drawn", LastDraws_},
                                   {"models", (int)Models.size()}});
}

} // namespace outshine::Render
