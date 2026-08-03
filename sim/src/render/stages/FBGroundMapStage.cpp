#include "FBGroundMapStage.h"

#include <cmath>

namespace FlightBox::Render {

/* One shader: the bay rectangle, and inside it the sector re-projected from the (azimuth, range)
 * raster the radar published. Everything outside the sector is opaque black — the bay must cover the
 * out-the-window picture on its own, because the page skips its veil where video is expected. */
static const char *kGroundMapWGSL = R"(
struct GU {
  rect  : vec4f,   // x0, y0, x1, y1 in frame pixels
  fan   : vec4f,   // apexX, apexY, radius, azHalf (radians)
  frame : vec4f,   // 2/width, 2/height, unused, unused
};
@group(0) @binding(0) var<uniform> gu : GU;
@group(0) @binding(1) var gsamp : sampler;
@group(0) @binding(2) var graster : texture_2d<f32>;
struct GVO { @builtin(position) pos : vec4f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> GVO {
  var q = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                          vec2f(1.0, 0.0), vec2f(1.0, 1.0), vec2f(0.0, 1.0));
  let t = q[i];
  let px = mix(gu.rect.x, gu.rect.z, t.x);
  let py = mix(gu.rect.y, gu.rect.w, t.y);
  var o : GVO;
  o.pos = vec4f(px * gu.frame.x - 1.0, 1.0 - py * gu.frame.y, 0.0, 1.0);
  return o;
}
@fragment fn fs(in : GVO) -> @location(0) vec4f {
  let d = vec2f(in.pos.x - gu.fan.x, gu.fan.y - in.pos.y);
  let rr = length(d);
  let ang = atan2(d.x, max(d.y, 1e-4));
  if (rr > gu.fan.z || abs(ang) > gu.fan.w || d.y < 0.0) { return vec4f(0.0, 0.0, 0.0, 1.0); }
  let u = ang / gu.fan.w * 0.5 + 0.5;
  let v = rr / gu.fan.z;
  let echo = textureSampleLevel(graster, gsamp, vec2f(u, v), 0.0).r;
  /* A monochrome phosphor: the ONE number a resolution cell carries is how much came back, so the
   * picture may only vary in brightness. pow(2.2) because the target view re-encodes to sRGB. */
  let disp = vec3f(0.16, 1.0, 0.30) * echo;
  return vec4f(pow(disp, vec3f(2.2)), 1.0);
}
)";

void FBGroundMapStage::Init(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)kGroundMapAz, (uint32_t)kGroundMapRange, 1};
  td.format = wgpu::TextureFormat::R8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Raster = Device.CreateTexture(&td);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  Samp = Device.CreateSampler(&sd);

  wgpu::BufferDescriptor bd{};
  bd.size = 48;   /* three vec4f — matches GU exactly, so minBindingSize cannot drift */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kGroundMapWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;   /* opaque: the bay REPLACES what the scene left there */
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = bd.size;
  be[1].binding = 1; be[1].sampler = Samp;
  be[2].binding = 2; be[2].textureView = Raster.CreateView();
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 3;
  bgd.entries = be;
  Bind = Device.CreateBindGroup(&bgd);
}

void FBGroundMapStage::SetTarget(const Systems::FBMfdBayRect &bay, const FBGroundMapBlock &map, bool have) {
  Bay = bay;
  Map = map;
  Have = have && map.H.Readable() && map.Mapping && bay.X1 - bay.X0 > 2.f && bay.Y1 - bay.Y0 > 2.f;
}

void FBGroundMapStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!Have || !Pipe) return;

  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = Raster;
  wgpu::TexelCopyBufferLayout lay{};
  lay.bytesPerRow = (uint32_t)kGroundMapAz;
  lay.rowsPerImage = (uint32_t)kGroundMapRange;
  wgpu::Extent3D ext{(uint32_t)kGroundMapAz, (uint32_t)kGroundMapRange, 1};
  Queue.WriteTexture(&dst, Map.Cell, sizeof Map.Cell, &lay, &ext);

  Systems::FBMfdBayRect body = Systems::FBMfdBodyOf(Bay);
  float apexX = 0.f, apexY = 0.f, radius = 0.f;
  Systems::FBFcrFan(body, Map.AzHalfDeg, apexX, apexY, radius);
  float u[12] = {Bay.X0, Bay.Y0, Bay.X1, Bay.Y1,
                 apexX, apexY, radius, Map.AzHalfDeg * 3.14159265f / 180.f,
                 2.0f / (float)ctx.Width, 2.0f / (float)ctx.Height, 0.f, 0.f};
  Queue.WriteBuffer(Uni, 0, u, sizeof u);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(6);
}

} // namespace FlightBox::Render
