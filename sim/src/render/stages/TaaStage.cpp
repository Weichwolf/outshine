#include "TaaStage.h"
#include "AtmoCommon.h"
#include "SceneTargets.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace outshine::Render {

/* DERIVED, against a 16x supersampled ground truth (16 pinned sub-pixel phases of the same frame,
 * averaged in linear light) and against the ghost-free answer at the same pose. The curve:
 *
 *   gamma   RMSE vs truth   mid-band power   Nyquist-octave power   lag at 3.8 px/frame
 *   1.0        4.455            0.753 x            0.632 x               0.50 px
 *   1.5        3.959            0.924 x            0.956 x               0.63 px
 *   2.0        3.929            0.948 x            1.016 x               0.66 px
 *   none      11.432            2.242 x            4.305 x               —
 *
 * 1.5 and not Karis' 1.0: the clip is what a TAA pays its sharpness tax to, and at 1.0 this scene
 * loses a quarter of its genuine detail. 2.0 buys 0.5 % more of the truth and lets the Nyquist octave
 * back over 1.0, i.e. it starts re-admitting the aliasing the round exists to remove. */
static constexpr float kTaaGamma = 1.5f;
static constexpr float kTaaVelRamp = 0.015f;
static constexpr float kTaaFeedMax = 0.85f;

static const char *kTaaWGSL = R"(
struct T {
  prevMvp : mat4x4f,
  eyeDelta: vec4f,   // xyz = eyeCur - eyePrev, ECEF metres; w unused
  jitter  : vec4f,   // xy = this frame's NDC jitter, zw = the previous frame's
  par     : vec4f,   // x,y = target size in pixels, z = 1 when the history is usable, w = feedback
};
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var cur : texture_2d<f32>;
@group(0) @binding(2) var hist : texture_2d<f32>;
@group(0) @binding(3) var velTex : texture_2d<f32>;
@group(0) @binding(4) var depthTex : texture_depth_2d;
@group(0) @binding(5) var<uniform> A : Atmo;
@group(0) @binding(6) var<uniform> t : T;

/* MvpCamRel's near plane; depth = zn / (-z_eye) is the whole of the reversed-Z projection. */
const kZNear : f32 = 0.05;
/* Beyond this the reprojection is a rotation and the eye's own translation is below a thousandth of
 * a pixel, so clamping it costs nothing and keeps the sky's 1/0 finite. */
const kMaxRangeM : f32 = 1.0e7;

/* YCoCg, because the neighbourhood clip has to bound LUMINANCE separately from chroma: an RGB box
 * around a sunlit blade and the shadow beside it is a cube whose corners are colours neither pixel
 * has, and history clipped into such a corner is the classic TAA colour fringe. */
fn rgb2ycocg(c : vec3f) -> vec3f {
  return vec3f(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * (c.r - c.b),
               -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}
fn ycocg2rgb(c : vec3f) -> vec3f {
  let tmp = c.x - c.z;
  return vec3f(tmp + c.y, c.x + c.z, tmp - c.y);
}

/* Clip toward the neighbourhood's CENTRE instead of clamping to its box: clamping moves a history
 * sample to the nearest face, which for a chroma-far sample is a colour the frame does not contain.
 * The line segment from the mean to the sample keeps the hue and only shortens the excursion. */
fn clipToBox(bmin : vec3f, bmax : vec3f, p : vec3f, q : vec3f) -> vec3f {
  let ctr = 0.5 * (bmax + bmin);
  let ext = max(0.5 * (bmax - bmin), vec3f(1.0e-5));
  let d = q - ctr;
  let uv = abs(d / ext);
  let m = max(uv.x, max(uv.y, uv.z));
  if (m > 1.0) { return ctr + d / m; }
  return q;
}

/* Catmull-Rom over the history, in the five-bilinear-tap form: a bilinear history fetch loses a
 * measurable amount of the sharpness the jitter just bought, and this is the standard remedy —
 * B-spline weights collapsed onto offset bilinear taps, no extra bandwidth beyond the taps. */
fn sampleHistory(uv : vec2f, dims : vec2f) -> vec4f {
  let pos = uv * dims;
  let c = floor(pos - 0.5) + 0.5;
  let f = pos - c;
  let f2 = f * f;
  let f3 = f2 * f;
  let w0 = f2 - 0.5 * (f3 + f);
  let w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
  let w3 = 0.5 * (f3 - f2);
  let w2 = 1.0 - w0 - w1 - w3;
  let w12 = w1 + w2;
  let t12 = w2 / max(w12, vec2f(1.0e-6));
  let uv0 = (c - 1.0) / dims;
  let uv12 = (c + t12) / dims;
  let uv3 = (c + 2.0) / dims;
  var o = vec4f(0.0);
  o = o + textureSampleLevel(hist, samp, vec2f(uv12.x, uv0.y), 0.0) * (w12.x * w0.y);
  o = o + textureSampleLevel(hist, samp, vec2f(uv0.x, uv12.y), 0.0) * (w0.x * w12.y);
  o = o + textureSampleLevel(hist, samp, uv12, 0.0) * (w12.x * w12.y);
  o = o + textureSampleLevel(hist, samp, vec2f(uv3.x, uv12.y), 0.0) * (w3.x * w12.y);
  o = o + textureSampleLevel(hist, samp, vec2f(uv12.x, uv3.y), 0.0) * (w12.x * w3.y);
  let wsum = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
  return o / max(wsum, 1.0e-6);
}

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
  let dims = vec2f(t.par.xy);
  let px = vec2i(in.pos.xy);
  let lim = vec2i(dims) - vec2i(1, 1);
  let uv = (vec2f(px) + vec2f(0.5)) / dims;
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

  let centre = textureLoad(cur, px, 0);
  if (t.par.z < 0.5) { return centre; }

  /* The 3x3 neighbourhood as first and second moment, in YCoCg. The box is the intersection of the
   * min/max hull with mean +- kGamma*sigma: the hull alone is too permissive in a field where one
   * neighbour is a sunlit margin, the moments alone too tight at a silhouette. */
  var m1 = vec3f(0.0);
  var m2 = vec3f(0.0);
  var lo = vec3f(1.0e9);
  var hi = vec3f(-1.0e9);
  for (var dy = -1; dy <= 1; dy = dy + 1) {
    for (var dx = -1; dx <= 1; dx = dx + 1) {
      let s = textureLoad(cur, clamp(px + vec2i(dx, dy), vec2i(0, 0), lim), 0);
      let y = rgb2ycocg(s.rgb);
      m1 = m1 + y;
      m2 = m2 + y * y;
      lo = min(lo, y);
      hi = max(hi, y);
    }
  }
  let mu = m1 * (1.0 / 9.0);
  let sigma = sqrt(max(m2 * (1.0 / 9.0) - mu * mu, vec3f(0.0)));
  let bmin = max(lo, mu - kGamma * sigma);
  let bmax = min(hi, mu + kGamma * sigma);

  /* THE MOTION. Anything world-fixed left the sentinel standing and is reprojected from its own
   * depth; anything that moved wrote the difference of its two clip positions itself. Both are
   * jittered, so the SAME correction takes both back to the true motion. */
  var v = textureLoad(velTex, px, 0).xy;
  if (v.x < -1.0e3) {
    let d = textureLoad(depthTex, px, 0);
    let ray = camRay(A, ndc);
    let range = min((kZNear / max(d, 1.0e-9)) / max(dot(ray, A.camFwd.xyz), 1.0e-4), kMaxRangeM);
    let cp = t.prevMvp * vec4f(ray * range + t.eyeDelta.xyz, 1.0);
    v = ndc - cp.xy / max(cp.w, 1.0e-9);
  }
  v = v - (t.jitter.xy - t.jitter.zw);
  let prevUv = uv + vec2f(-v.x, v.y) * 0.5;

  if (prevUv.x < 0.0 || prevUv.y < 0.0 || prevUv.x > 1.0 || prevUv.y > 1.0) { return centre; }

  let raw = sampleHistory(prevUv, dims);
  let clipped = ycocg2rgb(clipToBox(bmin, bmax, rgb2ycocg(centre.rgb), rgb2ycocg(raw.rgb)));

  /* Karis' luminance weighting. Without it one firefly in the history keeps its whole energy through
   * every blend and walks across the picture; weighting each side by 1/(1+Y) makes the mean the one
   * a tone curve would see rather than the one a linear average produces. */
  /* THE FEEDBACK IS NOT A CONSTANT, and the reason is measurable: a history that is resampled a
   * long way every frame passes through 1/a Catmull-Rom filters before it is forgotten, and at
   * 21.8 px per frame that chain smears the sward into a felt. So the new frame's weight rises with
   * the SPEED of the pixel — fast motion carries its own masking and needs the accumulation less.
   * The ramp's two numbers are measured in the table on kVelRamp. */
  let vpx = length(v * 0.5 * dims);
  let a = clamp(t.par.w + vpx * kVelRamp, t.par.w, kFeedMax);
  let wc = a / (1.0 + max(rgb2ycocg(centre.rgb).x, 0.0));
  let wh = (1.0 - a) / (1.0 + max(rgb2ycocg(clipped).x, 0.0));
  let rgb = (centre.rgb * wc + clipped * wh) / max(wc + wh, 1.0e-6);
  /* Alpha is the DIRECT fraction the tonemap weights its occlusion by (stages/SurfaceLight.h) — a
   * per-pixel share, not a radiance, so it accumulates but is not clipped against a colour box. */
  return vec4f(rgb, centre.a * a + raw.a * (1.0 - a));
}
)";

void TaaStage::Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView,
                         wgpu::TextureView velView, wgpu::TextureView depthView,
                         wgpu::Buffer atmoBuf, int width, int height) {
  Queue = gpu.Queue;
  Width = width;
  Height = height;

  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)width, (uint32_t)height, 1};
  td.format = gpu.HdrFormat;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  Hist[0] = gpu.Device.CreateTexture(&td);
  Hist[1] = gpu.Device.CreateTexture(&td);
  Bytes = 2L * (long)width * (long)height * 8L;   /* rgba16float */

  const std::string src =
      std::string(kAtmoCommon)
      /* How many standard deviations of the 3x3 neighbourhood the history may sit outside before it
       * is pulled in. */
      + "const kGamma : f32 = " + std::to_string(kTaaGamma) + ";\n"
      /* DERIVED, per pixel of motion per frame, against the settled TAA at the same pose (the
       * ghost-free answer) at two pan speeds. mid = genuine detail, top = the Nyquist octave whose
       * excess IS the aliasing this round exists to remove:
       *
       *   ramp     3.8 px/frame            21.8 px/frame
       *            RMSE   mid    top       RMSE   mid    top
       *   0.000   10.02  0.245  0.305     10.56  0.288  0.408
       *   0.015    9.42  0.299  0.373      9.23  0.719  1.118
       *   0.030    8.90  0.370  0.476     10.80  1.444  2.504
       *   0.060    8.15  0.557  0.784     11.15  1.563  2.734
       *   none      —      —      —       12.87  2.487  4.628
       *
       * 0.015 is the LARGEST ramp that still keeps the promise at speed: it minimises the distance
       * to the truth on the fast pan and holds the Nyquist octave at 1.1x, where 0.030 lets it back
       * to 2.5x — two thirds of the way to having no antialiasing at all while panning. */
      + "const kVelRamp : f32 = " + std::to_string(kTaaVelRamp) + ";\n"
      /* [SET] 0.85. A pixel moving fast enough to reach the cap has nothing usable in the history
       * anyway; leaving 0.15 keeps the edges of slow objects inside a fast pan from re-aliasing. */
      + "const kFeedMax : f32 = " + std::to_string(kTaaFeedMax) + ";\n"
      + kTaaWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  Pipe = gpu.Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = gpu.Device.CreateBuffer(&bd);

  for (int i = 0; i < 2; i++) {
    wgpu::BindGroupEntry be[7] = {};
    be[0].binding = 0; be[0].sampler = samp;
    be[1].binding = 1; be[1].textureView = hdrView;
    be[2].binding = 2; be[2].textureView = Hist[1 - i].CreateView();
    be[3].binding = 3; be[3].textureView = velView;
    be[4].binding = 4; be[4].textureView = depthView;
    be[5].binding = 5; be[5].buffer = atmoBuf; be[5].size = kAtmoUniformBytes;
    be[6].binding = 6; be[6].buffer = Uni; be[6].size = kUniFloats * sizeof(float);
    wgpu::BindGroupDescriptor bg{};
    bg.layout = Pipe.GetBindGroupLayout(0);
    bg.entryCount = 7;
    bg.entries = be;
    Bind[i] = gpu.Device.CreateBindGroup(&bg);
  }
}

void TaaStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!Pipe) return;
  float u[kUniFloats] = {};
  std::memcpy(u, ctx.PrevMvp16, 16 * sizeof(float));
  u[16] = ctx.EyeDeltaM[0]; u[17] = ctx.EyeDeltaM[1]; u[18] = ctx.EyeDeltaM[2]; u[19] = 0.0f;
  u[20] = ctx.JitterNdc[0]; u[21] = ctx.JitterNdc[1];
  u[22] = ctx.PrevJitterNdc[0]; u[23] = ctx.PrevJitterNdc[1];
  u[24] = (float)Width;
  u[25] = (float)Height;
  u[26] = ctx.HistoryValid ? 1.0f : 0.0f;
  /* [SET] 0.1 of the new frame per resolve. Its e-fold is 1/ln(1/0.9) = 9.5 frames, which is the
   * window the 8 jitter phases have to close inside; the neighbourhood clip is what keeps that long
   * memory from becoming a trail. */
  u[27] = 0.1f;
  Queue.WriteBuffer(Uni, 0, u, sizeof u);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind[ctx.FrameNo & 1u]);
  pass.Draw(3);
}

} // namespace outshine::Render
