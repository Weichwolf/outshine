#include "BuildingsStage.h"
#include "SceneTargets.h"
#include <cmath>
#include "ChunkVtx.h"
#include "Frustum.h"
#include "SceneScale.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this stage. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "ShadowSample.h"
#include "SurfaceLight.h"
#include <string>

namespace outshine::Render {

static const char *kBuildingWGSL = R"(
/* anc.xyz = anchor - camera (m); sun.xyz = ECEF direction to the sun, sun.w = the daylight factor;
 * up.xyz = geodetic up at the anchor, up.w = the ambient night floor this frame. */
struct B { mvp : mat4x4f, anc : vec4f, sun : vec4f, up : vec4f };
@group(0) @binding(0) var<uniform> b : B;
@group(0) @binding(1) var<storage, read> I : Irr;
@group(0) @binding(2) var<uniform> C : Csm;
@group(0) @binding(3) var shMap : texture_depth_2d;
@group(0) @binding(4) var shSamp : sampler_comparison;

/* [SET] REFLECTANCE, not a display colour: lime render measures 0.35-0.50 and weathered plaster with
 * timber framing sits lower; a red-clay pantile roof measures 0.10-0.15 with the red dominant. These
 * are the numbers litRadiance multiplies, so they must be albedos or nothing downstream is on scale.
 * Replaced the day a material comes off the Footprint record. */
const kWall : vec3f = vec3f(0.40, 0.37, 0.33);
const kRoof : vec3f = vec3f(0.14, 0.075, 0.055);
const kFloorM : f32 = 2.9;   /* [SET] German residential floor-to-floor; the band spacing IS this number */

struct BOut { @builtin(position) pos : vec4f, @location(0) nrm : vec3f, @location(1) uvb : vec2f,
              @location(2) rel : vec3f };

@vertex fn vs(@location(0) p : vec3f, @location(1) uvw : vec2f, @location(2) nb : vec3f) -> BOut {
  var o : BOut;
  let rel = p + b.anc.xyz;
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.nrm = nb;
  o.uvb = uvw;
  o.rel = rel;
  return o;
}

struct BFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
@fragment fn fs(in : BOut) -> BFrag {
  /* An OSM ring may be wound either way, so the extruded normal may point INTO the prism — measured:
   * a wall on the shaded side read N.L > 0 and was then correctly shadowed by the cascade that saw
   * the building in front of it, which is how the two disagreed visibly. The prism is closed and
   * opaque, so the visible face is always the near one and its outward normal must point back at the
   * eye; that is a test the winding cannot lie about. */
  let nrmB = normalize(in.nrm) * select(1.0, -1.0, dot(in.nrm, in.rel) > 0.0);
  let isRoof = select(0.0, 1.0, in.uvb.x < 0.0);   // the cap is tagged at build time (uv.x = -1)
  var alb = mix(kWall, kRoof, isRoof);
  // Storey banding: a dark line at every floor level, and a plinth at the bottom. Cheap, and it is
  // what makes the extrusion read as a building rather than as a box.
  let fl = fract(in.uvb.y / kFloorM);
  let band = smoothstep(0.0, 0.06, fl) * (1.0 - smoothstep(0.94, 1.0, fl));
  let plinth = 1.0 - 0.35 * (1.0 - smoothstep(0.0, 1.1, in.uvb.y));
  alb = alb * mix(1.0, 0.72 + 0.28 * band, 1.0 - isRoof) * plinth;
  let sunB = normalize(b.sun.xyz);
  let sunVis = csmSunVis(shMap, shSamp, C, in.rel, nrmB, sunB);
  let upB = normalize(b.up.xyz);
  var o : BFrag;
  o.col = litRadiance(I, alb, 1.0, nrmB, upB, sunB, sunVis,
                      1.0, 1.0, b.up.w);
  /* A prism is world-fixed, so its motion is the camera's and the resolve gets that from depth. The
   * write is not redundant: this stage draws AFTER the ground cover and a facade in front of a blade
   * would otherwise keep the blade's velocity. */
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

void BuildingsStage::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + ShadowSampleWGSL()
                        + kVelocityWGSL + kBuildingWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attr[3] = {};
  attr[0].format = wgpu::VertexFormat::Float32x3; attr[0].offset = 0;  attr[0].shaderLocation = 0;
  attr[1].format = wgpu::VertexFormat::Float32x2; attr[1].offset = 12; attr[1].shaderLocation = 1;
  attr[2].format = wgpu::VertexFormat::Float32x3; attr[2].offset = 20; attr[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = kVertexStrideB;
  vbl.attributeCount = 3;
  vbl.attributes = attr;

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = m;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(true)};
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  /* An extruded ring has no reliable winding — an OSM footprint may be given either way round — and a
   * prism is closed, so back faces cost nothing but a depth test. */
  rp.primitive.cullMode = wgpu::CullMode::None;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be[5] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = Light.Irradiance; be[1].size = wgpu::kWholeSize;
  be[2].binding = 2; be[2].buffer = Light.Cascades;   be[2].size = kShadowUniFloats * sizeof(float);
  be[3].binding = 3; be[3].textureView = Light.ShadowAtlas;
  be[4].binding = 4; be[4].sampler = Light.ShadowCompare;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 5;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
}

void BuildingsStage::SetMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                             const DagCluster *clusters, int nclusters, const double anchor[3]) {
  NVerts = nverts;
  NIdx = nidx;
  Clusters.assign(clusters, clusters + (nclusters > 0 ? nclusters : 0));
  if (Clusters.empty() && nverts > 0) {
    DagCluster c{};
    c.Count = nidx;
    c.ParentErr = kDagRootErr;
    BoundingSphere(verts, nverts, 8, c.SelfCenter, &c.SelfRadius);
    Clusters.push_back(c);
  }
  BaseVerts = 0;
  for (const DagCluster &c : Clusters)
    if (c.Level == 0) BaseVerts = std::max(BaseVerts, c.First + c.Count);
  for (int i = 0; i < 3; i++) Anchor[i] = anchor[i];
  if (nverts == 0 || !Device) return;
  if (Cap < nverts) {
    wgpu::BufferDescriptor bd{};
    bd.size = (uint64_t)nverts * 8 * sizeof(float);
    bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Vtx = Device.CreateBuffer(&bd);
    Cap = nverts;
  }
  Queue.WriteBuffer(Vtx, 0, verts, (size_t)nverts * 8 * sizeof(float));
  if (IdxCap < nidx) {
    wgpu::BufferDescriptor bd{};
    bd.size = (uint64_t)nidx * sizeof(uint32_t);
    bd.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    Idx = Device.CreateBuffer(&bd);
    IdxCap = nidx;
  }
  Queue.WriteBuffer(Idx, 0, idx, (size_t)nidx * sizeof(uint32_t));
}

void BuildingsStage::SetSun(const double sunEcef[3], const double up[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) { SunDir[i] = sunEcef[i]; Up[i] = up[i]; }
  NightAmbient = nightAmbient;
}

void BuildingsStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  DrawnVerts = 0;
  if (NVerts == 0 || !Vtx) return;

  const float fPx = 0.5f * (float)ctx.Height / std::tan(0.5f * (float)ctx.FovDeg * 3.14159265358979f / 180.0f);
  const double eyeLocal[3] = {ctx.Eye[0] - Anchor[0], ctx.Eye[1] - Anchor[1], ctx.Eye[2] - Anchor[2]};
  const double rel[3] = {-eyeLocal[0], -eyeLocal[1], -eyeLocal[2]};
  Frustum fr;
  fr.Set(ctx.Mvp20);
  Ranges.clear();
  for (const DagCluster &c : Clusters) {
    /* A prism has no up: the DAG measured its error as a nearest-point distance, which is already
     * the length in every direction. */
    static const float kNoUp[3] = {0.0f, 0.0f, 0.0f};
    if (!DagSelect(c, eyeLocal, fPx, SseTauPx(), kNoUp)) continue;
    if (!fr.Visible(rel, c.SelfCenter, c.SelfRadius)) continue;
    DrawnVerts += c.Count;
    if (!Ranges.empty() && Ranges.back().First + Ranges.back().Count == c.First)
      Ranges.back().Count += c.Count;
    else
      Ranges.push_back(DrawRange{c.First, c.Count});
  }
  if (Ranges.empty()) return;
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  u[16] = (float)(Anchor[0] - ctx.Eye[0]);
  u[17] = (float)(Anchor[1] - ctx.Eye[1]);
  u[18] = (float)(Anchor[2] - ctx.Eye[2]);
  u[19] = 0.0f;
  u[20] = (float)SunDir[0]; u[21] = (float)SunDir[1]; u[22] = (float)SunDir[2]; u[23] = 0.0f;
  u[24] = (float)Up[0]; u[25] = (float)Up[1]; u[26] = (float)Up[2]; u[27] = NightAmbient;
  Queue.WriteBuffer(Uni, 0, u, sizeof u);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);
  for (const DrawRange &r : Ranges) pass.DrawIndexed(r.Count, 1, r.First, 0, 0);
}

} // namespace outshine::Render
