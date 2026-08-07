#include "MoonStage.h"
#include "SceneTargets.h"
#include "AtmoCommon.h"
#include <string>

namespace outshine::Render {

/* Reconstruct the front-hemisphere normal, sample the albedo, light it with the REAL sun direction —
 * so phase and terminator emerge physically instead of being drawn. */
static const char *kMoonWGSL = R"(
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var moonTex : texture_2d<f32>;
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
  let moonUp = A.moonDir.xyz;
  if (evs <= 0.5 || dot(moonUp, A.up.xyz) <= -0.03) { return vec4f(0.0); }   /* SVS, or moon below horizon */
  let dir = camRay(A, in.ndc);
  let mr = A.skyExtra.w;               /* moon angular radius: 0.0045 rad (real) x FB_MOON_SCALE */
  let cosA = dot(dir, moonUp);
  if (cosA <= cos(mr * 3.0)) { return vec4f(0.0); }
  let mRight = normalize(cross(A.up.xyz, moonUp));
  let mUp = normalize(cross(moonUp, mRight));
  let toObs = -moonUp;                 /* moon centre -> observer */
  let u = dot(dir, mRight) / mr;
  let v = dot(dir, mUp) / mr;
  let r2 = u * u + v * v;
  if (r2 >= 1.0) { return vec4f(0.0); }
  let nrm = mRight * u + mUp * v + toObs * -sqrt(1.0 - r2);   /* sphere normal, front hemi */
  let lon = atan2(dot(nrm, mRight), dot(nrm, -toObs));
  let lat = asin(clamp(dot(nrm, mUp), -1.0, 1.0));
  let muv = vec2f(0.5 + lon / (2.0 * 3.14159265), 0.5 - lat / 3.14159265);
  let alb = textureSampleLevel(moonTex, lsamp, muv, 0.0).rgb;
  let lit = max(dot(nrm, A.sunDir.xyz), 0.0);
  let day = A.skyExtra.x;
  let earth = 0.06 * A.moonDir.w;      /* faint earthshine on the dark limb */
  let moonCol = alb * (lit + earth) * 12.0 * (1.0 - 0.6 * day);
  return vec4f(moonCol, 1.0);
}
)";

void MoonStage::Configure(const Gpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                             const uint8_t *rgba, size_t rgbaBytes, int w, int h) {
  int mw = w > 0 ? w : 1, mh = h > 0 ? h : 1;
  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)mw, (uint32_t)mh, 1};
  td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Tex = gpu.Device.CreateTexture(&td);
  static const uint8_t grey[4] = {150, 150, 150, 255};
  const uint8_t *src = rgba && rgbaBytes >= (size_t)mw * mh * 4 ? rgba : grey;
  wgpu::TexelCopyTextureInfo dst{}; dst.texture = Tex;
  wgpu::TexelCopyBufferLayout lay{}; lay.bytesPerRow = (uint32_t)mw * 4; lay.rowsPerImage = (uint32_t)mh;
  wgpu::Extent3D ext{(uint32_t)mw, (uint32_t)mh, 1};
  gpu.Queue.WriteTexture(&dst, src, (size_t)mw * mh * 4, &lay, &ext);

  std::string srcWgsl = std::string(kAtmoCommon) + kMoonWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = srcWgsl.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = false;
  ds.depthCompare = wgpu::CompareFunction::Always;

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
  be[0].binding = 0; be[0].buffer = atmoBuf; be[0].size = kAtmoUniformBytes;
  be[1].binding = 1; be[1].sampler = lutSamp;
  be[2].binding = 2; be[2].textureView = Tex.CreateView();
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 3;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void MoonStage::Encode(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace outshine::Render
