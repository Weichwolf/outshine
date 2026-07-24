#include "FBTonemapStage.h"

namespace FlightBox {

static const char *kTonemapWGSL = R"(
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var hdr : texture_2d<f32>;
@group(0) @binding(2) var cloud : texture_2d<f32>;   // quarter-res premultiplied cloud (upsampled)
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
  var scene = textureSample(hdr, samp, in.uv).rgb;
  let cl = textureSample(cloud, samp, in.uv);          // bilinear upsample of the quarter-res cloud
  scene = scene * (1.0 - cl.a) + cl.rgb;               // premultiplied cloud over the HDR scene
  return vec4f(aces(scene), 1.0);   // linear [0,1]; the sRGB swapchain view encodes on write
}
)";

/* Cloud-off tonemap: the SAME ACES compress with no cloud composite (no cloud texture binding at all,
 * so the cloud path can be skipped whole — no stale-history sample). Used unless FB_CLOUDS=1. */
static const char *kTonemapPlainWGSL = R"(
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var hdr : texture_2d<f32>;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  return o;
}
fn aces(x : vec3f) -> vec3f {
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3f(0.0), vec3f(1.0));
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let scene = textureSample(hdr, samp, in.uv).rgb;
  return vec4f(aces(scene), 1.0);
}
)";

void FBTonemapStage::Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView,
                                bool cloudsOn, const FBCloudResolveStage *cloudResolve) {
  CloudsOn = cloudsOn;
  CloudResolve = cloudResolve;

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

  /* Plain (cloud-off) tonemap always built — it is the default present path. */
  wgpu::ShaderSourceWGSL pwgsl{}; pwgsl.code = kTonemapPlainWGSL;
  wgpu::ShaderModuleDescriptor psmd{}; psmd.nextInChain = &pwgsl;
  wgpu::ShaderModule psm = gpu.Device.CreateShaderModule(&psmd);
  wgpu::RenderPipelineDescriptor prp{};
  prp.vertex.module = psm;
  wgpu::FragmentState pfs{}; pfs.module = psm; pfs.targetCount = 1; pfs.targets = &ct;
  prp.fragment = &pfs;
  TonemapPlainPipe = gpu.Device.CreateRenderPipeline(&prp);
  {
    wgpu::BindGroupEntry be[2] = {};
    be[0].binding = 0; be[0].sampler = samp;
    be[1].binding = 1; be[1].textureView = hdrView;
    wgpu::BindGroupDescriptor bgd{};
    bgd.layout = TonemapPlainPipe.GetBindGroupLayout(0);
    bgd.entryCount = 2;
    bgd.entries = be;
    TonemapBindPlain = gpu.Device.CreateBindGroup(&bgd);
  }

  /* Cloud-compositing tonemap only when the cloud path is armed (CloudHist exists). */
  if (CloudsOn) {
    TonemapPipe = gpu.Device.CreateRenderPipeline(&rp);
    for (int k = 0; k < 2; k++) {   /* one per history: composites the TEMPORALLY-accumulated cloud */
      wgpu::BindGroupEntry be[3] = {};
      be[0].binding = 0; be[0].sampler = samp;
      be[1].binding = 1; be[1].textureView = hdrView;
      be[2].binding = 2; be[2].textureView = CloudResolve->GetHistView(k);
      wgpu::BindGroupDescriptor bgd{};
      bgd.layout = TonemapPipe.GetBindGroupLayout(0);
      bgd.entryCount = 3;
      bgd.entries = be;
      TonemapBindH[k] = gpu.Device.CreateBindGroup(&bgd);
    }
  }
}

void FBTonemapStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  if (CloudsOn) {
    pass.SetPipeline(TonemapPipe);
    pass.SetBindGroup(0, TonemapBindH[CloudResolve->WriteIndex()]);   /* composite the temporally-accumulated cloud */
  } else {
    pass.SetPipeline(TonemapPlainPipe);   /* no cloud composite (default) */
    pass.SetBindGroup(0, TonemapBindPlain);
  }
  pass.Draw(3);
}

} // namespace FlightBox
