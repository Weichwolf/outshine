#include "BenchGroundStage.h"
#include "SceneTargets.h"
#include "SceneScale.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this stage. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "SurfaceLight.h"

#include <cmath>
#include <string>

namespace outshine::Render {

static const char *kBenchGroundWGSL = R"(
struct B {
  mvp : mat4x4f,
  ax  : vec4f,   // ECEF east axis,  w = plane radius (m)
  ay  : vec4f,   // ECEF north axis, w = grid spacing (m)
  az  : vec4f,   // ECEF up axis,    w = the plane's offset below the eye (m)
  sun : vec4f,   // ECEF sun direction, w = this frame's ambient night floor
  col : vec4f,   // rgb substrate reflectance, w = the neutral card's
  rf0 : vec4f,   // card centre east/north (m from the camera), half width (m), height (m)
  rf1 : vec4f,   // the card's width axis in east/north, z = 1 while the card stands
};
@group(0) @binding(0) var<uniform> b : B;
@group(0) @binding(1) var<storage, read> I : Irr;

struct BOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) plane : vec2f,
              @location(2) @interpolate(flat) card : f32, @location(3) nrm : vec3f };

@vertex fn vs(@builtin(vertex_index) vi : u32) -> BOut {
  var o : BOut;
  let qx = array<f32, 6>(-1.0, 1.0, -1.0, -1.0, 1.0, 1.0);
  let qy = array<f32, 6>(-1.0, -1.0, 1.0, 1.0, -1.0, 1.0);
  let k = vi % 6u;
  var relV : vec3f;
  var planeV = vec2f(0.0);
  var nrmV = b.az.xyz;
  var isCard = 0.0;
  if (vi < 6u) {
    let r = b.ax.w;
    planeV = vec2f(qx[k] * r, qy[k] * r);
    relV = b.ax.xyz * planeV.x + b.ay.xyz * planeV.y - b.az.xyz * b.az.w;
  } else {
    let latV = b.ax.xyz * b.rf1.x + b.ay.xyz * b.rf1.y;
    let ctrV = b.ax.xyz * b.rf0.x + b.ay.xyz * b.rf0.y - b.az.xyz * b.az.w;
    relV = ctrV + latV * (qx[k] * b.rf0.z) + b.az.xyz * ((qy[k] * 0.5 + 0.5) * b.rf0.w);
    /* Upright and square to the sight line: rotating the width axis a quarter turn in the ground
     * plane is the camera's own forward, and the face that shows is the one it points back along. */
    nrmV = -(b.ax.xyz * (-b.rf1.y) + b.ay.xyz * b.rf1.x);
    isCard = 1.0;
  }
  o.pos = b.mvp * vec4f(relV, 1.0);
  o.rel = relV;
  o.plane = planeV;
  o.nrm = nrmV;
  o.card = isCard;
  return o;
}

/* The RULER, and it has to survive being looked at nearly edge-on: the line is one PIXEL footprint
 * wide rather than a fixed width in metres, and it fades out once one footprint covers a third of the
 * spacing — past that a drawn grid is aliasing and not a scale. */
fn ruleAxis(v : f32, spacing : f32, foot : f32) -> f32 {
  let d = abs(fract(v / spacing - 0.5) - 0.5) * spacing;
  let w = max(foot, spacing * 0.004);
  let line = 1.0 - smoothstep(w, w * 2.5, d);
  let fade = 1.0 - smoothstep(spacing * 0.10, spacing * 0.33, foot);
  return line * fade;
}

@fragment fn fs(in : BOut) -> @location(0) vec4f {
  let upB = normalize(b.az.xyz);
  let sunB = normalize(b.sun.xyz);
  let nB = normalize(in.nrm);
  /* Outside every branch: a derivative in non-uniform control flow is undefined, and the card's own
   * plane coordinate is a constant that costs nothing to differentiate. */
  let foot = max(max(fwidth(in.plane.x), fwidth(in.plane.y)), 1.0e-6);
  let rule = max(ruleAxis(in.plane.x, b.ay.w, foot), ruleAxis(in.plane.y, b.ay.w, foot))
           * (1.0 - in.card);
  /* HALF THE LOCAL REFLECTANCE, so the ruler is one stop down on whatever it is ruled over and can
   * tint nothing: a fixed neutral line over a coloured substrate would be a second material. */
  let alb = select(b.col.rgb, vec3f(b.col.w), in.card > 0.5) * (1.0 - 0.5 * rule);
  let sunVis = 1.0;
  return litRadiance(I, alb, 1.0, nB, upB, sunB, sunVis,
                     1.0, 1.0, b.sun.w);
}
)";

void BenchGroundStage::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL
                        + kBenchGroundWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;   /* reversed-Z, as every scene-pass surface */

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  rp.vertex.bufferCount = 0;
  wgpu::FragmentState fs{};
  fs.module = m;
  /* The scene pass carries a SECOND attachment; a pipeline recorded into it declares two targets
   * whatever it writes. This one leaves the motion attachment alone — it is world-fixed (or it
   * blends without owning the depth), so the resolve reconstructs its pixels from depth. */
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(false)};
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
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

void BenchGroundStage::SetPlane(double eyeAglM, double radiusM, double gridM) {
  EyeAglM = eyeAglM;
  RadiusM = radiusM;
  GridM = gridM;
}

void BenchGroundStage::SetSubstrate(const float linearRgb[3]) {
  for (int i = 0; i < 3; i++) Substrate[i] = linearRgb[i];
}

void BenchGroundStage::SetSun(const double sunEcef[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) SunDir[i] = sunEcef[i];
  NightAmbient = nightAmbient;
}

void BenchGroundStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!Pipe || RadiusM <= 0.0) return;

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
  u[16] = (float)east[0];  u[17] = (float)east[1];  u[18] = (float)east[2];  u[19] = (float)RadiusM;
  u[20] = (float)north[0]; u[21] = (float)north[1]; u[22] = (float)north[2]; u[23] = (float)GridM;
  u[24] = (float)up[0];    u[25] = (float)up[1];    u[26] = (float)up[2];    u[27] = (float)EyeAglM;
  u[28] = (float)SunDir[0]; u[29] = (float)SunDir[1]; u[30] = (float)SunDir[2]; u[31] = NightAmbient;
  u[32] = Substrate[0]; u[33] = Substrate[1]; u[34] = Substrate[2]; u[35] = kCardAlbedo;
  const bool cardOn = Card.HalfWidthM > 0.0 && Card.HeightM > 0.0;
  u[36] = (float)Card.EastM; u[37] = (float)Card.NorthM;
  u[38] = (float)Card.HalfWidthM; u[39] = (float)Card.HeightM;
  u[40] = (float)Card.LatE; u[41] = (float)Card.LatN; u[42] = cardOn ? 1.0f : 0.0f;
  Queue.WriteBuffer(Uni, 0, u, sizeof u);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(cardOn ? 12u : 6u, 1);
}

} // namespace outshine::Render
