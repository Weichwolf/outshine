#include "MultiScatterStage.h"
#include "AtmoCommon.h"
#include <string>

namespace outshine::Render {

static const char *kMultiScatterCS = R"(
@group(0) @binding(0) var msOut : texture_storage_2d<rgba16float, write>;
@group(0) @binding(1) var tLUT : texture_2d<f32>;
@group(0) @binding(2) var lsamp : sampler;

const kMsSteps : f32 = 20.0;
const kMsDirsSqrt : i32 = 8;
/* The average albedo of the surface the atmosphere stands on. Land runs 0.10-0.25 and open water
 * 0.06; 0.15 is the value Hillaire's reference implementation uses and it changes the zenith by
 * under 3 % either way. */
const kGroundAlbedo : f32 = 0.15;

fn msSunTransmittance(pos : vec3f, sunDir : vec3f) -> vec3f {
  return textureSampleLevel(tLUT, lsamp, tLUTuv(pos, sunDir), 0.0).rgb;
}

/* Second-order in-scatter along one direction, plus the fraction of light that stays in the system.
 * The pair is what closes the geometric series: psi = L2 / (1 - f). */
struct MsPair { lum : vec3f, fms : vec3f };

fn msOneDir(pos : vec3f, sunDir : vec3f, rayDir : vec3f) -> MsPair {
  var o : MsPair;
  o.lum = vec3f(0.0);
  o.fms = vec3f(0.0);
  let atmoDist = rayIntersectSphere(pos, rayDir, atmosphereRadiusMM);
  let groundDist = rayIntersectSphere(pos, rayDir, groundRadiusMM);
  var tMax = atmoDist;
  if (groundDist > 0.0) { tMax = groundDist; }
  let cosT = dot(rayDir, sunDir);
  let miePh = getMiePhase(cosT);
  let rayPh = getRayleighPhase(-cosT);
  var transm = vec3f(1.0);
  var t = 0.0;
  for (var i = 0.0; i < kMsSteps; i = i + 1.0) {
    let newT = ((i + 0.3) / kMsSteps) * tMax;
    let dt = newT - t;
    t = newT;
    let newPos = pos + t * rayDir;
    let sv = getScatteringValues(newPos);
    let stepT = exp(-dt * sv.extinction);
    let scatNoPhase = sv.rayleigh + vec3f(sv.mie);
    o.fms = o.fms + transm * ((scatNoPhase - scatNoPhase * stepT) / sv.extinction);
    let sunT = msSunTransmittance(newPos, sunDir);
    let inScat = (sv.rayleigh * rayPh + vec3f(sv.mie * miePh)) * sunT;
    o.lum = o.lum + transm * ((inScat - inScat * stepT) / sv.extinction);
    transm = transm * stepT;
  }
  if (groundDist > 0.0 && dot(pos, sunDir) > 0.0) {
    let hit = normalize(pos + groundDist * rayDir) * groundRadiusMM;
    o.lum = o.lum + transm * kGroundAlbedo * msSunTransmittance(hit, sunDir);
  }
  return o;
}

@compute @workgroup_size(8, 8, 1)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 32u || id.y >= 32u) { return; }
  let uv = (vec2f(f32(id.x), f32(id.y)) + 0.5) / vec2f(32.0, 32.0);
  let sunCosTheta = 2.0 * uv.x - 1.0;
  let height = mix(groundRadiusMM, atmosphereRadiusMM, uv.y);
  let pos = vec3f(0.0, height, 0.0);
  let sunDir = normalize(vec3f(0.0, sunCosTheta, -sin(acos(clamp(sunCosTheta, -1.0, 1.0)))));

  var lumTotal = vec3f(0.0);
  var fmsTotal = vec3f(0.0);
  let invN = 1.0 / f32(kMsDirsSqrt * kMsDirsSqrt);
  for (var i = 0; i < kMsDirsSqrt; i = i + 1) {
    for (var j = 0; j < kMsDirsSqrt; j = j + 1) {
      let theta = PI * (f32(i) + 0.5) / f32(kMsDirsSqrt);
      let phi = acos(clamp(1.0 - 2.0 * (f32(j) + 0.5) / f32(kMsDirsSqrt), -1.0, 1.0));
      let sp = sin(phi);
      let rayDir = vec3f(sp * sin(theta), cos(phi), sp * cos(theta));
      let p = msOneDir(pos, sunDir, rayDir);
      lumTotal = lumTotal + p.lum * invN;
      fmsTotal = fmsTotal + p.fms * invN;
    }
  }
  let psi = lumTotal / max(vec3f(1.0) - fmsTotal, vec3f(1.0e-4));
  textureStore(msOut, vec2i(i32(id.x), i32(id.y)), vec4f(psi, 1.0));
}
)";

void MultiScatterStage::Configure(const Gpu &gpu, wgpu::TextureView msLutView,
                                  wgpu::TextureView transLutView, wgpu::Sampler lutSamp) {
  wgpu::ShaderSourceWGSL wgsl{};
  std::string src = std::string(kAtmoCommon) + kMultiScatterCS;
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = m;
  cp.compute.entryPoint = "cs";
  Pipe = gpu.Device.CreateComputePipeline(&cp);

  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].textureView = msLutView;
  be[1].binding = 1; be[1].textureView = transLutView;
  be[2].binding = 2; be[2].sampler = lutSamp;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 3;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void MultiScatterStage::EncodeCompute(const FrameContext &, wgpu::ComputePassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.DispatchWorkgroups(4, 4, 1);   /* 32x32 / 8x8 */
}

} // namespace outshine::Render
