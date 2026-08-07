#include "SpritesStage.h"
#include "SceneTargets.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include "AtmoCommon.h"
#include "AtmoHaze.h"
#include "AtmoSample.h"
#include "SceneScale.h"
#include "Log.h"

namespace outshine::Render {

/* One quad per effect, built in SCREEN space around the projection of its two end points, so a plume
 * segment is a stretched billboard and a flare is the same quad with equal sides. Three shapes share
 * the pipeline because they share the geometry; only the fragment profile differs.
 *
 * THE SUB-PIXEL FLOOR IS A GAIN, NOT A SIZE. Below half a pixel the quad is grown to stay rasterisable
 * and its radiance is divided by exactly the area that growth added, per axis — so a flare at 20 km
 * contributes the energy a flare at 20 km contributes and fades out of the picture on its own. Growing
 * it without the division is what would turn an effect into a target marker. */
static const char *kSpritesWGSL = R"(
struct U { mvp : mat4x4f, vp : vec4f, haze : vec4f, dk0 : vec4f, dk1 : vec4f, dk2 : vec4f };
@group(0) @binding(0) var<uniform> u : U;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var svLUT : texture_2d<f32>;
@group(0) @binding(3) var<uniform> A : Atmo;

const kSprNear : f32 = 0.05;      /* MvpCamRel's near plane: clip.w IS the forward distance in metres */
/* HALF-EXTENT FLOOR, and it is DERIVED, not chosen: a fragment exists only where the quad covers a
 * pixel CENTRE, and the farthest any point can lie from the nearest centre is sqrt(2)/2 = 0.7071 px.
 * A quad whose inscribed radius reaches that always lights at least one sample, so below it the sprite
 * stops flickering in and out with sub-pixel position (measured before this: an afterburner plume at
 * 650 m lit 0 pixels, at 850 m one pixel at 109/255 — the same plume, the same range decade). */
const kSprMinPx : f32 = 0.7072;

/* COLOUR IS RADIANCE, and the numbers are chosen against the ONE curve that decides what a colour
 * looks like on screen: the ACES fit in TonemapStage. It reaches 231/255 at radiance 1.0 and 250 at
 * 3.0 — so a channel above ~1 is white no matter what the other two do, and a flame painted with three
 * large channels is a WHITE BLOB (measured: peak 9.0 in all three gave 255,255,255 over 75 % of the
 * plume). Saturation therefore has to live in the RATIO: red high, green low, blue near zero.
 *   hot  (6.0, 2.2, 0.9) -> sRGB 255,246,231   the choked core, white with a yellow cast
 *   cool (0.9, 0.06, 0.04) -> sRGB 229,73,60   the burnt-out tip, deep red
 * interpolated GEOMETRICALLY, which is what keeps the ratio moving instead of the sum. */
const kFlameHot : vec3f = vec3f(6.0, 2.2, 0.9);
const kFlameCool : vec3f = vec3f(0.9, 0.06, 0.04);
const kFlameCore : vec3f = vec3f(9.0, 7.0, 4.5);   /* the choked throat itself: white, thin, short */
const kFlareCore : vec3f = vec3f(40.0, 34.0, 24.0);
const kFlareHalo : vec3f = vec3f(10.0, 2.4, 0.40);
/* THE DETONATION'S OWN ARC, on the same ratio rule as the flame above: the flash is the only stage
 * allowed three large channels, everything after it lives in red over green over blue.
 *   flash (30, 26, 20) -> sRGB 255,255,254   the first tenth of a second, and only in the core
 *   body  (5.0, 1.1, 0.15) -> sRGB 255,222,153  the burning ball
 *   soot  (0.05, 0.035, 0.03)                the same ball once it has stopped emitting */
const kBlastFlash : vec3f = vec3f(30.0, 26.0, 20.0);
const kBlastBody : vec3f = vec3f(5.0, 1.1, 0.15);
const kBlastSoot : vec3f = vec3f(0.05, 0.035, 0.03);
/* A LAMP carries NO spectrum: the amplitude is here, the hue is the sprite's Color, so one kind serves
 * a red port light, a green starboard one and a white strobe without three sets of constants. Split
 * core/halo like the flare, because a halo is what makes a point read as a light rather than as a dot. */
const kLightCore : f32 = 60.0;
const kLightHalo : f32 = 7.0;

/* THE UNRESOLVED LIMIT. Once a sprite is smaller than a pixel it may not be SAMPLED any more: the one
 * covered sample lands wherever it lands, and for a tapering plume that is usually where the profile
 * is zero — measured as a plume that lit 0 pixels at 672 m and one pixel at 95/255 at 896 m, blinking
 * rather than fading. Below full resolution the profile is therefore replaced by its own INTEGRAL over
 * the quad, which is what a point source contributes. The four means are numerically integrated from
 * the very expressions below (512x512 samples) and are the reason the fade is smooth.
 * Herleitung: doc/render/units-visual.md §10. */
const kFlameMeanT : f32 = 0.3077;      /* intensity-weighted mean of t: the colour of an unresolved plume */
const kFlameBodyMean : f32 = 0.2375;
const kFlameCoreMean : f32 = 0.0751;
const kFlareCoreMean : f32 = 0.01454;
const kFlareHaloMean : f32 = 0.09973;
const kSmokeMeanLong : f32 = 0.60;     /* a trail segment (taper -> 0) */
const kSmokeMeanRound : f32 = 0.24;    /* a puff (taper = 1) */
/* The same integral for the three procedural profiles below, taken over the very expressions in `fs`
 * with the noise held at its OWN mean (0.5) — a mean over turbulence, not over one realisation. */
const kBlastEdgeMean : f32 = 0.50894;
const kBlastCoreMean : f32 = 0.04040;
const kFireMean : f32 = 0.23124;
const kFireHotMean : f32 = 0.11562;
const kLightCoreMean : f32 = 0.02094;
const kLightHaloMean : f32 = 0.07854;

/* Value noise, hash-based: at 60 GB/s of bandwidth against 2.5-4 TFLOPS, a fireball generated in ALU
 * costs nothing that a fetched flipbook would not cost more of (doc/render/visual-target.md §1).
 * Hash: Dave Hoskins, "Hash without Sine" (shadertoy XdGfRR / jcgt.org, 2014). */
fn fbHash21(p : vec2f) -> f32 {
  var q = fract(p * vec2f(0.1031, 0.1030));
  q = q + dot(q, q.yx + 33.33);
  return fract((q.x + q.y) * q.x);
}
fn fbValue2(p : vec2f) -> f32 {
  let cell = floor(p);
  let f = fract(p);
  let w = f * f * (3.0 - 2.0 * f);
  return mix(mix(fbHash21(cell), fbHash21(cell + vec2f(1.0, 0.0)), w.x),
             mix(fbHash21(cell + vec2f(0.0, 1.0)), fbHash21(cell + vec2f(1.0, 1.0)), w.x), w.y);
}
fn fbFbm2(p : vec2f) -> f32 {
  return 0.55 * fbValue2(p) + 0.30 * fbValue2(p * 2.03) + 0.15 * fbValue2(p * 4.01);
}

struct SOut {
  @builtin(position) pos : vec4f,
  @location(0) uv : vec2f,
  @location(1) col : vec3f,
  @location(2) @interpolate(flat) kindAlpha : vec4f,   /* kind, alpha, param, endTaper */
  @location(3) wpos : vec3f,
  @location(4) @interpolate(flat) res : f32,
  @location(5) @interpolate(flat) fx : vec2f,   /* phase, seed */
};

@vertex fn vs(@builtin(vertex_index) vi : u32,
              @location(0) a0 : vec4f, @location(1) a1 : vec4f,
              @location(2) a2 : vec4f, @location(3) a3 : vec4f) -> SOut {
  var quad = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(-1.0, 1.0),
                             vec2f(-1.0, 1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0));
  let corner = quad[vi];
  let rel = a0.xyz;
  let radM = a0.w;
  let halfLenM = a1.w;

  var c0 = u.mvp * vec4f(rel - a1.xyz * halfLenM, 1.0);
  var c1 = u.mvp * vec4f(rel + a1.xyz * halfLenM, 1.0);
  var o : SOut;
  /* Wholly behind the near plane: emit a degenerate quad outside clip space rather than branch on the
   * draw side — the instance still costs its vertex shader and nothing else. */
  if (c0.w < kSprNear && c1.w < kSprNear) {
    o.pos = vec4f(4.0, 4.0, 0.5, 1.0);
    o.uv = vec2f(0.0); o.col = vec3f(0.0); o.kindAlpha = vec4f(0.0); o.wpos = vec3f(0.0, 0.0, 1.0);
    o.res = 0.0; o.fx = vec2f(0.0);
    return o;
  }
  /* w is linear along the segment, so the crossing parameter is exact. */
  if (c0.w < kSprNear) { c0 = mix(c0, c1, (kSprNear - c0.w) / (c1.w - c0.w)); }
  if (c1.w < kSprNear) { c1 = mix(c1, c0, (kSprNear - c1.w) / (c0.w - c1.w)); }

  let halfVp = 0.5 * vec2f(u.vp.x, u.vp.y);
  let p0 = c0.xy / c0.w * halfVp;                  /* pixels, y up */
  let p1 = c1.xy / c1.w * halfVp;
  let mid = 0.5 * (p0 + p1);
  let seg = p1 - p0;
  let segLen = length(seg);
  let axisPx = select(vec2f(1.0, 0.0), seg / max(segLen, 1e-6), segLen > 1e-6);
  let perpPx = vec2f(-axisPx.y, axisPx.x);

  let distC = max(0.5 * (c0.w + c1.w), kSprNear);
  let pxPerRad = 0.5 * u.vp.y * u.vp.z;            /* focal in pixels: 0.5*H/tan(fov/2) */
  var halfW = radM * pxPerRad / distC;
  var halfL = max(0.5 * segLen, halfW);            /* a round sprite is as long as it is wide */

  /* ENERGY, not size: what the floor adds in area is divided straight back out. */
  let gw = max(1.0, kSprMinPx / max(halfW, 1e-6));
  let gl = max(1.0, kSprMinPx / max(halfL, 1e-6));
  halfW = halfW * gw;
  halfL = halfL * gl;
  let gain = 1.0 / (gw * gl);

  let px = mid + axisPx * (corner.x * halfL) + perpPx * (corner.y * halfW);
  let ndc = px / halfVp;
  /* Reversed-Z, [0,1]: z = near / forwardDistance, taken along the segment so a long trail is depth-
   * tested at the depth it actually has at that end. */
  let wEnd = mix(c0.w, c1.w, 0.5 * (corner.x + 1.0));
  o.pos = vec4f(ndc, kSprNear / max(wEnd, kSprNear), 1.0);
  o.uv = corner;
  o.col = a2.xyz * gain;
  o.res = gain;   /* 1 = resolved; below that the fragment fades into its own integral */
  o.kindAlpha = vec4f(a3.x, a2.w * gain, a3.y, clamp(radM / max(halfLenM, 1e-4), 0.01, 1.0));
  o.fx = vec2f(a3.z, a3.w);
  o.wpos = rel;
  return o;
}

@fragment fn fs(in : SOut) -> @location(0) vec4f {
  let kind = in.kindAlpha.x;
  let param = in.kindAlpha.z;
  var rgb = in.col;
  var a = 0.0;

  if (kind < 0.5) {
    /* FLAME: a teardrop along the axis, hot and narrow at the nozzle, opening and cooling downstream,
     * with the shock train a choked nozzle shows. uv.x = -1 is the nozzle. */
    let t = clamp(0.5 * (in.uv.x + 1.0), 0.0, 1.0);
    /* The plume holds the nozzle's own width for the first third and then closes — a jet efflux is a
     * TONGUE, not a cone, and drawing it as a cone is what made the first frame read as a puff. */
    let env = pow(max(1.0 - t, 0.0), 0.35);
    let q = in.uv.y / max(env, 1e-3);
    let body = max(1.0 - q * q, 0.0);
    let radial = body * body;                             /* a defined edge, not a Gaussian smear */
    let axial = pow(1.0 - t, 0.9);
    let shock = 1.0 + param * 0.45 * cos(t * 18.85);      /* three cells over the visible plume */
    let ramp = kFlameHot * pow(kFlameCool / kFlameHot, vec3f(t));
    let core = pow(max(1.0 - 4.0 * q * q, 0.0), 2.0) * pow(1.0 - t, 2.2);
    let meanRamp = kFlameHot * pow(kFlameCool / kFlameHot, vec3f(kFlameMeanT));
    let sharp = ramp * (radial * axial * shock) + kFlameCore * core;
    let flat = meanRamp * kFlameBodyMean + kFlameCore * kFlameCoreMean;
    rgb = rgb * mix(flat, sharp, in.res);
  } else if (kind < 1.5) {
    /* FLARE: a point source. A white-hot core inside a saturated halo — the halo is what makes it read
     * as a light rather than as a dot, and it carries a fraction of the energy. */
    let r = length(in.uv);
    let core = pow(max(1.0 - 3.0 * r, 0.0), 2.0);   /* the burning grain: the inner third of the quad */
    let halo = pow(max(1.0 - r, 0.0), 2.5);         /* the glow around it, which is what makes it READ */
    let sharp = kFlareCore * core + kFlareHalo * halo;
    let flat = kFlareCore * kFlareCoreMean + kFlareHalo * kFlareHaloMean;
    rgb = rgb * mix(flat, sharp, in.res);
  } else if (kind < 2.5) {
    /* SMOKE: a capsule of coverage. The ends taper over roughly one radius so consecutive segments of
     * one trail join instead of beading. */
    let across = abs(in.uv.y);
    let along = abs(in.uv.x);
    let side = max(1.0 - across * across, 0.0);
    let ends = clamp((1.0 - along) / in.kindAlpha.w, 0.0, 1.0);
    let covMean = mix(kSmokeMeanLong, kSmokeMeanRound, in.kindAlpha.w);
    let cov = mix(covMean, pow(side * ends, 1.0 + param), in.res);
    rgb = rgb * cov;
    a = in.kindAlpha.y * cov;
  } else if (kind < 3.5) {
    /* FIREBALL. Three things happen at once and the phase is the only clock: the flash collapses, the
     * ball opens into a billow whose RIM is where the noise lives (a detonation reads by its outline,
     * not by its interior), and the same billow turns into soot that OCCLUDES instead of emitting.
     * That last transition is why this kind cannot be an additive one: a burnt-out ball you can see
     * the terrain through is the thing that made the first attempt read as a puff of steam. */
    let bp = in.fx.x;
    let bR = length(in.uv);
    let bN = fbFbm2(in.uv * 2.2 + vec2f(in.fx.y, -in.fx.y));
    let bEdge = 1.0 - smoothstep(0.35 + 0.5 * bN, 1.0, bR);
    let bCore = pow(max(1.0 - 1.8 * bR, 0.0), 2.0);
    let flash = exp(-14.0 * bp);              /* the first ~0.2 of the life, and only in the core */
    let soot = smoothstep(0.30, 1.0, bp);
    let emit = kBlastFlash * (bCore * flash) + kBlastBody * (bEdge * (1.0 - soot));
    let flatEmit = kBlastFlash * (kBlastCoreMean * flash) + kBlastBody * (kBlastEdgeMean * (1.0 - soot));
    let cov = mix(kBlastEdgeMean, bEdge, in.res);
    a = in.kindAlpha.y * soot * cov;
    rgb = in.col * mix(flatEmit, emit, in.res) + kBlastSoot * a;
  } else if (kind < 4.5) {
    /* FIRE: what is still burning where something was destroyed. A tongue, not a ball — width falls to
     * nothing at the tip and the noise SCROLLS along the axis (phase is a free-running clock, not a
     * life), which is what separates a fire from a lit sphere at any frame rate. */
    let fT = clamp(0.5 * (in.uv.x + 1.0), 0.0, 1.0);
    let fN = fbFbm2(vec2f(in.uv.y * 1.6, fT * 3.2 - in.fx.x) + in.fx.y);
    let fW = (1.0 - fT) * (0.55 + 0.75 * fN);
    let fTongue = clamp(1.0 - abs(in.uv.y) / max(fW, 1e-3), 0.0, 1.0);
    let fRamp = kFlameHot * pow(kFlameCool / kFlameHot, vec3f(fT));
    let fSharp = fRamp * fTongue * (0.35 + 0.65 * (1.0 - fT) * (1.0 - fT));
    let fMeanRamp = kFlameHot * pow(kFlameCool / kFlameHot, vec3f(0.5));
    let fFlat = fMeanRamp * (0.35 * kFireMean + 0.65 * kFireHotMean);
    rgb = in.col * mix(fFlat, fSharp, in.res);
  } else {
    /* LIGHT: a lamp. Same core-inside-halo construction as the flare, with the spectrum taken out of
     * the constants — a port light is red because its Color says so, and the sub-pixel energy floor
     * above is what makes one four kilometres away a faint point instead of a marker. */
    let gR = length(in.uv);
    let gCore = pow(max(1.0 - 2.5 * gR, 0.0), 2.0);
    let gHalo = pow(max(1.0 - gR, 0.0), 3.0);
    let gSharp = kLightCore * gCore + kLightHalo * gHalo;
    let gFlat = kLightCore * kLightCoreMean + kLightHalo * kLightHaloMean;
    rgb = in.col * mix(gFlat, gSharp, in.res);
  }

  /* The same air everything else is behind: transmittance always, inscatter only where the sprite
   * actually covers something (an emitter adds light, it does not hide the sky behind it). */
  let distM = length(in.wpos);
  let fragAltM = (length(A.camPosMm.xyz + in.wpos * 1.0e-6) - u.haze.z) * 1.0e6;
  let hz = hazeTransmittance3(u.haze.x, 0.5 * (u.haze.y + fragAltM), distM);
  let vdir = normalize(in.wpos);
  rgb = rgb * hz;
  if (a > 0.0) { rgb = rgb + hazeInscatter(svLUT, lsamp, A, vdir) * (vec3f(1.0) - hz) * a; }
  return vec4f(rgb, a);
}
)";

void SpritesStage::Configure(const Gpu &gpu, wgpu::Sampler lutSamp, wgpu::TextureView skyLutView,
                               wgpu::Buffer atmoBuf) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  LutSamp = lutSamp;
  SkyLutView = skyLutView;
  AtmoBuf = atmoBuf;

  const std::string src = std::string(kSceneScaleWGSL) + kAtmoCommon + kAtmoSample + HazeConstsWGSL() + kHazeWGSL
                        + kSpritesWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attrs[4] = {};
  for (int i = 0; i < 4; i++) {
    attrs[i].format = wgpu::VertexFormat::Float32x4;
    attrs[i].offset = (uint64_t)i * 16;
    attrs[i].shaderLocation = (uint32_t)i;
  }
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = kInstFloats * sizeof(float);
  vbl.stepMode = wgpu::VertexStepMode::Instance;
  vbl.attributeCount = 4;
  vbl.attributes = attrs;

  /* Premultiplied source: alpha 0 makes the very same blend PURELY ADDITIVE, which is why one pipeline
   * serves the emitters and the smoke. */
  wgpu::BlendState blend{};
  blend.color.srcFactor = wgpu::BlendFactor::One;
  blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;
  blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  ct.blend = &blend;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = false;   /* an effect never occludes another effect */
  ds.depthCompare = wgpu::CompareFunction::GreaterEqual;   /* reversed-Z: terrain in FRONT occludes */

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = sm;
  /* The scene pass carries a SECOND attachment; a pipeline recorded into it declares two targets
   * whatever it writes. This one leaves the motion attachment alone — it is world-fixed (or it
   * blends without owning the depth), so the resolve reconstructs its pixels from depth. */
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(false)};
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  bd.size = (uint64_t)kMaxSprites * kInstFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  Inst = Device.CreateBuffer(&bd);
  InstScratch.assign((size_t)kMaxSprites * kInstFloats, 0.0f);
  Order.reserve(kMaxSprites);

  wgpu::BindGroupEntry be[4] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].sampler = LutSamp;
  be[2].binding = 2; be[2].textureView = SkyLutView;
  be[3].binding = 3; be[3].buffer = AtmoBuf; be[3].size = kAtmoUniformBytes;
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 4;
  bgd.entries = be;
  Frame = Device.CreateBindGroup(&bgd);

  Ready = true;
}

void SpritesStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  LastDraws_ = 0;
  /* THE EMPTY-WORLD PATH, first thing here: no device or no effects records nothing at all. */
  if (!Ready || Count <= 0) { LogCast(ctx); return; }

  Order.clear();
  for (int i = 0; i < Count && (int)Order.size() < kMaxSprites; i++) {
    const SpriteDraw &s = Sprites[i];
    const double rel[3] = {s.Ecef[0] - ctx.Eye[0], s.Ecef[1] - ctx.Eye[1], s.Ecef[2] - ctx.Eye[2]};
    const double fwd = rel[0] * ctx.Fwd[0] + rel[1] * ctx.Fwd[1] + rel[2] * ctx.Fwd[2];
    if (fwd < -(double)(s.HalfLenM + s.RadiusM)) continue;   /* wholly behind the eye */
    Order.push_back(i);
  }
  if (Order.empty()) { LogCast(ctx); return; }

  /* Far to near: the alpha-blended half is order-dependent, the additive half is not. */
  std::sort(Order.begin(), Order.end(), [&](int a, int b) {
    const SpriteDraw &x = Sprites[a], &y = Sprites[b];
    double dx = 0.0, dy = 0.0;
    for (int k = 0; k < 3; k++) {
      const double ax = x.Ecef[k] - ctx.Eye[k], ay = y.Ecef[k] - ctx.Eye[k];
      dx += ax * ax; dy += ay * ay;
    }
    return dx > dy;
  });

  int n = 0;
  for (int idx : Order) {
    const SpriteDraw &s = Sprites[idx];
    float *o = InstScratch.data() + (size_t)n * kInstFloats;
    for (int k = 0; k < 3; k++) o[k] = (float)(s.Ecef[k] - ctx.Eye[k]);
    o[3] = s.RadiusM;
    for (int k = 0; k < 3; k++) o[4 + k] = s.Axis[k];
    o[7] = s.HalfLenM;
    for (int k = 0; k < 3; k++) o[8 + k] = s.Color[k];
    o[11] = s.Alpha;
    o[12] = (float)s.Kind;
    o[13] = s.Param;
    o[14] = s.Phase;
    o[15] = s.Seed;
    n++;
  }
  Queue.WriteBuffer(Inst, 0, InstScratch.data(), (size_t)n * kInstFloats * sizeof(float));

  float uni[kUniFloats];
  std::memcpy(uni, ctx.Mvp20, 16 * sizeof(float));
  uni[16] = (float)ctx.Width;
  uni[17] = (float)ctx.Height;
  uni[18] = 1.7320508f;   /* 1/tan(30 deg): the scene's own 60 deg vertical field */
  uni[19] = 0.0f;
  const double eyeLen = std::sqrt(ctx.Eye[0] * ctx.Eye[0] + ctx.Eye[1] * ctx.Eye[1] + ctx.Eye[2] * ctx.Eye[2]);
  uni[20] = HazeSigma0(Sky.VisibilityM);
  uni[21] = ctx.AltM;
  uni[22] = (float)((eyeLen - (double)ctx.AltM) / 1.0e6);
  uni[23] = 0.0f;
  const float sunUp = (float)(ctx.SunDir[0] * ctx.Up[0] + ctx.SunDir[1] * ctx.Up[1] + ctx.SunDir[2] * ctx.Up[2]);
  for (int i = 0; i < 3; i++) {
    const CloudDeckParams &dk = Sky.Deck[i];
    uni[24 + i * 4 + 0] = dk.BaseM;
    uni[24 + i * 4 + 1] = dk.TopM;
    uni[24 + i * 4 + 2] = DeckSunOpticalDepth(dk, sunUp);
    uni[24 + i * 4 + 3] = dk.Cover;
  }
  Queue.WriteBuffer(Uni, 0, uni, sizeof uni);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Frame);
  pass.SetVertexBuffer(0, Inst);
  pass.Draw(6, (uint32_t)n);
  LastDraws_ = n;
  LogCast(ctx);
}

/* How many effects the world offered and how many reached the pass — the "no effects, no draw"
 * contract as a number rather than as a claim — plus WHERE the first sprite of each kind landed,
 * through the SAME Mvp the draw used, root end and tip end apart: a screenshot can then be held
 * against the published anchor, and an inverted axis is a number rather than an impression. */
void SpritesStage::LogCast(const FrameContext &ctx) {
  int kinds[kSpriteKinds] = {};
  for (int i = 0; i < Count; i++)
    if (Sprites[i].Kind < kSpriteKinds) kinds[Sprites[i].Kind]++;
  bool changed = false;
  for (int k = 0; k < kSpriteKinds; k++) {
    changed = changed || kinds[k] != LastKindCount_[k];
    LastKindCount_[k] = kinds[k];
  }
  if (!changed && ctx.FrameNo % kSpriteLogEvery != 1) return;
  Log::Debug("render", "sprites", {{"cast", Count}, {"drawn", LastDraws_},
                                     {"flame", kinds[0]}, {"flare", kinds[1]}, {"smoke", kinds[2]},
                                     {"blast", kinds[3]}, {"fire", kinds[4]}, {"light", kinds[5]}});
  bool shown[kSpriteKinds] = {};
  for (int i = 0; i < Count; i++) {
    const SpriteDraw &s = Sprites[i];
    if (s.Kind >= kSpriteKinds || shown[s.Kind]) continue;
    shown[s.Kind] = true;
    double px[2], py[2], range = 0.0;
    for (int e = 0; e < 2; e++) {
      const double sgn = e == 0 ? -1.0 : 1.0;
      float clip[4] = {0, 0, 0, 0};
      const float p[4] = {(float)(s.Ecef[0] - ctx.Eye[0] + sgn * s.Axis[0] * s.HalfLenM),
                          (float)(s.Ecef[1] - ctx.Eye[1] + sgn * s.Axis[1] * s.HalfLenM),
                          (float)(s.Ecef[2] - ctx.Eye[2] + sgn * s.Axis[2] * s.HalfLenM), 1.0f};
      for (int r = 0; r < 4; r++)
        for (int k = 0; k < 4; k++) clip[r] += ctx.Mvp20[k * 4 + r] * p[k];
      const double w = clip[3] != 0.0f ? (double)clip[3] : 1.0;
      px[e] = 0.5 * (clip[0] / w + 1.0) * ctx.Width;
      py[e] = 0.5 * (1.0 - clip[1] / w) * ctx.Height;
      if (e == 0) range = w;
    }
    Log::Debug("render", "sprite_draw",
                 {{"kind", (int)s.Kind}, {"rangeM", range}, {"halfLenM", s.HalfLenM},
                  {"radiusM", s.RadiusM}, {"alpha", s.Alpha},
                  {"rootPx", px[0]}, {"rootPy", py[0]}, {"tipPx", px[1]}, {"tipPy", py[1]}});
  }
}

} // namespace outshine::Render
