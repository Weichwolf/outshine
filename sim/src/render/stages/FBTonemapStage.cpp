#include "FBTonemapStage.h"

namespace FlightBox::Render {

static const char *kTonemapWGSL = R"(
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var hdr : texture_2d<f32>;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0)); // fullscreen tri
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  return o;
}
fn aces(x : vec3f) -> vec3f {   // Narkowicz ACES fit
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let scene = textureSample(hdr, samp, in.uv).rgb;
  return vec4f(aces(scene), 1.0);   // linear [0,1]; the sRGB swapchain view encodes on write
}
)";

void FBTonemapStage::Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView) {
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kTonemapWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = gpu.Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;   /* no vertex buffers — the triangle comes from vertex_index */
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;       /* no depthStencil on the tonemap pass */
  TonemapPipe = gpu.Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[2] = {};
  be[0].binding = 0; be[0].sampler = samp;
  be[1].binding = 1; be[1].textureView = hdrView;
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = TonemapPipe.GetBindGroupLayout(0);
  bgd.entryCount = 2;
  bgd.entries = be;
  TonemapBind = gpu.Device.CreateBindGroup(&bgd);
}

void FBTonemapStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(TonemapPipe);
  pass.SetBindGroup(0, TonemapBind);
  pass.Draw(3);
}

} // namespace FlightBox::Render
