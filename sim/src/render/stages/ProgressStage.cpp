#include "ProgressStage.h"

namespace outshine::Render {

/* A fullscreen triangle; the bar is a rectangle test in pixels, so its edges are the same thickness
 * at every frame size. The target is sRGB, so the constants below are LINEAR reflectances and the
 * store encodes them — 0.216 linear is display code 128. */
static const char *kProgressWGSL = R"(
struct U { frac : f32, width : f32, height : f32, pad : f32 };
@group(0) @binding(0) var<uniform> u : U;
struct VO { @builtin(position) pos : vec4f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VO {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VO;
  o.pos = vec4f(c[i], 0.0, 1.0);
  return o;
}
@fragment fn fs(in : VO) -> @location(0) vec4f {
  let p = in.pos.xy;
  let barW = u.width * 0.5;
  let barH = max(4.0, u.height * 0.012);
  let x0 = (u.width - barW) * 0.5;
  let y0 = u.height * 0.5 - barH * 0.5;
  let inside = p.x >= x0 && p.x <= x0 + barW && p.y >= y0 && p.y <= y0 + barH;
  if (!inside) { return vec4f(0.0, 0.0, 0.0, 1.0); }
  let filled = (p.x - x0) / barW <= u.frac;
  if (filled) { return vec4f(0.6, 0.6, 0.6, 1.0); }
  return vec4f(0.02, 0.02, 0.02, 1.0);
}
)";

void ProgressStage::Init(const Gpu &gpu) {
  wgpu::BufferDescriptor bd{};
  bd.size = 16;
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = gpu.Device.CreateBuffer(&bd);

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kProgressWGSL;
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

  wgpu::BindGroupEntry be{};
  be.binding = 0;
  be.buffer = Uni;
  be.size = 16;
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 1;
  bgd.entries = &be;
  Bind = gpu.Device.CreateBindGroup(&bgd);

  const float init[4] = {0.0f, (float)gpu.Width, (float)gpu.Height, 0.0f};
  gpu.Queue.WriteBuffer(Uni, 0, init, sizeof init);
}

void ProgressStage::SetFraction(const wgpu::Queue &queue, float fraction) {
  if (!Uni) return;
  queue.WriteBuffer(Uni, 0, &fraction, sizeof fraction);
}

void ProgressStage::Encode(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  if (!Pipe) return;
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace outshine::Render
