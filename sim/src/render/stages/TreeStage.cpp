#include "TreeStage.h"

#include "SceneTargets.h"
#include "SceneScale.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this stage. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "ShadowSample.h"
#include "SurfaceLight.h"

#include <cmath>
#include <cstring>
#include <string>

namespace outshine::Render {

static const char *kTreeWGSL = R"(
struct T {
  mvp  : mat4x4f,
  ax   : vec4f,   // ECEF east axis,  w = the stand's east offset from the camera (m)
  ay   : vec4f,   // ECEF north axis, w = the stand's north offset (m)
  az   : vec4f,   // ECEF up axis,    w = the eye's height over the stand's foot (m)
  sun  : vec4f,   // ECEF sun direction, w = this frame's ambient night floor
  bark : vec4f,   // rgb bark reflectance, w = the furrow's share of it
  bpar : vec4f,   // x = furrow cycles per radian, y = furrow depth, z = tree height (m), w = leaf scale (m)
  leaf : vec4f,   // rgb lamina reflectance (tint x base), w unused
};
@group(0) @binding(0) var<uniform> b : T;
@group(0) @binding(1) var<storage, read> I : Irr;
@group(0) @binding(2) var<uniform> C : Csm;
@group(0) @binding(3) var shMap : texture_depth_2d;
@group(0) @binding(4) var shSamp : sampler_comparison;

/* Tree space is y-up and metric once multiplied by the declared height; the stand maps it onto the
 * ECEF triad the whole frame is built in. One place, both nets. */
fn standOrigin() -> vec3f {
  return b.ax.xyz * b.ax.w + b.ay.xyz * b.ay.w - b.az.xyz * b.az.w;
}
/* TREE Z POINTS SOUTH, and that is not a taste: (east, up, north) is LEFT-handed (east x up = -north),
 * so mapping a right-handed y-up model onto it MIRRORS the tree and inverts every triangle's winding.
 * Negating one axis makes the map a proper rotation — the mesh keeps its handedness and back-face
 * culling keeps its meaning. */
fn toEcef(local : vec3f) -> vec3f {
  return b.ax.xyz * local.x - b.ay.xyz * local.z + b.az.xyz * local.y;
}

struct TOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) nrm : vec3f,
              @location(2) loc : vec3f };

/* Ein Stand: xy sind Ost/Nord in Metern vom Kamerastandpunkt, z die Hoehe, w die Gierung. */
@vertex fn vsBark(@location(0) p : vec3f, @location(1) n : vec3f,
                  @location(2) st : vec4f) -> TOut {
  var o : TOut;
  let hs = select(b.bpar.z, st.z, st.z > 0.0);
  var pm = p * hs;
  let cy = cos(st.w); let sy = sin(st.w);
  pm = vec3f(pm.x * cy - pm.z * sy, pm.y, pm.x * sy + pm.z * cy);
  let rel = standOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  let cy2 = cos(st.w); let sy2 = sin(st.w);
  let nr = vec3f(n.x * cy2 - n.z * sy2, n.y, n.x * sy2 + n.z * cy2);
  o.nrm = toEcef(nr);
  o.loc = vec3f(nr.x, pm.y, nr.z);   /* the furrow reads a circumferential angle and a height, no uv */
  return o;
}

/* THE LAMINA ROLLS FREELY ABOUT ITS STALK and that is the same assumption world/LeafAngleDistribution
 * closes G(el) under: midrib on the measured stalk direction, normal anywhere on the circle
 * perpendicular to it. A tilt invented here would draw a canopy the far stage no longer computes. */
@vertex fn vsLeaf(@location(0) v : vec3f, @location(1) n : vec3f,
                  @location(2) ip : vec4f, @location(3) idir : vec4f) -> TOut {
  var o : TOut;
  let yAx = normalize(idir.xyz);
  let refA = select(vec3f(0.0, 1.0, 0.0), vec3f(1.0, 0.0, 0.0), abs(yAx.y) > 0.9);
  let uAx = normalize(cross(refA, yAx));
  let wAx = cross(yAx, uAx);
  let zAx = uAx * cos(ip.w) + wAx * sin(ip.w);
  let xAx = cross(yAx, zAx);
  let pm = ip.xyz * b.bpar.z + (xAx * v.x + yAx * v.y + zAx * v.z) * b.bpar.w;
  let nl = xAx * n.x + yAx * n.y + zAx * n.z;
  let rel = standOrigin() + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  o.nrm = toEcef(nl);
  o.loc = vec3f(0.0);
  return o;
}

/* THE FURROW, and its metric is the declared one: `bpar.x` counts cycles per RADIAN of circumference,
 * so the pitch on a 0.4 m trunk radius is 0.4/f metres and a twig — whose radius is a tenth of that —
 * comes out smooth, which is what bark does. The second sine breaks the comb into ridges of unequal
 * width; the meander with height keeps them from being drawn lines. */
fn furrow(nlx : f32, nlz : f32, y : f32, f : f32) -> f32 {
  let ang = atan2(nlz, nlx) + 0.22 * sin(y * 2.1) + 0.11 * sin(y * 5.3 + 1.3);
  let s1 = sin(ang * f) * 0.5 + 0.5;
  let s2 = sin(ang * f * 2.37 + 1.7) * 0.5 + 0.5;
  return pow(mix(s1, s1 * s2, 0.55), 1.4);
}

@fragment fn fsBark(in : TOut) -> @location(0) vec4f {
  let upB = normalize(b.az.xyz);
  let sunB = normalize(b.sun.xyz);
  let nB = normalize(in.nrm);
  /* A furrow runs ALONG the axis, so a face looking down the axis has none. The local normal's radial
   * length is that fade and it also keeps atan2 out of its degenerate direction. */
  let radial = length(vec2f(in.loc.x, in.loc.z));
  let fr = furrow(in.loc.x, in.loc.z, in.loc.y, b.bpar.x) * radial;
  let alb = b.bark.rgb * mix(b.bark.w, 1.0, mix(1.0, fr, b.bpar.y));
  let sunVis = csmSunVis(shMap, shSamp, C, in.rel, upB, sunB);
  return litRadiance(I, alb, 1.0, nB, upB, sunB, sunVis,
                     1.0, 1.0, b.sun.w);
}

@fragment fn fsLeaf(in : TOut) -> @location(0) vec4f {
  let upB = normalize(b.az.xyz);
  let sunB = normalize(b.sun.xyz);
  let n0 = normalize(in.nrm);
  let vB = normalize(-in.rel);
  let viewSide = dot(n0, vB);
  let nB = select(-n0, n0, viewSide >= 0.0);
  let alb = b.leaf.rgb;
  let sunVis = csmSunVis(shMap, shSamp, C, in.rel, upB, sunB);
  let thruDir = 1.0;
  let lit = litRadiance(I, alb, 1.0, nB, upB, sunB, sunVis, thruDir,
                        1.0, b.sun.w);

  /* THE ONE TERM AN OPAQUE BRDF HAS NO PLACE FOR. A leaf is 0.2 mm of mesophyll: in the visible it
   * transmits about as much as it reflects (LOPEX/PROSPECT, tau ~ rho at 550 and 650 nm), so the
   * transmittance IS the albedo and this line introduces no constant. It fires only when sun and eye
   * stand on opposite faces, which is the whole of what `backlit` exists to show. */
  let sunSide = dot(n0, sunB);
  let through = select(0.0, abs(sunSide), sunSide * viewSide < 0.0);
  let trans = alb * (kSceneExposure * kInvPi) * I.sun.xyz * (through * sunVis * thruDir);

  let yw = vec3f(0.2126, 0.7152, 0.0722);
  let rgb = lit.rgb + trans;
  let outY = dot(rgb, yw);
  let dirY = dot(lit.rgb, yw) * lit.a + dot(trans, yw);
  return vec4f(rgb, select(1.0, clamp(dirY / outY, 0.0, 1.0), outY > 1.0e-9));
}
)";

void TreeStage::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + ShadowSampleWGSL()
                        + kTreeWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(false)};

  /* EXPLICIT, because two pipelines share one bind group: a default layout belongs to the pipeline
   * that produced it, and a group built from the bark's would be rejected by the leaf's. */
  wgpu::BindGroupLayoutEntry ble[6] = {};
  ble[0].binding = 0;
  ble[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  ble[0].buffer.type = wgpu::BufferBindingType::Uniform;
  ble[0].buffer.minBindingSize = kUniFloats * sizeof(float);
  ble[1].binding = 1;
  ble[1].visibility = wgpu::ShaderStage::Fragment;
  ble[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
  ble[2].binding = 2;
  ble[2].visibility = wgpu::ShaderStage::Fragment;
  ble[2].buffer.type = wgpu::BufferBindingType::Uniform;
  ble[3].binding = 3;
  ble[3].visibility = wgpu::ShaderStage::Fragment;
  ble[3].texture.sampleType = wgpu::TextureSampleType::Depth;
  ble[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  ble[4].binding = 4;
  ble[4].visibility = wgpu::ShaderStage::Fragment;
  ble[4].sampler.type = wgpu::SamplerBindingType::Comparison;
  ble[5].binding = 5;
  ble[5].visibility = wgpu::ShaderStage::Fragment;
  ble[5].buffer.type = wgpu::BufferBindingType::Uniform;
  wgpu::BindGroupLayoutDescriptor bgld{};
  bgld.entryCount = 5;
  bgld.entries = ble;
  wgpu::BindGroupLayout bgl = Device.CreateBindGroupLayout(&bgld);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &bgl;
  wgpu::PipelineLayout pl = Device.CreatePipelineLayout(&pld);

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;   /* reversed-Z, as every scene-pass surface */

  wgpu::VertexAttribute barkAttr[2] = {};
  barkAttr[0].format = wgpu::VertexFormat::Float32x3; barkAttr[0].offset = 0;  barkAttr[0].shaderLocation = 0;
  barkAttr[1].format = wgpu::VertexFormat::Float32x3; barkAttr[1].offset = 12; barkAttr[1].shaderLocation = 1;
  wgpu::VertexBufferLayout barkBuf{};
  barkBuf.arrayStride = kBarkFloats * sizeof(float);
  barkBuf.attributeCount = 2;
  barkBuf.attributes = barkAttr;
  wgpu::VertexAttribute standAttr{};
  standAttr.format = wgpu::VertexFormat::Float32x4; standAttr.offset = 0; standAttr.shaderLocation = 2;
  wgpu::VertexBufferLayout standBuf{};
  standBuf.arrayStride = 4 * sizeof(float);
  standBuf.stepMode = wgpu::VertexStepMode::Instance;
  standBuf.attributeCount = 1;
  standBuf.attributes = &standAttr;
  wgpu::VertexBufferLayout barkBufs[2] = {barkBuf, standBuf};

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = pl;
  rp.vertex.module = m;
  rp.vertex.entryPoint = "vsBark";
  rp.vertex.bufferCount = 1;
  rp.vertex.bufferCount = 2;
  rp.vertex.buffers = barkBufs;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.entryPoint = "fsBark";
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  rp.primitive.cullMode = wgpu::CullMode::Back;
  BarkPipe = Device.CreateRenderPipeline(&rp);

  wgpu::VertexAttribute leafAttr[2] = {};
  leafAttr[0].format = wgpu::VertexFormat::Float32x3; leafAttr[0].offset = 0;  leafAttr[0].shaderLocation = 0;
  leafAttr[1].format = wgpu::VertexFormat::Float32x3; leafAttr[1].offset = 12; leafAttr[1].shaderLocation = 1;
  wgpu::VertexAttribute instAttr[2] = {};
  instAttr[0].format = wgpu::VertexFormat::Float32x4; instAttr[0].offset = 0;  instAttr[0].shaderLocation = 2;
  instAttr[1].format = wgpu::VertexFormat::Float32x4; instAttr[1].offset = 16; instAttr[1].shaderLocation = 3;
  wgpu::VertexBufferLayout leafBufs[2] = {};
  leafBufs[0].arrayStride = kLeafFloats * sizeof(float);
  leafBufs[0].attributeCount = 2;
  leafBufs[0].attributes = leafAttr;
  leafBufs[1].arrayStride = kInstFloats * sizeof(float);
  leafBufs[1].stepMode = wgpu::VertexStepMode::Instance;
  leafBufs[1].attributeCount = 2;
  leafBufs[1].attributes = instAttr;

  rp.vertex.entryPoint = "vsLeaf";
  rp.vertex.bufferCount = 2;
  rp.vertex.buffers = leafBufs;
  fs.entryPoint = "fsLeaf";
  /* A lamina has no back: it is one sheet with a face on either side, and the fragment picks the one
   * that faces the eye. Culling it would delete half the canopy at every sun angle. */
  rp.primitive.cullMode = wgpu::CullMode::None;
  LeafPipe = Device.CreateRenderPipeline(&rp);

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
  bg.layout = bgl;
  bg.entryCount = 5;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
}

wgpu::Buffer TreeStage::Upload(const void *data, size_t bytes, wgpu::BufferUsage usage) {
  wgpu::BufferDescriptor bd{};
  bd.size = (bytes + 3u) & ~size_t(3);
  bd.usage = usage | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer b = Device.CreateBuffer(&bd);
  Queue.WriteBuffer(b, 0, data, bytes);
  return b;
}

void TreeStage::SetBark(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx) {
  BarkCount = 0;
  if (!Device || !verts || nverts == 0 || nidx == 0) return;
  BarkVtx = Upload(verts, (size_t)nverts * kBarkFloats * sizeof(float), wgpu::BufferUsage::Vertex);
  BarkIdx = Upload(idx, (size_t)nidx * sizeof(uint32_t), wgpu::BufferUsage::Index);
  BarkCount = nidx;
}

void TreeStage::SetLeaf(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                        const float *inst, uint32_t ninst, float scaleM) {
  LeafCount = 0;
  InstCount = 0;
  if (!Device || !verts || nverts == 0 || nidx == 0 || !inst || ninst == 0) return;
  LeafVtx = Upload(verts, (size_t)nverts * kLeafFloats * sizeof(float), wgpu::BufferUsage::Vertex);
  LeafIdx = Upload(idx, (size_t)nidx * sizeof(uint32_t), wgpu::BufferUsage::Index);
  LeafInst = Upload(inst, (size_t)ninst * kInstFloats * sizeof(float), wgpu::BufferUsage::Vertex);
  LeafCount = nidx;
  InstCount = ninst;
  LeafScaleM = scaleM;
}

void TreeStage::SetStand(double eastM, double northM, double eyeAglM, double heightM) {
  EastM = eastM;
  NorthM = northM;
  EyeAglM = eyeAglM;
  HeightM = heightM;
}

void TreeStage::SetStands(const float *inst, uint32_t n) {
  StandCount = n;
  if (n == 0 || !Device) return;
  StandBuf = Upload(inst, (size_t)n * 4 * sizeof(float), wgpu::BufferUsage::Vertex);
}

void TreeStage::SetSun(const double sunEcef[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) SunDir[i] = sunEcef[i];
  NightAmbient = nightAmbient;
}

void TreeStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  Drawn = 0;
  /* Ein Stand aus SetStand ODER ein Feld aus SetStands; ohne beides zeichnet die Stage nichts. */
  if (!BarkPipe || BarkCount == 0 || (HeightM <= 0.0 && StandCount == 0)) return;

  double east[3], north[3];
  const double up[3] = {ctx.Up[0], ctx.Up[1], ctx.Up[2]};
  east[0] = -up[1]; east[1] = up[0]; east[2] = 0.0;   /* z_ecef x up, before normalising */
  double el = std::sqrt(east[0] * east[0] + east[1] * east[1]);
  if (el < 1.0e-12) { east[0] = 1.0; east[1] = 0.0; el = 1.0; }
  for (int a = 0; a < 3; a++) east[a] /= el;
  north[0] = up[1] * east[2] - up[2] * east[1];
  north[1] = up[2] * east[0] - up[0] * east[2];
  north[2] = up[0] * east[1] - up[1] * east[0];

  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  u[16] = (float)east[0];  u[17] = (float)east[1];  u[18] = (float)east[2];  u[19] = (float)EastM;
  u[20] = (float)north[0]; u[21] = (float)north[1]; u[22] = (float)north[2]; u[23] = (float)NorthM;
  u[24] = (float)up[0];    u[25] = (float)up[1];    u[26] = (float)up[2];    u[27] = (float)EyeAglM;
  u[28] = (float)SunDir[0]; u[29] = (float)SunDir[1]; u[30] = (float)SunDir[2]; u[31] = NightAmbient;
  u[32] = Look.BarkRgb[0]; u[33] = Look.BarkRgb[1]; u[34] = Look.BarkRgb[2]; u[35] = Look.BarkDark;
  u[36] = Look.BarkFreq; u[37] = Look.BarkRidge; u[38] = (float)HeightM; u[39] = LeafScaleM;
  u[40] = Look.LeafRgb[0]; u[41] = Look.LeafRgb[1]; u[42] = Look.LeafRgb[2];
  Queue.WriteBuffer(Uni, 0, u, sizeof u);

  pass.SetPipeline(BarkPipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, BarkVtx);
  if (StandCount > 0 && StandBuf) {
    pass.SetVertexBuffer(1, StandBuf);
  } else {
    /* Ein einzelner Stand ist der Sonderfall von N: die Bank bleibt eine Instanz. */
    if (!OneStand) {
      const float one[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      OneStand = Upload(one, sizeof one, wgpu::BufferUsage::Vertex);
    }
    pass.SetVertexBuffer(1, OneStand);
  }
  pass.SetIndexBuffer(BarkIdx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(BarkCount, StandCount > 0 ? StandCount : 1u);
  Drawn += (long)BarkCount / 3;

  if (!LeavesOn || LeafCount == 0 || InstCount == 0) return;
  pass.SetPipeline(LeafPipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, LeafVtx);
  pass.SetVertexBuffer(1, LeafInst);
  pass.SetIndexBuffer(LeafIdx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(LeafCount, InstCount);
  Drawn += (long)LeafCount / 3 * (long)InstCount;
}

} // namespace outshine::Render
