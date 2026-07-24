#include "FBSkyViewStage.h"
#include "FBAtmoCommon.h"
#include <string>

namespace FlightBox {

static const char *kSkyViewCS = R"(
@group(0) @binding(0) var svOut : texture_storage_2d<rgba16float, write>;
@group(0) @binding(1) var tLUT : texture_2d<f32>;
@group(0) @binding(2) var lsamp : sampler;
@group(0) @binding(3) var<uniform> A : Atmo;
fn getValFromTLUT(pos : vec3f, sunDir : vec3f) -> vec3f {
  return textureSampleLevel(tLUT, lsamp, tLUTuv(pos, sunDir), 0.0).rgb;
}
fn raymarchScattering(pos0 : vec3f, rayDir : vec3f, sunDir : vec3f, steps : f32) -> vec3f {
  let cosTheta = dot(rayDir, sunDir);
  let miePhase = getMiePhase(cosTheta);
  let rayleighPhase = getRayleighPhase(-cosTheta);
  var lum = vec3f(0.0);
  var transmittance = vec3f(1.0);
  var t = 0.0;
  let atmoDist = rayIntersectSphere(pos0, rayDir, atmosphereRadiusMM);
  let groundDist = rayIntersectSphere(pos0, rayDir, groundRadiusMM);
  var maxDist = atmoDist;
  if (groundDist > 0.0) { maxDist = groundDist; }
  for (var i = 0.0; i < steps; i = i + 1.0) {
    let newT = ((i + 0.3) / steps) * maxDist;
    let dt = newT - t;
    t = newT;
    let newPos = pos0 + t * rayDir;
    let sv = getScatteringValues(newPos);
    let sampleTransmittance = exp(-dt * sv.extinction);
    let sunTransmittance = getValFromTLUT(newPos, sunDir);
    let inScattering = (sv.rayleigh * rayleighPhase + vec3f(sv.mie * miePhase)) * sunTransmittance;
    let scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / sv.extinction;
    lum = lum + scatteringIntegral * transmittance;
    transmittance = transmittance * sampleTransmittance;
  }
  return lum;
}
@compute @workgroup_size(8, 8, 1)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 192u || id.y >= 108u) { return; }
  let uv = (vec2f(f32(id.x), f32(id.y)) + 0.5) / vec2f(192.0, 108.0);
  let azimuth = 2.0 * PI * uv.x;
  let coord = 2.0 * uv.y - 1.0;
  let altitude = sign(coord) * (PI * 0.5) * coord * coord;   /* horizon-dense mapping */
  let ca = cos(altitude);
  let rayDir = A.up.xyz * sin(altitude) + (A.sunTan.xyz * cos(azimuth) + A.side.xyz * sin(azimuth)) * ca;
  let lum = raymarchScattering(A.camPosMm.xyz, rayDir, A.sunDir.xyz, 32.0);
  textureStore(svOut, vec2i(i32(id.x), i32(id.y)), vec4f(lum, 1.0));
}
)";

void FBSkyViewStage::Configure(const FBGpu &gpu, wgpu::TextureView skyLutView, wgpu::TextureView transLutView,
                               wgpu::Sampler lutSamp, wgpu::Buffer atmoBuf) {
  wgpu::ShaderSourceWGSL wgsl{};
  std::string src = std::string(kAtmoCommon) + kSkyViewCS;
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = m;
  cp.compute.entryPoint = "cs";
  Pipe = gpu.Device.CreateComputePipeline(&cp);

  wgpu::BindGroupEntry be[4] = {};
  be[0].binding = 0; be[0].textureView = skyLutView;
  be[1].binding = 1; be[1].textureView = transLutView;
  be[2].binding = 2; be[2].sampler = lutSamp;
  be[3].binding = 3; be[3].buffer = atmoBuf; be[3].size = 11 * 4 * sizeof(float);
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 4;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void FBSkyViewStage::EncodeCompute(const FBFrameContext &, wgpu::ComputePassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.DispatchWorkgroups(24, 14, 1);   /* 192x108 / 8x8 (ceil) */
}

} // namespace FlightBox
