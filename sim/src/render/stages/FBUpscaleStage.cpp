#include "FBUpscaleStage.h"

namespace FlightBox {

/* Upscale/present: a fullscreen triangle sampling the fixed-720p FrameTex (linear) onto the swapchain
 * at the display resolution. SVS goes straight through; a future EVS WebCodecs link would swap the
 * sampled source for the decoded frame. Verbatim FBRenderer::CreatePresent's shader/pipeline half. */
static const char *kUpscaleWGSL = R"(
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var frame : texture_2d<f32>;
struct VO { @builtin(position) pos : vec4f, @location(0) uv : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VO {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VO;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);   /* flip Y: NDC up -> texture down */
  return o;
}
@fragment fn fs(in : VO) -> @location(0) vec4f {
  return vec4f(textureSampleLevel(frame, samp, in.uv, 0.0).rgb, 1.0);
}
)";

void FBUpscaleStage::Configure(const FBGpu &gpu, wgpu::TextureView frameView) {
  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  Samp = gpu.Device.CreateSampler(&sd);

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kUpscaleWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = gpu.Device.CreateShaderModule(&smd);
  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  Pipe = gpu.Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[2] = {};
  be[0].binding = 0; be[0].sampler = Samp;
  be[1].binding = 1; be[1].textureView = frameView;
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 2;
  bgd.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bgd);
}

void FBUpscaleStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace FlightBox
