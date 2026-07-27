#include "FBCloudCellBakeStage.h"
#include "FBCloudNoiseCommon.h"
#include <string>

namespace FlightBox {

/* Tileable F1-round bumps pow(1-F1,2) at ~57 texel/cell, which is what makes the puffs ROUND rather
 * than angular plates. The octave frequencies must be INTEGER to wrap seamlessly. */
static const char *kCloudCellCS = R"(
@group(0) @binding(0) var outTex : texture_storage_2d<rgba8unorm, write>;
@compute @workgroup_size(8, 8)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 512u || id.y >= 512u) { return; }
  let uv = (vec2f(f32(id.x), f32(id.y)) + 0.5) / 512.0;
  let cellF = clamp(pow(worley2D(uv, 7.0), 2.0) * 0.72 + pow(worley2D(uv * 1.9 + 5.3, 4.0), 2.0) * 0.28, 0.0, 1.0);
  textureStore(outTex, vec2i(i32(id.x), i32(id.y)), vec4f(cellF, 0.0, 0.0, 1.0));
}
)";

void FBCloudCellBakeStage::Configure(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  const uint32_t cn = 512;

  wgpu::TextureDescriptor stD{};
  stD.size = {cn, cn, 1};
  stD.format = wgpu::TextureFormat::RGBA8Unorm;
  stD.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::CopySrc;
  wgpu::Texture stor = Device.CreateTexture(&stD);

  wgpu::TextureDescriptor smD{};
  smD.size = {cn, cn, 1};
  smD.format = wgpu::TextureFormat::RGBA8Unorm;
  smD.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Tex = Device.CreateTexture(&smD);

  wgpu::ShaderSourceWGSL w{};
  std::string src = std::string(kCloudNoiseCommon) + kCloudCellCS;
  w.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &w;
  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = Device.CreateShaderModule(&smd);
  cp.compute.entryPoint = "cs";
  wgpu::ComputePipeline pipe = Device.CreateComputePipeline(&cp);

  wgpu::BindGroupEntry be{}; be.binding = 0; be.textureView = stor.CreateView();
  wgpu::BindGroupDescriptor bg{}; bg.layout = pipe.GetBindGroupLayout(0); bg.entryCount = 1; bg.entries = &be;
  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = enc.BeginComputePass();
  pass.SetPipeline(pipe); pass.SetBindGroup(0, Device.CreateBindGroup(&bg));
  pass.DispatchWorkgroups(cn / 8, cn / 8, 1); pass.End();
  wgpu::TexelCopyTextureInfo s{}, d{}; s.texture = stor; d.texture = Tex; wgpu::Extent3D ext{cn, cn, 1};
  enc.CopyTextureToTexture(&s, &d, &ext);
  wgpu::CommandBuffer cmd = enc.Finish(); Queue.Submit(1, &cmd);
}

} // namespace FlightBox
