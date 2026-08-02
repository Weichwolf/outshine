#include "FBCloudLayerStage.h"
#include "FBAtmoCommon.h"
#include "FBAtmoSample.h"
#include "FBAtmoHaze.h"
#include "FBCloudDensityWGSL.h"
#include "FBLog.h"
#include <cmath>
#include <cstring>
#include <string>

namespace FlightBox::Render {

/* Look/material constants of the MARCH (the shape constants live in core/FBCloudDensity.h, and the
 * AIR — Koschmieder, the two haze scale heights, kMinSunUp — in FBAtmoHaze.h, shared with the terrain). All
 * [SET] except where a derivation is named. */
static constexpr float kSunIntensity   = 18.0f;   /* [SET] against kSkyExposure = 8 (FBAtmoSample.h) */
static constexpr float kMaxSegM        = 60000.0f;/* [SET] longest marched span through one deck */
static constexpr float kAmbientFloor   = 0.18f;   /* [SET] a dense base is mid-grey, not black */
static constexpr float kDitherRel      = 0.0025f;  /* ~1/255 relative — one 8-bit step at the output */
static constexpr float kErodeFadeNearM = 8000.0f; /* [SET] erosion starts fading out at 8 km ... */
static constexpr float kErodeFadeFarM  = 45000.0f;/* [SET] ... and is gone by 45 km (undersampled) */
static constexpr float kErodeNyqLo     = 0.15f;   /* [SET] erosion cells crossed per step: full amplitude below ... */
static constexpr float kErodeNyqHi     = 0.60f;   /* [SET] ... gone at (just under) one cell per step = Nyquist */

/* Prepends kAtmoCommon (Atmo, PI, groundRadiusMM, tLUTuv) + kAtmoSample (skyViewSample) + the shared
 * density function and its emitted constants. */
static const char *kCloudLayerWGSL = R"(
struct Sky {
  axE : vec4f,          /* xyz = ECEF east axis at the anchor, w = camera east offset (m) */
  axN : vec4f,          /* xyz = ECEF north axis, w = camera north offset (m) */
  p0 : vec4f,           /* x = ground radius (Mm), y = quality, z = haze sigma0 (1/m), w = camera altitude (m) */
  p1 : vec4f,           /* spare */
  deck : array<CloudDeck, 3>,
};
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var svLUT : texture_2d<f32>;
@group(0) @binding(3) var tLUT : texture_2d<f32>;
@group(0) @binding(4) var depthTex : texture_depth_2d;
@group(0) @binding(5) var<uniform> S : Sky;

/* Per-pixel values every helper needs. var<private> instead of a parameter bundle: the alternative is
 * threading eleven arguments through three functions. */
var<private> gCam : vec3f;
var<private> gDir : vec3f;
var<private> gSun : vec3f;
var<private> gSunE : f32;
var<private> gSunN : f32;
var<private> gGroundR : f32;
var<private> gJitter : f32;
var<private> gSunCol : vec3f;
var<private> gSkyAmb : vec3f;
var<private> gHorizonCol : vec3f;
var<private> gPhase : f32;

struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  o.pos = vec4f(corners[i], 0.0, 1.0);
  o.ndc = corners[i];
  return o;
}

/* Interleaved-gradient noise: blue-ish, and SPATIAL ONLY. No frame term anywhere in this shader —
 * without a temporal resolve a per-frame jitter is just flicker. */
fn ignoise(p : vec2f) -> f32 {
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
}

fn hgPhase(c : f32, g : f32) -> f32 {
  let g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * c, 1.5));
}

/* Ray ∩ spherical shell, ONE segment (x = enter, y = exit; empty when x >= y). The three cases —
 * camera below the deck, inside it, above it — fall out of the same two quadratics, which is what makes
 * a fly-through seamless: nothing switches code path at the boundary. */
fn shellSegment(rIn : f32, rOut : f32) -> vec2f {
  let b = dot(gCam, gDir);
  let r2 = dot(gCam, gCam);
  let discOut = b * b - r2 + rOut * rOut;
  if (discOut <= 0.0) { return vec2f(1.0, 0.0); }
  let sOut = sqrt(discOut);
  var t0 = -b - sOut;
  var t1 = -b + sOut;
  if (t1 <= 0.0) { return vec2f(1.0, 0.0); }
  let discIn = b * b - r2 + rIn * rIn;
  if (discIn > 0.0) {
    let sIn = sqrt(discIn);
    let i0 = -b - sIn;
    let i1 = -b + sIn;
    if (r2 < rIn * rIn) {
      t0 = i1;                                  /* below the deck: the band starts past its base */
    } else {
      if (i0 > 0.0) { t1 = min(t1, i0); }       /* looking down at the base: stop there */
    }
  }
  t0 = max(t0, 0.0);                            /* camera inside the band -> the segment starts at it */
  return vec2f(t0, t1);
}

/* Optical depth toward the sun, CLOSED FORM over the remaining deck thickness plus two taps into the
 * 2-D field (spec: no secondary march). The taps carry their own height, so a tap that leaves the deck
 * through the top contributes nothing — the profile does that by itself. */
fn sunOpticalDepth(d : CloudDeck, eastM : f32, northM : f32, h : f32, dens : f32, sunUp : f32) -> f32 {
  let thick = d.topM - d.baseM;
  let remain = (1.0 - h) * thick / max(sunUp, kMinSunUp);   /* metres of deck left toward the sun */
  var occl = dens;
  for (var k = 1; k <= 2; k = k + 1) {
    let frac = f32(k) / 3.0;
    let hk = min(h + (1.0 - h) * frac, 0.999);
    let stepM = remain * frac * sqrt(max(1.0 - sunUp * sunUp, 0.0));   /* the HORIZONTAL leg of that slant path */
    let s = cloudCoverage(d, eastM + gSunE * stepM, northM + gSunN * stepM);
    occl = occl + cloudShape(s.coverage, hk);
  }
  return d.sigma * remain * (occl / 3.0);
}

struct Acc { scat : vec3f, transm : f32, wDist : f32, wAlt : f32, wSum : f32 };

/* One NODE of the quadrature: the density at a ray parameter plus the geometry the lighting needs
 * there. Erosion is the highest frequency in the field, so it is also the first thing a 6-12 step
 * march undersamples. The FUNCTION is untouched — only this sample's deck parameters are, exactly as
 * a mip level is a parameter of a texture fetch rather than a different texture. */
struct Node { dens : f32, h : f32, eastM : f32, northM : f32, radius : f32 };

/* THE erosion prefilter, and the measured reason the march is grainy at all. The erosion lattice is
 * featureM/kCloudErodeFreq wide (16 km / 10 = 1.6 km) and thick/kCloudErodeVert tall (900 m / 3 =
 * 300 m); a step through a 900 m deck is ~260 m long. Looking DOWN through the deck a step therefore
 * crosses about half an erosion cell — the field is sampled at its own Nyquist rate, and what comes
 * out is not detail but aliasing that the entry jitter converts into per-pixel grain. MEASURED at the
 * undercast camera (high-pass std/mean over a flat deck): 0.0388 as committed, 0.0328 with this filter
 * and the trapezoid below, and 0.0149 with the erosion term removed altogether — that last one is the
 * control that identified the erosion as the source in the first place.
 *
 * So the erosion is band-limited by how well THIS step resolves it — cells crossed per step, the same
 * quantity a mip footprint measures — and by distance, which is the older, cruder proxy for the same
 * thing. Where you fly THROUGH the deck the step is near-horizontal against a 1.6 km cell and the
 * erosion survives in full; where you cross it steeply from above it collapses to its own MEAN, which
 * is what a correctly filtered field does (and why it is `erodeFlat` and not a scale on `erosion` —
 * see core/FBCloudDensity.h: the amplitude route would brighten the deck by the erosion's mean). */
fn erodeFlatness(d : CloudDeck, thick : f32, stepM : f32, up : vec3f, tM : f32) -> f32 {
  let cellH = d.featureM / kCloudErodeFreq;
  let cellV = thick / kCloudErodeVert;
  let sv = abs(dot(gDir, up));                        /* vertical share of the step direction */
  let sh = sqrt(max(1.0 - sv * sv, 0.0));
  let cells = stepM * sqrt((sh / cellH) * (sh / cellH) + (sv / cellV) * (sv / cellV));
  return max(fbSmooth(kErodeNyqLo, kErodeNyqHi, cells),
             fbSmooth(kErodeFadeNearM, kErodeFadeFarM, tM));
}

fn nodeAt(d : CloudDeck, rBase : f32, thick : f32, stepM : f32, t : f32) -> Node {
  let rel = gDir * t;
  let pos = gCam + rel;
  var n : Node;
  n.radius = length(pos);
  n.h = (n.radius - rBase) * 1.0e6 / thick;
  n.eastM = S.axE.w + dot(rel, S.axE.xyz) * 1.0e6;
  n.northM = S.axN.w + dot(rel, S.axN.xyz) * 1.0e6;
  n.dens = 0.0;
  if (n.h >= 0.0 && n.h <= 1.0) {
    var dd = d;
    dd.erodeFlat = max(d.erodeFlat, erodeFlatness(d, thick, stepM, pos / n.radius, t * 1.0e6));
    n.dens = cloudDensity(dd, n.eastM, n.northM, n.h);
  }
  return n;
}

/* COMPOSITE TRAPEZOID over the segment, not a rectangle rule at a jittered offset — and it is an
 * ACCURACY decision far more than a noise one, which is the opposite of what it was tried for. The
 * rectangle rule applies the ENTRY density of each step over that whole step and then early-terminates
 * on transm < 0.02, so against an optically thick deck (sigma*thickness = 20) it stops after one or two
 * samples having over-attenuated both: MEASURED, it renders the deck 3.7 % darker than the converged
 * reference, where the trapezoid at the same node count lands within 0.8 %. Its nodes SHARE their ends,
 * so the sum telescopes to h*(f0/2 + f1 + ... + fn/2) and is unbiased for a linear density.
 *
 * On the GRAIN it is worth about a tenth at equal node count — the per-pixel scatter is dominated by
 * the erosion aliasing the band-limit above removes, not by the quadrature rule. Both numbers and the
 * five rejected alternatives are in doc/render/clouds.md.
 *
 * The jitter stays (spec: blue-noise), now as a shift of the INTERIOR nodes between the segment's two
 * TRUE ends: nodes t0, t0+(k+j)h (k=0..steps-1), t1. Total width stays steps*h, and both ends are
 * exact, so nothing is dropped at the entry the way an offset rectangle rule drops it. */
fn marchDeck(accIn : Acc, d : CloudDeck, seg : vec2f) -> Acc {
  var acc = accIn;
  if (d.cover <= 0.0 || seg.x >= seg.y) { return acc; }
  let thick = d.topM - d.baseM;
  let rBase = gGroundR + d.baseM * 1.0e-6;
  var t1 = min(seg.y, seg.x + kMaxSegM * 1.0e-6);
  let segLenM = (t1 - seg.x) * 1.0e6;
  /* 6-12 SAMPLES per segment (the spec's budget), and with a trapezoid the samples are the NODES: a
   * segment split into n intervals evaluates the density n+2 times (both true ends plus the jittered
   * interior). Solving for the interval count keeps the tap count at the budget instead of 2 above it. */
  let nNodes = i32(clamp(segLenM / (thick * 0.35), 6.0, 12.0) * clamp(S.p0.y, 0.25, 8.0) + 0.5);
  let steps = max(nNodes - 2, 3);
  let stepMm = (t1 - seg.x) / f32(steps);
  var tA = seg.x;
  var nA = nodeAt(d, rBase, thick, stepMm * 1.0e6, tA);
  for (var i = 0; i <= steps; i = i + 1) {
    if (acc.transm < 0.02) { break; }
    var tB = seg.x + stepMm * (f32(i) + gJitter);
    if (i == steps) { tB = t1; }
    let dtMm = tB - tA;
    if (dtMm > 0.0) {
      let nB = nodeAt(d, rBase, thick, dtMm * 1.0e6, tB);
      let dens = 0.5 * (nA.dens + nB.dens);      /* the trapezoid's own value for this interval */
      if (dens > 0.002) {
        let tm = 0.5 * (tA + tB);
        let h = 0.5 * (nA.h + nB.h);
        let eastM = 0.5 * (nA.eastM + nB.eastM);
        let northM = 0.5 * (nA.northM + nB.northM);
        let pos = gCam + gDir * tm;
        let radius = length(pos);
        let up = pos / radius;
        let sunUp = dot(gSun, up);
        let od = sunOpticalDepth(d, eastM, northM, h, dens, sunUp);
        var ms = 0.0;                            /* Wrenninge multi-scatter octaves */
        var att = 1.0; var sharp = 1.0; var contrib = 1.0;
        for (var o = 0; o < 3; o = o + 1) {
          let msPhase = mix(0.5 / (4.0 * PI), gPhase, sharp);
          ms = ms + contrib * msPhase * exp(-od * att);
          att = att * 0.5; sharp = sharp * 0.5; contrib = contrib * 0.55;
        }
        let powder = 1.0 - exp(-dens * 6.0);
        let sunLit = ms * gSunCol * kSunIntensity * (0.25 + 0.75 * powder);
        let ambGrad = kAmbientFloor + (1.0 - kAmbientFloor) * sqrt(clamp(1.0 - dens, 0.0, 1.0));
        let amb = (gSkyAmb * mix(0.62, 0.85, h) + gHorizonCol * 0.55 * (1.0 - h)) * 0.7 * ambGrad;
        let sigma = dens * d.sigma;
        let tr = exp(-sigma * dtMm * 1.0e6);
        let w = acc.transm * (1.0 - tr);
        acc.scat = acc.scat + w * (sunLit + amb);
        acc.wDist = acc.wDist + w * tm;
        acc.wAlt = acc.wAlt + w * (radius - gGroundR) * 1.0e6;
        acc.wSum = acc.wSum + w;
        acc.transm = acc.transm * tr;
      }
      nA = nB;
      tA = tB;
    }
  }
  return acc;
}

@fragment fn fs(in : VOut) -> @location(0) vec4f {
  gCam = A.camPosMm.xyz;
  gDir = camRay(A, in.ndc);
  gSun = A.sunDir.xyz;
  gGroundR = S.p0.x;
  gSunE = dot(gSun, S.axE.xyz);
  gSunN = dot(gSun, S.axN.xyz);
  gJitter = ignoise(in.pos.xy);

  /* Scene depth is the far clamp for every segment: reversed-Z infinite projection,
   * depth = zNear / (t * cos), so t = zNear / (depth * cos). zNear = 0.05 m (FBRenderer::MvpCamRel). */
  var tScene = 1.0e9;
  let dep = textureLoad(depthTex, vec2i(i32(in.pos.x), i32(in.pos.y)), 0);
  if (dep > 1.0e-9) {
    tScene = (0.05 / (dep * max(dot(gDir, A.camFwd.xyz), 1.0e-3))) * 1.0e-6;
  }

  var seg : array<vec2f, 3>;
  var order : array<i32, 3>;
  var any = false;
  for (var i = 0; i < 3; i = i + 1) {
    let d = S.deck[i];
    var s = vec2f(1.0, 0.0);
    if (d.cover > 0.0) {
      s = shellSegment(gGroundR + d.baseM * 1.0e-6, gGroundR + d.topM * 1.0e-6);
      s.y = min(s.y, tScene);
      if (s.x < s.y) { any = true; }
    }
    seg[i] = s;
    order[i] = i;
  }
  if (!any) { return vec4f(0.0); }   /* premultiplied zero = a no-op blend; cheaper than discard */

  /* Front to back over at most three segments — an insertion sort of three entries, so the composite
   * is correct whether the camera is under, in or over any of the decks. */
  for (var i = 1; i < 3; i = i + 1) {
    let key = order[i];
    var j = i - 1;
    while (j >= 0 && seg[order[j]].x > seg[key].x) {
      order[j + 1] = order[j];
      j = j - 1;
    }
    order[j + 1] = key;
  }

  let up0 = normalize(gCam);
  let camAltMm = length(gCam) - gGroundR;
  let camHill = up0 * (groundRadiusMM + max(camAltMm, 0.0));
  gSunCol = textureSampleLevel(tLUT, lsamp, tLUTuv(camHill, gSun), 0.0).rgb;
  gSkyAmb = skyViewSample(svLUT, lsamp, A, up0);
  gHorizonCol = skyViewSample(svLUT, lsamp, A, normalize(gSun - up0 * dot(gSun, up0)));
  let cosT = dot(gDir, gSun);
  gPhase = mix(hgPhase(cosT, 0.8), hgPhase(cosT, -0.5), 0.5);   /* dual-lobe HG: the silver rim */

  var acc : Acc;
  acc.scat = vec3f(0.0); acc.transm = 1.0; acc.wDist = 0.0; acc.wAlt = 0.0; acc.wSum = 0.0;
  for (var i = 0; i < 3; i = i + 1) { acc = marchDeck(acc, S.deck[order[i]], seg[order[i]]); }

  var alpha = clamp(1.0 - acc.transm, 0.0, 1.0);
  if (alpha < 0.001) { return vec4f(0.0); }

  /* Aerial perspective on the cloud itself: the deck dissolves into the haze the weather reported —
   * THE shared air (FBAtmoHaze.h), the identical two calls the terrain makes, so a ridge and the deck
   * over it fade at one rate into one colour. */
  let meanDistM = acc.wDist / max(acc.wSum, 1.0e-6) * 1.0e6;
  let meanAltM = acc.wAlt / max(acc.wSum, 1.0e-6);
  let hz = hazeTransmittance3(S.p0.z, 0.5 * (camAltMm * 1.0e6 + meanAltM), meanDistM);
  var rgb = acc.scat * hz + hazeInscatter(svLUT, lsamp, A, gDir) * (vec3f(1.0) - hz) * alpha;

  /* Dither at the output, relative so it costs one 8-bit step wherever the value lands after ACES. */
  let dth = (ignoise(in.pos.xy + vec2f(37.0, 17.0)) - 0.5) * kDitherRel;
  rgb = rgb * (1.0 + dth);
  alpha = clamp(alpha * (1.0 + dth), 0.0, 1.0);
  return vec4f(max(rgb, vec3f(0.0)), alpha);   /* premultiplied over the HDR scene */
}
)";

void FBCloudLayerStage::Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                                  wgpu::TextureView skyLUTView, wgpu::TextureView transLUTView,
                                  wgpu::TextureView depthView) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  wgpu::BufferDescriptor bd{};
  bd.size = 4 * 4 * sizeof(float) + 3 * sizeof(FBCloudDeckParams);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  char mats[1024];
  snprintf(mats, sizeof mats,
           "const kSunIntensity : f32 = %.9g;\nconst kMaxSegM : f32 = %.9g;\n"
           "const kAmbientFloor : f32 = %.9g;\nconst kDitherRel : f32 = %.9g;\n"
           "const kErodeFadeNearM : f32 = %.9g;\nconst kErodeFadeFarM : f32 = %.9g;\n"
           "const kErodeNyqLo : f32 = %.9g;\nconst kErodeNyqHi : f32 = %.9g;\n",
           (double)kSunIntensity, (double)kMaxSegM, (double)kAmbientFloor,
           (double)kDitherRel, (double)kErodeFadeNearM, (double)kErodeFadeFarM,
           (double)kErodeNyqLo, (double)kErodeNyqHi);
  const std::string src = std::string(kAtmoCommon) + kAtmoSample + FBHazeConstsWGSL() + kHazeWGSL +
                          FBCloudDensityConstsWGSL() + mats + kCloudDensityWGSL + kCloudLayerWGSL;

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  /* Premultiplied OVER, straight into the HDR scene target — no separate cloud texture, no composite
   * in the tonemap. rgba16float carries the alpha this needs (FBRenderer::OnAdapter). */
  wgpu::BlendState blend{};
  blend.color.srcFactor = wgpu::BlendFactor::One;
  blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;
  blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
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
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[6] = {};
  be[0].binding = 0; be[0].buffer = atmoBuf; be[0].size = kAtmoUniformBytes;
  be[1].binding = 1; be[1].sampler = lutSamp;
  be[2].binding = 2; be[2].textureView = skyLUTView;
  be[3].binding = 3; be[3].textureView = transLUTView;
  be[4].binding = 4; be[4].textureView = depthView;
  be[5].binding = 5; be[5].buffer = Uni; be[5].size = bd.size;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 6;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
}

void FBCloudLayerStage::Update(const FBFrameContext &ctx) {
  if (!Active()) return;
  /* Once, and again whenever a deck's geometry actually changes: "which deck sits where" is the first
   * thing to check when a frame looks wrong, and it is not visible anywhere else. */
  if (std::fabs(Sky.Deck[0].BaseM - LoggedBase[0]) > 1.0f || std::fabs(Sky.Deck[1].BaseM - LoggedBase[1]) > 1.0f ||
      std::fabs(Sky.Deck[2].BaseM - LoggedBase[2]) > 1.0f || !LoggedOnce) {
    LoggedOnce = true;
    for (int i = 0; i < 3; i++) LoggedBase[i] = Sky.Deck[i].BaseM;
    FBLog::Debug("render", "cloud_decks",
                 {{"camAltM", (double)ctx.AltM}, {"visM", (double)Sky.VisibilityM},
                  {"lowCover", (double)Sky.Deck[0].Cover}, {"lowBaseM", (double)Sky.Deck[0].BaseM}, {"lowTopM", (double)Sky.Deck[0].TopM},
                  {"midCover", (double)Sky.Deck[1].Cover}, {"midBaseM", (double)Sky.Deck[1].BaseM}, {"midTopM", (double)Sky.Deck[1].TopM},
                  {"highCover", (double)Sky.Deck[2].Cover}, {"highBaseM", (double)Sky.Deck[2].BaseM}, {"highTopM", (double)Sky.Deck[2].TopM},
                  {"highWindE", (double)Sky.Deck[2].WindDirE}, {"highWindN", (double)Sky.Deck[2].WindDirN}});
  }
  const double eyeLen = std::sqrt(ctx.Eye[0] * ctx.Eye[0] + ctx.Eye[1] * ctx.Eye[1] + ctx.Eye[2] * ctx.Eye[2]);
  const double groundR = eyeLen - (double)ctx.AltM;   /* the REAL WGS84 radius under the camera */

  if (!HaveAnchor) {
    for (int a = 0; a < 3; a++) Anchor[a] = ctx.Eye[a];
    double up[3] = {ctx.Up[0], ctx.Up[1], ctx.Up[2]};
    const double zax[3] = {0.0, 0.0, 1.0};
    AxisE[0] = zax[1] * up[2] - zax[2] * up[1];
    AxisE[1] = zax[2] * up[0] - zax[0] * up[2];
    AxisE[2] = zax[0] * up[1] - zax[1] * up[0];
    double l = std::sqrt(AxisE[0] * AxisE[0] + AxisE[1] * AxisE[1] + AxisE[2] * AxisE[2]);
    if (l < 1e-9) { AxisE[0] = 1.0; AxisE[1] = 0.0; AxisE[2] = 0.0; l = 1.0; }
    for (int a = 0; a < 3; a++) AxisE[a] /= l;
    AxisN[0] = up[1] * AxisE[2] - up[2] * AxisE[1];
    AxisN[1] = up[2] * AxisE[0] - up[0] * AxisE[2];
    AxisN[2] = up[0] * AxisE[1] - up[1] * AxisE[0];
    HaveAnchor = true;
  }
  const double rel[3] = {ctx.Eye[0] - Anchor[0], ctx.Eye[1] - Anchor[1], ctx.Eye[2] - Anchor[2]};
  const double camE = rel[0] * AxisE[0] + rel[1] * AxisE[1] + rel[2] * AxisE[2];
  const double camN = rel[0] * AxisN[0] + rel[1] * AxisN[1] + rel[2] * AxisN[2];

  float u[4 * 4 + 3 * 16];
  u[0] = (float)AxisE[0]; u[1] = (float)AxisE[1]; u[2] = (float)AxisE[2]; u[3] = (float)camE;
  u[4] = (float)AxisN[0]; u[5] = (float)AxisN[1]; u[6] = (float)AxisN[2]; u[7] = (float)camN;
  u[8] = (float)(groundR / 1.0e6);
  u[9] = (float)Quality;
  u[10] = FBHazeSigma0(Sky.VisibilityM);
  u[11] = ctx.AltM;
  u[12] = 0.0f; u[13] = 0.0f; u[14] = 0.0f; u[15] = 0.0f;
  static_assert(sizeof(FBCloudDeckParams) == 16 * sizeof(float),
                "the deck struct is memcpy'd into the uniform — it must stay 16 tight floats (64 B stride)");
  std::memcpy(&u[16], Sky.Deck, 3 * sizeof(FBCloudDeckParams));
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
}

void FBCloudLayerStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  if (!Active()) return;
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace FlightBox::Render
