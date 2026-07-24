#include "FBSkyStage.h"
#include "FBAtmoCommon.h"
#include "FBAtmoSample.h"
#include <string>

namespace FlightBox {

static const char *kSkyWGSL = R"(
@group(0) @binding(0) var svLUT : texture_2d<f32>;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var tLUT : texture_2d<f32>;
@group(0) @binding(3) var<uniform> A : Atmo;
@group(0) @binding(4) var moonTex : texture_2d<f32>;   /* NASA LROC equirect albedo */
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
  let dir = normalize(A.camFwd.xyz + in.ndc.x * A.params.x * A.params.y * A.camRight.xyz
                                   + in.ndc.y * A.params.x * A.camUp.xyz);
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

  /* Moon as a lit sphere: within its angular radius, reconstruct the front-hemisphere normal, sample
   * the NASA albedo, and light it with the REAL sun direction — phase/terminator emerge physically. */
  let moonUp = A.moonDir.xyz;
  if (evs > 0.5 && dot(moonUp, A.up.xyz) > -0.03) {   /* moon at/above the local horizon */
    let mr = A.skyExtra.w;               /* moon angular radius: 0.0045 rad (real) x FB_MOON_SCALE */
    let cosA = dot(dir, moonUp);
    if (cosA > cos(mr * 3.0)) {
      let mRight = normalize(cross(A.up.xyz, moonUp));
      let mUp = normalize(cross(moonUp, mRight));
      let toObs = -moonUp;                 /* moon centre -> observer */
      let u = dot(dir, mRight) / mr;
      let v = dot(dir, mUp) / mr;
      let r2 = u * u + v * v;
      if (r2 < 1.0) {
        let nrm = mRight * u + mUp * v + toObs * -sqrt(1.0 - r2);   /* sphere normal, front hemi */
        let lon = atan2(dot(nrm, mRight), dot(nrm, -toObs));
        let lat = asin(clamp(dot(nrm, mUp), -1.0, 1.0));
        let muv = vec2f(0.5 + lon / (2.0 * 3.14159265), 0.5 - lat / 3.14159265);
        let alb = textureSampleLevel(moonTex, lsamp, muv, 0.0).rgb;
        let lit = max(dot(nrm, A.sunDir.xyz), 0.0);
        let earth = 0.06 * A.moonDir.w;    /* faint earthshine on the dark limb */
        let moonCol = alb * (lit + earth) * 12.0 * (1.0 - 0.6 * day);
        col = col + moonCol;
      }
    }
  }

  /* Sun disc + glow (EVS, sun above horizon). Disc = solar transmittance; glow = soft forward halo. */
  if (evs > 0.5) {
    let sa = acos(clamp(dot(dir, A.sunDir.xyz), -1.0, 1.0));
    let sup = smoothstep(-0.06, 0.0, A.sunDir.y * 0.0 + dot(A.sunDir.xyz, A.up.xyz));
    let disc = select(0.0, 1.0, dot(dir, A.sunDir.xyz) > A.params.z);
    let sunT = textureSampleLevel(tLUT, lsamp, tLUTuv(A.camPosMm.xyz, A.sunDir.xyz), 0.0).rgb;
    let glow = (exp(-sa * 7.0) * 0.35 + exp(-sa * 1.5) * 0.12 * day) * kSkyExposure;
    col = col + (disc * sunT * A.params.w + glow * vec3f(1.0, 0.80, 0.55)) * sup;
  }
  return vec4f(col, 1.0);
}
)";

void FBSkyStage::Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp,
                           wgpu::TextureView transLutView, wgpu::Buffer atmoBuf, wgpu::TextureView moonTexView) {
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

  wgpu::BindGroupEntry be[5] = {};
  be[0].binding = 0; be[0].textureView = skyLutView;
  be[1].binding = 1; be[1].sampler = lutSamp;
  be[2].binding = 2; be[2].textureView = transLutView;
  be[3].binding = 3; be[3].buffer = atmoBuf; be[3].size = 11 * 4 * sizeof(float);
  be[4].binding = 4; be[4].textureView = moonTexView;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 5;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void FBSkyStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);   /* physically-based sky background, the fullscreen triangle */
}

} // namespace FlightBox
