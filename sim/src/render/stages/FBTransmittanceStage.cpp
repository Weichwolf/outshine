#include "FBTransmittanceStage.h"
#include "FBAtmoCommon.h"
#include <string>

namespace FlightBox::Render {

static const char *kTransmittanceCS = R"(
@group(0) @binding(0) var tOut : texture_storage_2d<rgba16float, write>;
fn getSunTransmittance(pos : vec3f, sunDir : vec3f) -> vec3f {
  if (rayIntersectSphere(pos, sunDir, groundRadiusMM) > 0.0) { return vec3f(0.0); }
  let atmoDist = rayIntersectSphere(pos, sunDir, atmosphereRadiusMM);
  let steps = 40.0;
  var t = 0.0;
  var transmittance = vec3f(1.0);
  for (var i = 0.0; i < steps; i = i + 1.0) {
    let newT = ((i + 0.3) / steps) * atmoDist;
    let dt = newT - t;
    t = newT;
    let sv = getScatteringValues(pos + t * sunDir);
    transmittance = transmittance * exp(-dt * sv.extinction);
  }
  return transmittance;
}
@compute @workgroup_size(8, 8, 1)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 256u || id.y >= 64u) { return; }
  let uv = (vec2f(f32(id.x), f32(id.y)) + 0.5) / vec2f(256.0, 64.0);
  let sunCosTheta = 2.0 * uv.x - 1.0;
  let sunTheta = acos(clamp(sunCosTheta, -1.0, 1.0));
  let height = mix(groundRadiusMM, atmosphereRadiusMM, uv.y);
  let pos = vec3f(0.0, height, 0.0);
  let sunDir = normalize(vec3f(0.0, sunCosTheta, -sin(sunTheta)));
  textureStore(tOut, vec2i(i32(id.x), i32(id.y)), vec4f(getSunTransmittance(pos, sunDir), 1.0));
}
)";

void FBTransmittanceStage::Configure(const FBGpu &gpu, wgpu::TextureView transLutView) {
  wgpu::ShaderSourceWGSL wgsl{};
  std::string src = std::string(kAtmoCommon) + kTransmittanceCS;
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = m;
  cp.compute.entryPoint = "cs";
  Pipe = gpu.Device.CreateComputePipeline(&cp);

  wgpu::BindGroupEntry be[1] = {};
  be[0].binding = 0;
  be[0].textureView = transLutView;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 1;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void FBTransmittanceStage::EncodeCompute(const FBFrameContext &, wgpu::ComputePassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.DispatchWorkgroups(32, 8, 1);   /* 256x64 / 8x8 */
}

} // namespace FlightBox::Render
