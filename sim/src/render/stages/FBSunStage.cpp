#include "FBSunStage.h"
#include "FBAtmoCommon.h"
#include "FBAtmoSample.h"
#include <string>

namespace FlightBox::Render {

/* Disc = solar transmittance, glow = a soft forward halo. */
static const char *kSunWGSL = R"(
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var tLUT : texture_2d<f32>;
struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.ndc = p;
  return o;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let evs = A.skyExtra.y;
  if (evs <= 0.5) { return vec4f(0.0); }   /* SVS: no sun disc/glow — additive contributes nothing */
  let dir = camRay(A, in.ndc);
  let day = A.skyExtra.x;
  let sa = acos(clamp(dot(dir, A.sunDir.xyz), -1.0, 1.0));
  let sup = smoothstep(-0.06, 0.0, A.sunDir.y * 0.0 + dot(A.sunDir.xyz, A.up.xyz));
  let disc = select(0.0, 1.0, dot(dir, A.sunDir.xyz) > A.params.z);
  let sunT = textureSampleLevel(tLUT, lsamp, tLUTuv(A.camPosMm.xyz, A.sunDir.xyz), 0.0).rgb;
  let glow = (exp(-sa * 7.0) * 0.35 + exp(-sa * 1.5) * 0.12 * day) * kSkyExposure;
  let col = (disc * sunT * A.params.w + glow * vec3f(1.0, 0.80, 0.55)) * sup;
  return vec4f(col, 1.0);
}
)";

void FBSunStage::Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp, wgpu::TextureView transLutView) {
  std::string src = std::string(kAtmoCommon) + kAtmoSample + kSunWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = false;
  ds.depthCompare = wgpu::CompareFunction::Always;   /* draws over the sky, same as the original merged shader */

  wgpu::BlendState blend{};                        /* additive: sums onto the sky draw beneath */
  blend.color.srcFactor = wgpu::BlendFactor::One;  blend.color.dstFactor = wgpu::BlendFactor::One;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;  blend.alpha.dstFactor = wgpu::BlendFactor::One;
  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  ct.blend = &blend;
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
  be[0].binding = 0; be[0].buffer = atmoBuf; be[0].size = kAtmoUniformBytes;
  be[1].binding = 1; be[1].sampler = lutSamp;
  be[2].binding = 2; be[2].textureView = transLutView;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 3;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void FBSunStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace FlightBox::Render
