#include "WaterStage.h"
#include "SceneTargets.h"
#include "SceneScale.h"
#include "SurfaceLight.h"
#include <string>

namespace outshine::Render {

static const char *kWaterWGSL = R"(
struct W { mvp : mat4x4f, anc : vec4f, sun : vec4f, up : vec4f };
@group(0) @binding(0) var<uniform> w : W;
@group(0) @binding(1) var<storage, read> I : Irr;

/* [SET] REFLECTANCE of the body, not of the surface: 0.05 is the diffuse return of a few metres of
 * water column (ground-materials.json `water`). What makes water read as water is the SPECULAR half,
 * and that is stage 2 — until then this is an honest dark Lambertian sheet and looks like one. */
const kWater : vec3f = vec3f(0.035, 0.045, 0.050);

struct WOut { @builtin(position) pos : vec4f, @location(0) nrm : vec3f, @location(1) rel : vec3f };

@vertex fn vs(@location(0) p : vec3f, @location(1) n : vec3f) -> WOut {
  var o : WOut;
  o.rel = p + w.anc.xyz;
  o.pos = w.mvp * vec4f(o.rel, 1.0);
  o.nrm = n;
  return o;
}

struct WFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
@fragment fn fs(in : WOut) -> WFrag {
  let upW = normalize(w.up.xyz);
  let nrmW = normalize(in.nrm);
  let sunW = normalize(w.sun.xyz);
  var o : WFrag;
  o.col = litRadiance(I, kWater, 1.0, nrmW, upW, sunW, 1.0, 1.0, 1.0, w.up.w);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

void WaterStage::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + kVelocityWGSL + kWaterWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attr[2] = {};
  attr[0].format = wgpu::VertexFormat::Float32x3; attr[0].offset = 0;  attr[0].shaderLocation = 0;
  attr[1].format = wgpu::VertexFormat::Float32x3; attr[1].offset = 12; attr[1].shaderLocation = 1;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 6 * sizeof(float);
  vbl.attributeCount = 2;
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
  /* An OSM ring's winding is not reliable and a fan may face either way. */
  rp.primitive.cullMode = wgpu::CullMode::None;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be[2] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = Light.Irradiance; be[1].size = wgpu::kWholeSize;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 2;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
}

void WaterStage::SetMesh(const float *verts, uint32_t nverts, const double anchor[3]) {
  NVerts = nverts;
  for (int i = 0; i < 3; i++) Anchor[i] = anchor[i];
  if (nverts == 0 || !Device) return;
  wgpu::BufferDescriptor bd{};
  bd.size = (uint64_t)nverts * 6 * sizeof(float);
  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  Vtx = Device.CreateBuffer(&bd);
  Queue.WriteBuffer(Vtx, 0, verts, (size_t)nverts * 6 * sizeof(float));
}

void WaterStage::SetSun(const double sunEcef[3], const double up[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) { SunDir[i] = sunEcef[i]; Up[i] = up[i]; }
  NightAmbient = nightAmbient;
}

void WaterStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (NVerts == 0 || !Vtx) return;
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  for (int i = 0; i < 3; i++) u[16 + i] = (float)(Anchor[i] - ctx.Eye[i]);
  for (int i = 0; i < 3; i++) u[20 + i] = (float)SunDir[i];
  u[23] = (float)ctx.DayFactor;
  for (int i = 0; i < 3; i++) u[24 + i] = (float)Up[i];
  u[27] = NightAmbient;
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx);
  pass.Draw(NVerts);
}

} // namespace outshine::Render
