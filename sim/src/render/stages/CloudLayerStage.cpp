#include "CloudLayerStage.h"
#include "SceneTargets.h"
#include "AtmoCommon.h"
#include "AtmoSample.h"
#include "SceneScale.h"
#include "AtmoHaze.h"
#include "CloudDensityWGSL.h"
#include "CloudShadow.h"
#include "Log.h"
#include <cmath>
#include <cstring>
#include <string>

namespace outshine::Render {

/* Look/material constants of the MARCH (the shape constants live in core/CloudDensity.h, and the
 * AIR — Koschmieder, the two haze scale heights, kMinSunUp — in AtmoHaze.h, shared with the terrain). All
 * [SET] except where a derivation is named. */
static constexpr float kSunIntensity   = 18.0f;   /* [SET] against kSceneExposure (stages/SceneScale.h) */
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
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var svLUT : texture_2d<f32>;
@group(0) @binding(3) var tLUT : texture_2d<f32>;
@group(0) @binding(4) var depthTex : texture_depth_2d;
@group(0) @binding(5) var<uniform> S : CloudSkyU;

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
 * see core/CloudDensity.h: the amplitude route would brighten the deck by the erosion's mean). */
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
   * depth = zNear / (t * cos), so t = zNear / (depth * cos). zNear = 0.05 m (Renderer::MvpCamRel). */
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
   * THE shared air (AtmoHaze.h), the identical two calls the terrain makes, so a ridge and the deck
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

/* THE SHEET — the deck as a LAYER on the dome, which is what Witcher 3 and Fallout 4 did and why it
 * was cheap. It is NOT a second cloud field: it reads the same CloudSkyU the march reads and the
 * ground shadows itself against, so the cloud drawn and the shadow cast still belong to each other.
 *
 * It is the march's own integrand at ONE node — the ray's crossing of the deck's mid shell — with the
 * analytic slant chord as the interval. Same density function, same three multi-scatter octaves, same
 * phase, same kSunIntensity and kAmbientFloor, same aerial perspective. What it gives up is structure
 * INSIDE the deck, which from 1.7 m under a 1 200 m base is 95 % of the visible sky at over 6 km
 * slant, where a 55 % field with 16 km features integrates to opaque anyway. SetCloudQuality > 0
 * brings the march back for the scene where that is no longer true (a camera at deck height). */
static const char *kCloudSheetWGSL = R"(
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var svLUT : texture_2d<f32>;
@group(0) @binding(3) var tLUT : texture_2d<f32>;
@group(0) @binding(4) var<uniform> S : CloudSkyU;

struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var corners = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  o.pos = vec4f(corners[i], 0.0, 1.0);
  o.ndc = corners[i];
  return o;
}

fn ignoiseSheet(p : vec2f) -> f32 {
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
}
fn shPhase(c : f32, g : f32) -> f32 {
  let g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * c, 1.5));
}

/* Ray to a shell of radius rad, the crossing a viewer under the deck sees. Negative = the ray never
 * reaches it (looking down, or the deck is behind). */
fn shellHit(cam : vec3f, dir : vec3f, rad : f32) -> f32 {
  let b = dot(cam, dir);
  let disc = b * b - dot(cam, cam) + rad * rad;
  if (disc <= 0.0) { return -1.0; }
  let t = -b + sqrt(disc);
  return select(-1.0, t, t > 0.0);
}

@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let cam = A.camPosMm.xyz;
  let dir = camRay(A, in.ndc);
  let gR = S.p0.x;
  let camAltM = (length(cam) - gR) * 1.0e6;
  let up0 = normalize(cam);

  let cosT = dot(dir, A.sunDir.xyz);
  let phase = mix(shPhase(cosT, 0.8), shPhase(cosT, -0.5), 0.5);
  let camHill = up0 * (groundRadiusMM + max(length(cam) - gR, 0.0));
  let sunCol = textureSampleLevel(tLUT, lsamp, tLUTuv(camHill, A.sunDir.xyz), 0.0).rgb;
  let skyAmb = skyViewSample(svLUT, lsamp, A, up0);
  let horizonCol = skyViewSample(svLUT, lsamp, A, normalize(A.sunDir.xyz - up0 * dot(A.sunDir.xyz, up0)));

  var rgb = vec3f(0.0);
  var transm = 1.0;
  /* Front to back is index order here: the three decks are ordered by altitude and the viewer stands
   * under all of them, so a sort would answer the question it is already given. */
  for (var i = 0; i < 3; i = i + 1) {
    let d = S.deck[i];
    if (d.cover <= 0.0 || transm < 0.02) { continue; }
    let thick = max(d.topM - d.baseM, 1.0);
    let zMid = 0.5 * (d.baseM + d.topM);
    if (zMid <= camAltM) { continue; }
    let tMm = shellHit(cam, dir, gR + zMid * 1.0e-6);
    if (tMm <= 0.0) { continue; }
    let hit = cam + dir * tMm;
    let relM = dir * (tMm * 1.0e6);
    let eastM = S.axE.w + dot(relM, S.axE.xyz);
    let northM = S.axN.w + dot(relM, S.axN.xyz);
    /* THE density function itself, at the node — not a second formula. The erosion is band-limited by
     * DISTANCE with the march's own two constants, because at the horizon the crossing is 100 km out
     * and a 1.6 km erosion cell is narrower than a pixel; beyond kErodeFadeFarM it collapses to its
     * mean, which is what a correctly filtered field does. */
    let distM = tMm * 1.0e6;
    var dl = d;
    dl.erodeFlat = clamp((distM - kErodeFadeNearM) / (kErodeFadeFarM - kErodeFadeNearM), 0.0, 1.0);
    let dens = cloudDensity(dl, eastM, northM, 0.5);
    if (dens <= 0.002) { continue; }

    /* The chord through the layer, from the shell geometry rather than from a written-down angle:
     * at the horizon it grows without a special case and the deck closes up by itself. */
    let sinEl = max(dot(dir, normalize(hit)), 1.0e-3);
    let pathM = min(thick / sinEl, kMaxSegM);
    let alphaI = 1.0 - exp(-dens * d.sigma * pathM);

    /* The march's own lighting, one node: three Wrenninge octaves against the deck's slant optical
     * depth, which is S.tau — the SAME number the ground's shadow is computed from. */
    let od = S.tau[i] * dens;
    var ms = 0.0;
    var att = 1.0; var sharp = 1.0; var contrib = 1.0;
    for (var o = 0; o < 3; o = o + 1) {
      ms = ms + contrib * mix(0.5 / (4.0 * PI), phase, sharp) * exp(-od * att);
      att = att * 0.5; sharp = sharp * 0.5; contrib = contrib * 0.55;
    }
    let powder = 1.0 - exp(-dens * 6.0);
    let sunLit = ms * sunCol * kSunIntensity * (0.25 + 0.75 * powder);
    let ambGrad = kAmbientFloor + (1.0 - kAmbientFloor) * sqrt(clamp(1.0 - dens, 0.0, 1.0));
    /* h = 0 at the node: what a viewer under the deck sees IS its base, and the march's height mix
     * evaluated there is what that base is worth. */
    let amb = (skyAmb * 0.62 + horizonCol * 0.55) * 0.7 * ambGrad;

    let hz = hazeTransmittance3(S.p0.z, 0.5 * (camAltM + zMid), distM);
    let lay = (sunLit + amb) * hz + hazeInscatter(svLUT, lsamp, A, dir) * (vec3f(1.0) - hz);
    rgb = rgb + transm * alphaI * lay;
    transm = transm * (1.0 - alphaI);
  }
  var alpha = clamp(1.0 - transm, 0.0, 1.0);
  if (alpha < 0.001) { return vec4f(0.0); }
  let dth = (ignoiseSheet(in.pos.xy + vec2f(37.0, 17.0)) - 0.5) * kDitherRel;
  rgb = rgb * (1.0 + dth);
  alpha = clamp(alpha * (1.0 + dth), 0.0, 1.0);
  return vec4f(max(rgb, vec3f(0.0)), alpha);
}
)";

void CloudLayerStage::Configure(const Gpu &gpu, wgpu::Buffer atmoBuf, wgpu::Buffer cloudBuf,
                                  wgpu::Sampler lutSamp, wgpu::TextureView skyLUTView,
                                  wgpu::TextureView transLUTView, wgpu::TextureView depthView) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  char mats[1024];
  snprintf(mats, sizeof mats,
           "const kSunIntensity : f32 = %.9g;\nconst kMaxSegM : f32 = %.9g;\n"
           "const kAmbientFloor : f32 = %.9g;\nconst kDitherRel : f32 = %.9g;\n"
           "const kErodeFadeNearM : f32 = %.9g;\nconst kErodeFadeFarM : f32 = %.9g;\n"
           "const kErodeNyqLo : f32 = %.9g;\nconst kErodeNyqHi : f32 = %.9g;\n",
           (double)kSunIntensity, (double)kMaxSegM, (double)kAmbientFloor,
           (double)kDitherRel, (double)kErodeFadeNearM, (double)kErodeFadeFarM,
           (double)kErodeNyqLo, (double)kErodeNyqHi);
  const std::string src = std::string(kSceneScaleWGSL) + kAtmoCommon + kAtmoSample + HazeConstsWGSL() + kHazeWGSL +
                          CloudDensityConstsWGSL() + mats + kCloudDensityWGSL + kCloudShadowWGSL
                        + kCloudLayerWGSL;

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  /* Premultiplied OVER, straight into the HDR scene target — no separate cloud texture, no composite
   * in the tonemap. rgba16float carries the alpha this needs (Renderer::OnAdapter). */
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
  be[5].binding = 5; be[5].buffer = cloudBuf; be[5].size = kCloudSkyBytes;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 6;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);

  const std::string shSrc = std::string(kSceneScaleWGSL) + kAtmoCommon + kAtmoSample +
                            HazeConstsWGSL() + kHazeWGSL + CloudDensityConstsWGSL() + mats +
                            kCloudDensityWGSL + kCloudShadowWGSL + kCloudSheetWGSL;
  wgpu::ShaderSourceWGSL shWgsl{};
  shWgsl.code = shSrc.c_str();
  wgpu::ShaderModuleDescriptor shSmd{};
  shSmd.nextInChain = &shWgsl;
  wgpu::ShaderModule shM = Device.CreateShaderModule(&shSmd);

  /* Depth Always / no write: the sheet stands behind everything the scene pass draws after it, and
   * the terrain overwrites it wherever there is terrain. */
  wgpu::DepthStencilState shDs{};
  shDs.format = wgpu::TextureFormat::Depth32Float;
  shDs.depthWriteEnabled = false;
  shDs.depthCompare = wgpu::CompareFunction::Always;
  wgpu::RenderPipelineDescriptor shRp{};
  shRp.vertex.module = shM;
  wgpu::FragmentState shFs{};
  shFs.module = shM;
  /* The SHEET is drawn in the scene pass and therefore declares its second target; the MARCH above
   * has a pass of its own with one attachment. A deck is blended and owns no depth, so it writes no
   * motion — the resolve reprojects those pixels off the terrain or sky behind them. */
  wgpu::ColorTargetState shCts[2] = {ct, VelocityTarget(false)};
  shFs.targetCount = 2;
  shFs.targets = shCts;
  shRp.fragment = &shFs;
  shRp.depthStencil = &shDs;
  SheetPipe = Device.CreateRenderPipeline(&shRp);

  wgpu::BindGroupEntry sbe[5] = {};
  sbe[0].binding = 0; sbe[0].buffer = atmoBuf; sbe[0].size = kAtmoUniformBytes;
  sbe[1].binding = 1; sbe[1].sampler = lutSamp;
  sbe[2].binding = 2; sbe[2].textureView = skyLUTView;
  sbe[3].binding = 3; sbe[3].textureView = transLUTView;
  sbe[4].binding = 4; sbe[4].buffer = cloudBuf; sbe[4].size = kCloudSkyBytes;
  wgpu::BindGroupDescriptor sbg{};
  sbg.layout = SheetPipe.GetBindGroupLayout(0);
  sbg.entryCount = 5;
  sbg.entries = sbe;
  SheetBind = Device.CreateBindGroup(&sbg);
}

void CloudLayerStage::Update(const FrameContext &ctx) {
  if (!Sky.Any()) return;
  /* Once, and again whenever a deck's geometry actually changes: "which deck sits where" is the first
   * thing to check when a frame looks wrong, and it is not visible anywhere else. */
  if (std::fabs(Sky.Deck[0].BaseM - LoggedBase[0]) > 1.0f || std::fabs(Sky.Deck[1].BaseM - LoggedBase[1]) > 1.0f ||
      std::fabs(Sky.Deck[2].BaseM - LoggedBase[2]) > 1.0f || !LoggedOnce) {
    LoggedOnce = true;
    for (int i = 0; i < 3; i++) LoggedBase[i] = Sky.Deck[i].BaseM;
    Log::Debug("render", "cloud_decks",
                 {{"camAltM", (double)ctx.AltM}, {"visM", (double)Sky.VisibilityM},
                  {"lowCover", (double)Sky.Deck[0].Cover}, {"lowBaseM", (double)Sky.Deck[0].BaseM}, {"lowTopM", (double)Sky.Deck[0].TopM},
                  {"midCover", (double)Sky.Deck[1].Cover}, {"midBaseM", (double)Sky.Deck[1].BaseM}, {"midTopM", (double)Sky.Deck[1].TopM},
                  {"highCover", (double)Sky.Deck[2].Cover}, {"highBaseM", (double)Sky.Deck[2].BaseM}, {"highTopM", (double)Sky.Deck[2].TopM},
                  {"highWindE", (double)Sky.Deck[2].WindDirE}, {"highWindN", (double)Sky.Deck[2].WindDirN}});
  }
}

void CloudLayerStage::Encode(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  if (!Active()) return;
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

void CloudLayerStage::EncodeSheet(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  if (!SheetActive() || !SheetPipe) return;
  pass.SetPipeline(SheetPipe);
  pass.SetBindGroup(0, SheetBind);
  pass.Draw(3);
}

} // namespace outshine::Render
