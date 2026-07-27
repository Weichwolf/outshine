#include "FBCloudResolveStage.h"
#include "FBAtmoCommon.h"
#include <string>

namespace FlightBox {

/* Blends the fresh jittered march into the reprojected history with a neighbourhood clamp against
 * ghosting; prepends kAtmoCommon for the Atmo struct. */
static const char *kCloudResolveWGSL = R"(
struct RU { prevVP : mat4x4f, camMove : vec4f, blend : vec4f };   /* camMove.xyz metres; blend: alpha, histValid, midR(Mm), - */
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var freshTex : texture_2d<f32>;
@group(0) @binding(2) var histTex : texture_2d<f32>;
@group(0) @binding(3) var<uniform> A : Atmo;
@group(0) @binding(4) var<uniform> RB : RU;
@group(0) @binding(5) var wsumTex : texture_2d<f32>;   /* accumulated splat weight (accum mode) */
struct ROut { @location(0) col : vec4f, @location(1) wsum : vec4f };
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f, @location(1) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var cc = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut; let p = cc[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  o.ndc = p;
  return o;
}
@fragment fn fs(in : VOut) -> ROut {
  let lowDim = vec2f(f32(textureDimensions(freshTex).x), f32(textureDimensions(freshTex).y));
  let fLow = in.uv * lowDim;
  var out : ROut;
  out.wsum = vec4f(1.0, 0.0, 0.0, 0.0);   /* live path: unused */

  /* ACCUM SPLAT proof (blend.y 2/3): place each cell's JITTERED sample with a tent kernel at its sub-cell
   * position and carry a running WEIGHTED average (weight sum in wsumTex). Over the 16 Halton phases each
   * full-res pixel accumulates the samples that landed on it -> SHARP full-res reconstruction, static. */
  if (RB.blend.y > 1.5) {
    let cell = floor(fLow) + 0.5;
    let pPos = cell + vec2f(RB.camMove.w, -RB.blend.w);         /* the cell's jittered sample position (low-res px) */
    let w = max(0.0, 1.0 - length(fLow - pPos) / 0.2);   /* narrow tent (~0.8 full-res px) for sharp placement */
    let sample = textureSampleLevel(freshTex, samp, cell / lowDim, 0.0);   /* nearest (no bilinear pre-blur) */
    var pw = 0.0; var pc = vec4f(0.0);
    if (RB.blend.y > 2.5) {   /* exact same pixel (static accum); textureLoad avoids the r32 filterable-sampler rule */
      let px = vec2i(i32(in.pos.x), i32(in.pos.y));
      pw = textureLoad(wsumTex, px, 0).r; pc = textureLoad(histTex, px, 0);
    }
    let nw = pw + w;
    out.col = select(pc, (pc * pw + sample * w) / max(nw, 1.0e-6), nw > 0.0);
    out.wsum = vec4f(nw, 0.0, 0.0, 0.0);
    return out;
  }

  /* LIVE path: same SPLAT placement as accum (nearest cell sample at its jittered sub-position, accepted by
   * confidence) but with an exponential blend instead of 1/N -> reconstructs full-res over frames AND adapts
   * to motion. Confidence modulates the blend alpha: heavy fresh where the jittered sample lands on F. */
  let cellL = floor(fLow) + 0.5;
  let pPosL = cellL + vec2f(RB.camMove.w, -RB.blend.w);
  let confL = max(0.0, 1.0 - length(fLow - pPosL) / 0.5);
  let cellUVL = cellL / lowDim;
  let fresh = textureSampleLevel(freshTex, samp, cellUVL, 0.0);   /* nearest (no bilinear pre-blur) */
  out.col = fresh;
  if (RB.blend.y < 0.5) { return out; }            /* first frame: no history */
  let dir = normalize(A.camFwd.xyz + in.ndc.x * A.params.x * A.params.y * A.camRight.xyz
                                   + in.ndc.y * A.params.x * A.camUp.xyz);
  let cam = A.camPosMm.xyz;
  let midR = RB.blend.z;
  let b = dot(cam, dir);
  let disc = b * b - (dot(cam, cam) - midR * midR);
  if (disc <= 0.0) { return out; }
  let midDist = -b + sqrt(disc);
  if (midDist <= 0.0) { return out; }
  let prevRelM = dir * midDist * 1.0e6 + RB.camMove.xyz;
  let clip = RB.prevVP * vec4f(prevRelM, 1.0);
  if (clip.w <= 0.0) { return out; }
  let puv = (clip.xy / clip.w) * vec2f(0.5, -0.5) + vec2f(0.5, 0.5);
  if (puv.x < 0.0 || puv.x > 1.0 || puv.y < 0.0 || puv.y > 1.0) { return out; }
  var hist = textureSampleLevel(histTex, samp, puv, 0.0);
  var mn = fresh; var mx = fresh;
  let texel = 1.0 / lowDim;
  for (var j = -1; j <= 1; j++) { for (var i = -1; i <= 1; i++) {
    let s = textureSampleLevel(freshTex, samp, cellUVL + vec2f(f32(i), f32(j)) * texel, 0.0);
    mn = min(mn, s); mx = max(mx, s);
  }}
  hist = clamp(hist, mn, mx);
  let motion = length(RB.camMove.xyz);
  let graze = 1.0 - abs(dot(normalize(cam), dir));
  /* confidence modulates the fresh weight: accept this cell's jittered sample mostly where it lands on F. */
  let a = clamp((RB.blend.x + motion * 0.02 + graze * 0.05) * (0.25 + 0.75 * confL), 0.0, 0.5);
  out.col = mix(hist, fresh, a);
  return out;
}
)";

void FBCloudResolveStage::Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler samp,
                                     wgpu::TextureView cloudLowView) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  for (int k = 0; k < 2; k++) {
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)gpu.Width, (uint32_t)gpu.Height, 1};
    td.format = wgpu::TextureFormat::RGBA16Float;
    td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    CloudHist[k] = Device.CreateTexture(&td);
    td.format = wgpu::TextureFormat::R32Float;   /* accum-mode splat weight sum (MRT target 1) */
    CloudWSum[k] = Device.CreateTexture(&td);
  }

  wgpu::BufferDescriptor rbd{};
  rbd.size = (16 + 4 + 4) * sizeof(float);   /* mat4 + camMove vec4 + blend vec4 */
  rbd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  ResolveUni = Device.CreateBuffer(&rbd);

  std::string src = std::string(kAtmoCommon) + kCloudResolveWGSL;
  wgpu::ShaderSourceWGSL w{};
  w.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &w;
  wgpu::ShaderModule rm = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState rct[2]{};
  rct[0].format = wgpu::TextureFormat::RGBA16Float;   /* [0] = resolved cloud (normalized) */
  rct[1].format = wgpu::TextureFormat::R32Float;      /* [1] = accum splat weight sum */
  wgpu::RenderPipelineDescriptor rrp{};
  rrp.vertex.module = rm;
  wgpu::FragmentState rfs{};
  rfs.module = rm;
  rfs.targetCount = 2;
  rfs.targets = rct;
  rrp.fragment = &rfs;
  CloudResolvePipe = Device.CreateRenderPipeline(&rrp);

  for (int k = 0; k < 2; k++) {   /* [k] binds CloudHist[k] + CloudWSum[k] as the PREV history */
    wgpu::BindGroupEntry rbe[6] = {};
    rbe[0].binding = 0; rbe[0].sampler = samp;
    rbe[1].binding = 1; rbe[1].textureView = cloudLowView;
    rbe[2].binding = 2; rbe[2].textureView = CloudHist[k].CreateView();
    rbe[3].binding = 3; rbe[3].buffer = atmoBuf; rbe[3].size = 11 * 4 * sizeof(float);
    rbe[4].binding = 4; rbe[4].buffer = ResolveUni; rbe[4].size = (16 + 4 + 4) * sizeof(float);
    rbe[5].binding = 5; rbe[5].textureView = CloudWSum[k].CreateView();
    wgpu::BindGroupDescriptor rbg{};
    rbg.layout = CloudResolvePipe.GetBindGroupLayout(0);
    rbg.entryCount = 6;
    rbg.entries = rbe;
    CloudResolveBind[k] = Device.CreateBindGroup(&rbg);
  }
}

void FBCloudResolveStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass, double cloudMidR) {
  int hprev = ReadIndex();
  float rb[24] = {0};
  for (int i = 0; i < 16; i++) rb[i] = PrevVP[i];
  rb[16] = (float)(ctx.Eye[0] - PrevEye[0]);   /* camMove, metres */
  rb[17] = (float)(ctx.Eye[1] - PrevEye[1]);
  rb[18] = (float)(ctx.Eye[2] - PrevEye[2]);
  { uint32_t ph = ctx.FrameNo % 16u;           /* SAME 4x4-grid jitter the march used this frame (low-res px) */
    rb[19] = ((float)(ph % 4u) + 0.5f) / 4.0f - 0.5f; rb[23] = ((float)(ph / 4u) + 0.5f) / 4.0f - 0.5f; }
  if (AccumMode) { AccumN++; rb[20] = 1.0f; rb[21] = HistValid ? 3.0f : 2.0f; }   /* splat: 2=first accum frame, 3=has history */
  else { rb[20] = 0.12f; rb[21] = HistValid ? 1.0f : 0.0f; }
  rb[22] = (float)cloudMidR;
  Queue.WriteBuffer(ResolveUni, 0, rb, sizeof rb);
  pass.SetPipeline(CloudResolvePipe);
  pass.SetBindGroup(0, CloudResolveBind[hprev]);
  pass.Draw(3);
}

void FBCloudResolveStage::Advance(const FBFrameContext &ctx) {
  for (int i = 0; i < 16; i++) PrevVP[i] = ctx.Mvp20[i];
  for (int a = 0; a < 3; a++) PrevEye[a] = ctx.Eye[a];
  HistCur = ReadIndex();   /* the history we just wrote becomes next frame's "previous" */
  HistValid = true;
}

} // namespace FlightBox
