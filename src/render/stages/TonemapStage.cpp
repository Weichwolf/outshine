#include "TonemapStage.h"

#include <string>
#include <vector>

#include "ExposureStage.h"

namespace outshine::Render {

namespace {

/* One triangle covering the frame, so there is no vertex buffer and no index buffer to own. */
const char *kFullScreenWGSL = R"(
struct VOut { @builtin(position) pos : vec4f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  o.pos = vec4f(c[i], 0.0, 1.0);
  return o;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let px = vec2i(in.pos.xy);
  return displayed(textureLoad(scene, px, 0), in.pos.xy, textureLoad(sceneDepth, px, 0));
}
)";

} // namespace

void TonemapStage::Configure(const Gpu &gpu, wgpu::TextureView linearView,
                             wgpu::TextureView depthView, wgpu::TextureView aoView,
                             wgpu::Buffer meterBuf, const DisplayOptions &options) {
  /* NO SAMPLER. The transfer reads one texel per fragment at the fragment's own coordinate, so it is
   * a load and not a sample -- and a sampler bound but unused is a binding the pipeline's derived
   * layout does not contain. */
  std::string bindings = "@group(0) @binding(1) var scene : texture_2d<f32>;\n"
                         "@group(0) @binding(4) var sceneDepth : texture_depth_2d;\n";
  if (options.HasOcclusion) { bindings += "@group(0) @binding(2) var aoTex : texture_2d<f32>;\n"; }
  if (options.HasMeter) {
    bindings += "struct Meter { expScale : f32, keyLog : f32, horizE : f32, pad0 : f32 };\n"
                "@group(0) @binding(3) var<storage, read> meter : Meter;\n";
  }
  const std::string source =
      std::string(kFilmicWGSL) + bindings + DisplayWGSL(options) + kFullScreenWGSL;

  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = source.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule module = gpu.Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;
  wgpu::FragmentState fs{};
  fs.module = module;
  fs.targetCount = 1;
  fs.targets = &ct;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = module;
  rp.fragment = &fs;
  Pipe = gpu.Device.CreateRenderPipeline(&rp);

  std::vector<wgpu::BindGroupEntry> entries;
  wgpu::BindGroupEntry entry{};
  entry.binding = 1;
  entry.textureView = linearView;
  entries.push_back(entry);
  entry = wgpu::BindGroupEntry{};
  entry.binding = 4;
  entry.textureView = depthView;
  entries.push_back(entry);
  if (options.HasOcclusion) {
    entry = wgpu::BindGroupEntry{};
    entry.binding = 2;
    entry.textureView = aoView;
    entries.push_back(entry);
  }
  if (options.HasMeter) {
    entry = wgpu::BindGroupEntry{};
    entry.binding = 3;
    entry.buffer = meterBuf;
    entry.size = ExposureStage::kMeterBytes;
    entries.push_back(entry);
  }
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = entries.size();
  bg.entries = entries.data();
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void TonemapStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!Pipe) return;
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace outshine::Render
