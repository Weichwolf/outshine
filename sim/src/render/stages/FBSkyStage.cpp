#include "FBSkyStage.h"
#include "FBAtmoCommon.h"
#include "FBAtmoSample.h"
#include <string>

namespace FlightBox::Render {

static const char *kSkyWGSL = R"(
@group(0) @binding(0) var svLUT : texture_2d<f32>;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var<uniform> A : Atmo;
struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.ndc = p;
  return o;
}
fn h21(p : vec2f) -> f32 { return fract(sin(dot(p, vec2f(127.1, 311.7))) * 43758.5453); }
fn vnoise(p : vec2f) -> f32 {
  let i = floor(p); var f = fract(p); f = f * f * (3.0 - 2.0 * f);
  let a = h21(i); let b = h21(i + vec2f(1.0, 0.0));
  let c = h21(i + vec2f(0.0, 1.0)); let d = h21(i + vec2f(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let dir = camRay(A, in.ndc);
  var col = skyViewSample(svLUT, lsamp, A, dir);   /* physically-based scattering: day/dusk/night */

  let evs = A.skyExtra.y;        /* SVS (OSM) suppresses every real-sky extra below (constant day) */
  let day = A.skyExtra.x;
  let hgt = dot(dir, A.up.xyz);  /* sin(view elevation) */

  /* Clouds: value-noise sheet on the dome, lit by day, dark occluders by night (stars read through). */
  let cloud = A.skyExtra.z;
  if (evs > 0.5 && cloud > 0.01 && hgt > 0.04) {
    let side = normalize(cross(A.up.xyz, A.camFwd.xyz));
    let fwd = normalize(cross(side, A.up.xyz));
    let cuv = vec2f(dot(dir, side), dot(dir, fwd)) / (hgt + 0.25) * 1.6;
    let n = 0.55 * vnoise(cuv) + 0.35 * vnoise(cuv * 2.3) + 0.15 * vnoise(cuv * 4.7);
    let cl = smoothstep(1.0 - cloud, 1.0 - cloud * 0.35, n) * smoothstep(0.04, 0.22, hgt);
    let cc = mix(vec3f(0.03, 0.04, 0.06), vec3f(0.95, 0.96, 1.0), day) * kSkyExposure;
    col = mix(col, cc, cl * 0.85);
  }
  return vec4f(col, 1.0);   /* Sun disc/glow (FBSunStage) + moon (FBMoonStage) composite additively on top */
}
)";

void FBSkyStage::Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp,
                           wgpu::Buffer atmoBuf) {
  std::string src = std::string(kAtmoCommon) + kAtmoSample + kSkyWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = false;
  ds.depthCompare = wgpu::CompareFunction::Always;
  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  Pipe = gpu.Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].textureView = skyLutView;
  be[1].binding = 1; be[1].sampler = lutSamp;
  be[2].binding = 2; be[2].buffer = atmoBuf; be[2].size = kAtmoUniformBytes;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 3;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void FBSkyStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);   /* physically-based sky background, the fullscreen triangle */
}

} // namespace FlightBox::Render
