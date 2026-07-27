#include "FBCloudMipDownStage.h"

namespace FlightBox::Render {

static const char *kMipDownCS = R"(
@group(0) @binding(0) var src : texture_3d<f32>;
@group(0) @binding(1) var outTex : texture_storage_3d<rgba8unorm, write>;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  let d = textureDimensions(outTex);
  if (id.x >= d.x || id.y >= d.y || id.z >= d.z) { return; }
  let c = vec3i(id) * 2;
  var s = vec4f(0.0);
  for (var k = 0; k < 2; k++) { for (var j = 0; j < 2; j++) { for (var i = 0; i < 2; i++) {
    s += textureLoad(src, c + vec3i(i, j, k), 0);   /* box-average 2x2x2 from the previous level */
  }}}
  textureStore(outTex, vec3i(id), s / 8.0);
}
)";

void FBCloudMipDownStage::Configure(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kMipDownCS;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);
  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = m;
  cp.compute.entryPoint = "cs";
  Pipe = Device.CreateComputePipeline(&cp);
}

void FBCloudMipDownStage::Downsample(wgpu::TextureView srcView, wgpu::TextureView dstView, uint32_t dstSize) {
  wgpu::BindGroupEntry be[2]{};
  be[0].binding = 0; be[0].textureView = srcView;
  be[1].binding = 1; be[1].textureView = dstView;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 2;
  bg.entries = be;
  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = enc.BeginComputePass();
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Device.CreateBindGroup(&bg));
  uint32_t g = (dstSize + 3) / 4;
  pass.DispatchWorkgroups(g, g, g);
  pass.End();
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);
}

} // namespace FlightBox::Render
