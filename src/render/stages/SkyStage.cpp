#include "SkyStage.h"
#include "SceneTargets.h"
#include "AtmoCommon.h"
#include "AtmoSample.h"
#include "SceneScale.h"
#include <string>

namespace outshine::Render {

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
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let dir = camRay(A, in.ndc);
  /* Physically-based scattering: day/dusk/night. The CLOUDS are not here — they are one field, the
   * one the ground shadows itself against, and CloudLayerStage draws it (stages/clouds.md). A second
   * noise sheet on the dome is what used to stand here, and it put a cloud in the picture that no
   * shadow on the ground belonged to. */
  let col = skyViewSample(svLUT, lsamp, A, dir);
  /* alpha 0: nothing in the sky is sky-LIT, so the AO composite must leave it alone. */
  return vec4f(col, 1.0);   /* Sun disc/glow (SunStage) + moon (MoonStage) composite additively on top */
}
)";

void SkyStage::Configure(const Gpu &gpu, wgpu::TextureView skyLutView, wgpu::Sampler lutSamp,
                           wgpu::Buffer atmoBuf) {
  std::string src = std::string(kSceneScaleWGSL) + kAtmoCommon + kAtmoSample + kSkyWGSL;
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
  /* The scene pass carries a SECOND attachment; a pipeline recorded into it declares two targets
   * whatever it writes. This one leaves the motion attachment alone — it is world-fixed (or it
   * blends without owning the depth), so the resolve reconstructs its pixels from depth. */
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(false)};
  fs.targetCount = 2;
  fs.targets = cts;
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

void SkyStage::Encode(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);   /* physically-based sky background, the fullscreen triangle */
}

} // namespace outshine::Render
