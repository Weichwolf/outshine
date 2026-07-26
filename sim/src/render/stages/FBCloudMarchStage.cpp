#include "FBCloudMarchStage.h"
#include "FBAtmoCommon.h"
#include "FBAtmoSample.h"
#include "FBCloudNoiseCommon.h"
#include "FBLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace FlightBox {

static bool CloudCellsOn(void) { const char *e = getenv("FB_CLOUD_CELLS"); return !e || atoi(e) != 0; }

/* Raymarch. Prepend kAtmoCommon (Atmo struct, rayIntersectSphere, groundRadiusMM, PI) + kAtmoSample. */
static const char *kCloudWGSL = R"(
struct Cloud { p0 : vec4f, p1 : vec4f, p2 : vec4f, p3 : vec4f, p4 : vec4f, p5 : vec4f, p6 : vec4f, p7 : vec4f };   /* p0: rBaseMm,rTopMm(ABS),coverage,quality; p1: low,mid,high,time; p2: wind.xyz,densityScale; p3: depthScaleX,depthScaleY,groundR,jitterX; p4: extinction,sunIntensity,detailStrength,jitterY; p5: d2Freq,d2Weight,baseLODbias,coneR; p6: tangent1.xyz,cellSpanKm; p7: tangent2.xyz,- */
@group(0) @binding(0) var<uniform> A : Atmo;
@group(0) @binding(1) var lsamp : sampler;
@group(0) @binding(2) var svLUT : texture_2d<f32>;
@group(0) @binding(3) var tLUT : texture_2d<f32>;
@group(0) @binding(4) var nsamp : sampler;
@group(0) @binding(5) var baseTex : texture_3d<f32>;
@group(0) @binding(6) var detTex : texture_3d<f32>;
@group(0) @binding(7) var depthTex : texture_depth_2d;
@group(0) @binding(8) var<uniform> C : Cloud;
@group(0) @binding(9) var cellTex : texture_2d<f32>;   /* 512² tileable F1-round-cell bump field (B mode) */

/* Cloud-shape switch, baked at shader build from env FB_CLOUD_CELLS (CreateClouds): 0.0 = the deployed
 * Stand-A tower/threshold silhouette (default), 1.0 = the F1-round-cell (B) field. A module const so the
 * unused branch dead-strips — no per-pixel cost, no uniform slot (the Cloud struct is fully packed). */
const CELLS_ON : f32 = 0.0;
/* Moonlight strength, baked from env FB_MOONLIGHT (default 1.0). Scales the moon-as-second-source term
 * (see the march): moonlit clouds are faintly silver-grey and occlude stars; new moon (phase~0) stays
 * near-black. 0.0 disables it entirely (sun-only, the old behaviour). */
const MOONLIGHT : f32 = 1.0;

struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var cc = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  o.pos = vec4f(cc[i], 0.0, 1.0);
  o.ndc = cc[i];
  return o;
}
/* Tower height from a LOW-FREQ base sample (the large-scale mass sets how tall the tower is), decoupled
 * from the local/eroded density so the silhouette doesn't flutter at high frequency (team-lead guardrail /
 * Schneider dimensional-profile). low weather -> flat stratus ceiling, high -> taller cumulus towers. */
fn towerTop(bLow : f32, low : f32, high : f32) -> f32 {
  let topMax = mix(1.0 + 0.15 * high, 0.5, low);   /* cumulus tall (+high) vs stratus flat */
  return mix(0.35, topMax, pow(clamp(bLow, 0.0, 1.0), 1.5));
}
fn density(posMm : vec3f, h : f32, detLod : f32, footKm : f32) -> f32 {
  let posKm = posMm * 1000.0 + C.p2.xyz;   /* world-space km -> noise tiles fixed to the ground */
  /* Footprint-derived mip LOD: a grazing ray's pixel covers a large world span -> sample a COARSE mip so
   * the 128^3/32^3 field isn't read over Nyquist (kills the stable grazing moiré/crosshatch; 04 density-LOD). */
  let lodB0 = max(0.0, log2(max(footKm, 1.0e-5) * 128.0 / 9.0)) + C.p5.z;   /* p5.z = base LOD bias (corduroy test) */
  let lodB1 = max(0.0, log2(max(footKm, 1.0e-5) * 128.0 / 24.0)) + C.p5.z;
  let b0 = textureSampleLevel(baseTex, nsamp, posKm / 9.0, lodB0).r;    /* detail-scale mass */
  let b1 = textureSampleLevel(baseTex, nsamp, posKm / 24.0, lodB1).r;   /* mid-scale mass (intra-cell billow) */
  let b = b1 * 0.6 + b0 * 0.4;             /* horizontal base density [0,1] (contrast-stretched at gen) */
  let thresh = 1.0 - C.p0.z;               /* coverage threshold */
  var d : f32;
  var hNorm : f32;
  if (CELLS_ON > 0.5) {
    /* (B) F1-ROUND CELL FIELD (Candidate 12): discrete round convection cells with gaps = real
     * stratocumulus. ROUNDED DOME via HEIGHT SUBTRACTION (Schneider): cell strength drops with height so
     * the cloud narrows to the cell CENTRE going up. Coverage shifts the threshold: LOW/MID = discrete
     * cells with gaps, HIGH = cells merge into a closed smooth deck (undercast). */
    // Cell field from the dedicated 512² 2D texture, indexed by the HORIZONTAL (tangent-plane) world
    // position -> the cells extrude along LOCAL UP = vertical round columns. (The old 128³-G field was
    // worley2D constant along the noise z-axis, which maps to ECEF-Z / the polar axis -> ~43° tilt at
    // lat 47 = the tilted-PLATE artifact. 512² also gives ~57 texel/cell vs ~14 -> round, not angular.)
    let uvC = vec2f(dot(posKm, C.p6.xyz), dot(posKm, C.p7.xyz)) / C.p6.w;
    let cellF = textureSampleLevel(cellTex, nsamp, uvC, 0.0).r;   // [0,1] round hills, tileable
    let hDeck = clamp(h, 0.0, 1.0);
    // Per-column cloud-top height (deck fraction): taller where the cell field is stronger. Cross-section
    // shrinks with height (fewer cells reach up) AND the top ~45% tapers as a smooth DOMED cap (not a flat
    // linear cutoff = the boxy deckel). Wispy base at the deck floor. p7.w = height scale (FB_CELL_DOME).
    let colTop = (cellF - thresh) / max(C.p7.w, 0.01);
    if (colTop <= 0.0) { return 0.0; }     /* between cells -> clear sky */
    let cap = smoothstep(colTop, colTop * 0.55 - 0.05, hDeck);   /* rounded dome over the top ~45% */
    /* RAGGED base: a finer cell octave varies the base height per column, so the underside seen from
     * below is a bumpy surface, not a mirror-flat slab (the overhead-plate artifact). */
    let baseRough = textureSampleLevel(cellTex, nsamp, uvC * 3.3 + vec2f(1.7, 4.1), 0.0).r;
    let baseH = 0.04 + 0.20 * baseRough;
    d = cap * smoothstep(baseH, baseH + 0.10, hDeck) * mix(0.7, 1.0, b);
    if (d <= 0.001) { return 0.0; }
    hNorm = clamp((hDeck - baseH) / max(colTop - baseH, 0.05), 0.0, 1.0);
  } else {
    /* (A) DEPLOYED tower/threshold silhouette: coverage-remap the base, height-profile a per-column tower
     * (towerTop from the large-scale mass), wispy base + soft top auslauf. */
    if (b <= thresh) { return 0.0; }       /* below coverage -> clear sky */
    let cov = (b - thresh) / (1.0 - thresh);
    let topH = towerTop((b1 - thresh) / (1.0 - thresh), C.p1.x, C.p1.z);
    hNorm = h / max(topH, 0.05);
    if (hNorm >= 1.0) { return 0.0; }      /* above this column's tower top -> sky */
    let wispBase = smoothstep(0.0, 0.12, hNorm);   /* wispy bottom */
    let topFall = smoothstep(1.0, 0.72, hNorm);    /* soft auslauf in the upper quarter of the tower */
    d = cov * wispBase * topFall;
  }
  if (d <= 0.0) { return 0.0; }
  /* Top-biased detail erosion in hNorm (Nubis height dep): cauliflower heads high, smooth base low. */
  if (detLod > 0.01) {
    let d2f = C.p5.x; let d2w = C.p5.y;   /* fine-detail freq + weight (swept for cauliflower softness) */
    let lodD1 = max(0.0, log2(max(footKm, 1.0e-5) * 32.0 / 0.9));
    let lodD2 = max(0.0, log2(max(footKm, 1.0e-5) * 32.0 / d2f));
    let d1 = textureSampleLevel(detTex, nsamp, posKm / 0.9, lodD1);
    let d2 = textureSampleLevel(detTex, nsamp, posKm / d2f, lodD2);
    let dfbm = (d1.r * 0.55 + d1.g * 0.25 + d1.b * 0.2) * (1.0 - d2w) + (d2.r * 0.6 + d2.g * 0.4) * d2w;
    /* Cells (B): erode the BASE too (0.16..0.28) so the underside seen from below is ragged, not a hard
     * flat plate. Stand-A (A) keeps its clean top-biased base (0.02..0.26) unchanged. */
    let eBase = select(0.02, 0.16, CELLS_ON > 0.5);
    d = d - dfbm * mix(eBase, 0.28, hNorm) * max(C.p4.z, 0.01) * detLod;
  }
  return clamp(d, 0.0, 1.0);   /* dens [0,1] — powder/erosion see a real gradient */
}
fn hgPhase(c : f32, g : f32) -> f32 {
  let g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * c, 1.5));
}
fn lightOD(posMm : vec3f, ldir : vec3f, jitter : f32) -> f32 {   /* optical depth toward a light: Schneider 6-cone (03 §6) */
  /* Two axes perpendicular to the light to spread the cone taps laterally — a straight 6-tap ray casts
   * hard banded self-shadows at grazing low light (finding 3); the cone spread smooths the banding. */
  var t0 = vec3f(0.0, 1.0, 0.0);
  if (abs(ldir.y) > 0.95) { t0 = vec3f(1.0, 0.0, 0.0); }
  let cx = normalize(cross(ldir, t0));
  let cy = cross(ldir, cx);
  var od = 0.0;
  var tt = 0.0;
  var stepMm = 0.00010;   /* 100 m first step, doubling */
  for (var i = 0; i < 5; i++) {   /* 5 near cone taps */
    let mid = tt + stepMm * 0.5;
    let ang = (f32(i) + jitter) * 2.3999632;   /* golden-angle rotation + per-pixel/-frame jitter -> temporal averages residual */
    let off = (cx * cos(ang) + cy * sin(ang)) * (mid * max(C.p5.w, 0.001));   /* cone half-angle ~17deg: radius grows with distance */
    let sp = posMm + ldir * mid + off;
    let h = clamp((length(sp) - C.p0.x) / (C.p0.y - C.p0.x), 0.0, 1.0);
    od += density(sp, h, 0.5, 0.03) * stepMm * 1.0e6;   /* light ray: short, keep fine mips */
    tt += stepMm;
    stepMm *= 2.0;
  }
  /* 1 distant shadow tap (Schneider): captures shadows cast by far clouds; base-only (cheap), half weight */
  let fp = posMm + ldir * (tt + 0.020);
  let fh = clamp((length(fp) - C.p0.x) / (C.p0.y - C.p0.x), 0.0, 1.0);
  od += density(fp, fh, 0.0, 0.1) * 0.020 * 1.0e6 * 0.5;
  return od;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  if (A.skyExtra.y < 0.5 || C.p0.z < 0.01) { return vec4f(0.0); }   /* SVS or clear -> nothing */
  let ndcJ = in.ndc + vec2f(C.p3.w, C.p4.w);   /* per-frame TAA screen jitter -> full-res reconstruction */
  let dir = normalize(A.camFwd.xyz + ndcJ.x * A.params.x * A.params.y * A.camRight.xyz
                                   + ndcJ.y * A.params.x * A.camUp.xyz);
  let cam = A.camPosMm.xyz;
  let rBase = C.p0.x;   /* absolute Mm radii — referenced to the REAL ground under the camera */
  let rTop = C.p0.y;
  let b = dot(cam, dir);
  let R2 = dot(cam, cam);
  let discTop = b * b - R2 + rTop * rTop;
  if (discTop <= 0.0) { return vec4f(0.0); }
  let sT = sqrt(discTop);
  var tStart = max(-b - sT, 0.0);
  var tEnd = -b + sT;
  if (tEnd <= 0.0) { return vec4f(0.0); }
  let discBase = b * b - R2 + rBase * rBase;
  if (discBase > 0.0) {
    let sB = sqrt(discBase);
    if (R2 < rBase * rBase) { tStart = max(tStart, -b + sB); }         /* below deck: start past the base */
    else if (R2 > rTop * rTop) { tEnd = min(tEnd, -b - sB); }          /* above deck (space): stop at base */
    else { if (-b - sB > 0.0) { tEnd = min(tEnd, -b - sB); } tStart = 0.0; }   /* inside the deck */
  }
  /* Terrain occlusion: reversed-Z infinite proj, depth = zn / (t * dot(dir,fwd)) -> t_hit. The march
   * is QUARTER-RES, so take the NEAREST full-res depth in this low-res pixel's 4x4 footprint
   * (conservative — a cloud never bleeds over a terrain silhouette). */
  let dpx = vec2i(i32(in.pos.x * C.p3.x), i32(in.pos.y * C.p3.y));
  var maxDepth = 0.0;
  for (var dy = 0; dy < 4; dy++) { for (var dx = 0; dx < 4; dx++) {
    maxDepth = max(maxDepth, textureLoad(depthTex, dpx + vec2i(dx, dy), 0));
  }}
  if (maxDepth > 1.0e-9) {   /* nearest terrain -> largest reversed-Z depth */
    let tTer = (0.05 / (maxDepth * max(dot(dir, A.camFwd.xyz), 1.0e-3))) / 1.0e6;
    tEnd = min(tEnd, tTer);
  }
  tEnd = min(tEnd, tStart + 0.24);   /* cap the march span at 240 km */
  if (tEnd <= tStart) { return vec4f(0.0); }

  let nSteps = max(24, i32(160.0 * clamp(C.p0.w, 0.05, 1.0)));
  let fine = min((tEnd - tStart) / f32(nSteps), 0.00012);   /* fine step, capped ~120 m: grazing/near-over-deck
                                                               rays step finely -> no big tangent slabs (Gap #3) */
  let coarse = fine * 3.0;                              /* empty-space skip step when out of cloud */
  let jit = fract(52.9829189 * fract(dot(in.pos.xy, vec2f(0.06711056, 0.00583715)) + C.p1.w));
  var t = tStart + fine * jit;

  let sun = A.sunDir.xyz;
  let up = normalize(cam);
  let cosT = dot(dir, sun);
  let phase = mix(hgPhase(cosT, 0.8), hgPhase(cosT, -0.5), 0.5);   /* dual-lobe HG: Hillaire 2016 measured fit */
  /* Sun colour reaching the deck: atmosphere transmittance toward the sun -> reddens near sunset. The
   * cloud march is in real WGS84 radii (~6.37 Mm); the Hillaire tLUT uses groundRadiusMM (6.360). REBASE
   * the camera to the LUT frame at its TRUE altitude, else a low sun lands the lookup below the horizon. */
  let camAltMm = length(cam) - C.p3.z;
  let camHill = up * (groundRadiusMM + max(camAltMm, 0.0));
  let sunCol = textureSampleLevel(tLUT, lsamp, tLUTuv(camHill, sun), 0.0).rgb;
  /* Ambient hemisphere: cool zenith sky above (brighter at cloud tops) + a warm bounce from the lit
   * horizon toward the sun (fills the shaded undersides with sunset colour). */
  let skyAmb = skyViewSample(svLUT, lsamp, A, up);
  let horizonCol = skyViewSample(svLUT, lsamp, A, normalize(sun - up * dot(sun, up)));

  var transm = 1.0;
  var scat = vec3f(0.0);
  var skipping = true;   /* two-tier: coarse-skip empty space, step BACK on entry, then fine-march the cloud */
  for (var i = 0; i < 384; i++) {   /* break on span end or transmittance saturation */
    if (t >= tEnd || transm < 0.02) { break; }
    let pos = cam + dir * t;
    let h = clamp((length(pos) - C.p0.x) / (C.p0.y - C.p0.x), 0.0, 1.0);
    let detLod = clamp(1.0 - (t * 1000.0 - 8.0) / 22.0, 0.0, 1.0);   /* fade detail 8->30 km (undersampled far) */
    let footKm = t * 1000.0 * (2.0 * A.params.x / 180.0);   /* low-res pixel world footprint (km) at this distance */
    let dens = density(pos, h, detLod, footKm);
    if (dens > 0.002) {
      if (skipping) { t = max(tStart, t - coarse); skipping = false; continue; }   /* back up to the boundary, re-march fine */
      let stepM = fine * 1.0e6;
      let sigK = C.p4.x * C.p2.w;   /* thickness lives in the extinction, NOT in dens */
      /* Sharpen the OPACITY on the POST-EROSION field: pow(d_eroded, gamma) puts the transmittance
       * transition on DETAIL wavelength (~100 m) with an IRREGULAR detail-shaped isosurface -> a crisp
       * optical surface (no along-ray smear) WITHOUT the smooth low-freq level-sets that caused corduroy. */
      /* Horizon distance-fade (B/high-cloud only — Stand-A keeps its full-range deck unchanged): clouds
       * beyond ~60 km dissolve into the haze by ~130 km (t = ray distance in Mm). Hides the quarter-res
       * mush of the far scattered field; matches reality (distant cumulus vanish in aerial perspective). */
      let distFade = select(1.0, 1.0 - smoothstep(0.060, 0.130, t), CELLS_ON > 0.5);
      let sigma = pow(clamp(dens, 0.0, 1.0), 2.5) * sigK * distFade;
      let od = lightOD(pos, sun, jit);
      /* Wrenninge multi-scatter octaves: attenuation a, phase-sharpness b, scatter c. */
      var ms = 0.0;
      var a = 1.0; var bo = 1.0; var co = 1.0;
      for (var o = 0; o < 3; o++) {
        let msPhase = mix(0.5 / (4.0 * PI), phase, bo);
        ms += co * msPhase * exp(-od * C.p4.x * C.p2.w * a);
        a *= 0.5; bo *= 0.5; co *= 0.55;
      }
      let powder = 1.0 - exp(-dens * 6.0);   /* dark-edge powder term */
      let sunLit = ms * sunCol * C.p4.y * (0.25 + 0.75 * powder);
      /* MOONLIGHT: the moon as a second source when it is UP — real moonlit clouds read faintly
       * silver-grey and occlude the stars. ~1/400 of the sun at full moon, phase-weighted (A.moonDir.w),
       * softened as the moon nears the horizon. Same cone-OD + multi-scatter chain, silver tint. */
      var moonLit = vec3f(0.0);
      let moonDir = A.moonDir.xyz;
      let moonUp = dot(moonDir, up);
      if (MOONLIGHT > 0.0 && moonUp > 0.02 && A.moonDir.w > 0.02) {
        let cosM = dot(dir, moonDir);
        let phaseM = mix(hgPhase(cosM, 0.8), hgPhase(cosM, -0.5), 0.5);
        let odM = lightOD(pos, moonDir, jit);
        var msM = 0.0; var am = 1.0; var bm = 1.0; var cm = 1.0;
        for (var o = 0; o < 3; o++) {
          let mp = mix(0.5 / (4.0 * PI), phaseM, bm);
          msM += cm * mp * exp(-odM * C.p4.x * C.p2.w * am);
          am *= 0.5; bm *= 0.5; cm *= 0.55;
        }
        let moonI = C.p4.y * A.moonDir.w * 0.10 * MOONLIGHT;   /* render-tuned: physically ~sun/400, but the
           night exposure is boosted (stars/lights visible), so ~sun/10 effective is where moonlit clouds
           actually READ as faint silver-grey. FB_MOONLIGHT tunes around 1.0; 0 = sun-only (near-black). */
        moonLit = msM * vec3f(0.72, 0.80, 1.0) * moonI * (0.25 + 0.75 * powder)
                  * smoothstep(0.0, 0.15, moonUp) * (1.0 - A.skyExtra.x);   /* night-only: fades out by day */
      }
      /* Nubis3 slide 32: ambient penetrates the surface, not the core -> fade by pow(1-profile, 0.5). But
       * with a FLOOR (0.30): a real day-cumulus base seen from below is mid-grey, not black — sky + ground
       * bounce reach even the dense underside. Without the floor the dense (sun-shadowed) base went near-
       * black = the "flatly dark from below" look. Slightly more sky fill at the base too (0.5 -> 0.62). */
      let ambGrad = 0.30 + 0.70 * pow(clamp(1.0 - dens, 0.0, 1.0), 0.5);
      let amb = (skyAmb * mix(0.62, 0.85, h) + horizonCol * 0.55 * (1.0 - h)) * 0.9 * ambGrad;
      let srcLum = sunLit + moonLit + amb;
      let tr = exp(-sigma * stepM);
      scat += transm * srcLum * (1.0 - tr);   /* energy-conserving in-scatter integration */
      transm *= tr;
      t += fine;
    } else {
      if (skipping) { t += coarse; } else { t += fine; }   /* coarse only while searching; fine once engaged (no re-overshoot) */
    }
  }
  return vec4f(max(scat, vec3f(0.0)), clamp(1.0 - transm, 0.0, 1.0));   /* premultiplied; guard NaN/neg */
}
)";

void FBCloudMarchStage::Configure(const FBGpu &gpu, wgpu::Buffer atmoBuf, wgpu::Sampler lutSamp,
                                   wgpu::TextureView skyLUTView, wgpu::TextureView transLUTView,
                                   wgpu::TextureView depthView, wgpu::TextureView baseView,
                                   wgpu::TextureView detailView, wgpu::TextureView cellView,
                                   bool hasTimestamp) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  HasTimestamp = hasTimestamp;

  /* Quarter-res march target: the whole cloud pass runs at Width/4 x Height/4 and is upsampled in the
   * tonemap composite. 16x fewer marched pixels — the core of the perf budget. */
  CloudW = gpu.Width / 4;
  CloudH = gpu.Height / 4;
  {
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)CloudW, (uint32_t)CloudH, 1};
    td.format = wgpu::TextureFormat::RGBA16Float;
    td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    CloudLowTex = Device.CreateTexture(&td);
  }

  wgpu::SamplerDescriptor ns{};
  ns.addressModeU = wgpu::AddressMode::Repeat;
  ns.addressModeV = wgpu::AddressMode::Repeat;
  ns.addressModeW = wgpu::AddressMode::Repeat;
  ns.magFilter = wgpu::FilterMode::Linear;
  ns.minFilter = wgpu::FilterMode::Linear;
  CloudSamp = Device.CreateSampler(&ns);

  wgpu::BufferDescriptor bd{};
  bd.size = 8 * 4 * sizeof(float);   /* p0..p7 (added p6/p7 = the cell-field tangent basis) */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  CloudUni = Device.CreateBuffer(&bd);

  /* Raymarch render pipeline: writes premultiplied cloud (rgb=scatter, a=1-transm) into the quarter-res
   * target; the tonemap composites it over the HDR scene. No blend (the target holds only the cloud).
   * kCloudNoiseCommon supplies remap() (the worley/perlin helpers go unused here — harmless). */
  std::string marchSrc = std::string(kAtmoCommon) + kAtmoSample + kCloudNoiseCommon + kCloudWGSL;
  if (CloudCellsOn()) {   /* flip the baked shape switch on (paired with the base-gen cellF above) */
    const std::string from = "const CELLS_ON : f32 = 0.0;";
    auto p = marchSrc.find(from);
    if (p != std::string::npos) marchSrc.replace(p, from.size(), "const CELLS_ON : f32 = 1.0;");
  }
  if (const char *ml = getenv("FB_MOONLIGHT")) {   /* moonlight strength (default 1.0) */
    const std::string from = "const MOONLIGHT : f32 = 1.0;";
    auto p = marchSrc.find(from);
    if (p != std::string::npos) marchSrc.replace(p, from.size(), "const MOONLIGHT : f32 = " + std::string(ml) + ";");
  }
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = marchSrc.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);
  wgpu::ColorTargetState ct{};
  ct.format = wgpu::TextureFormat::RGBA16Float;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  CloudPipe = Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[10] = {};
  be[0].binding = 0; be[0].buffer = atmoBuf; be[0].size = 11 * 4 * sizeof(float);
  be[1].binding = 1; be[1].sampler = lutSamp;
  be[2].binding = 2; be[2].textureView = skyLUTView;
  be[3].binding = 3; be[3].textureView = transLUTView;
  be[4].binding = 4; be[4].sampler = CloudSamp;
  be[5].binding = 5; be[5].textureView = baseView;
  be[6].binding = 6; be[6].textureView = detailView;
  be[7].binding = 7; be[7].textureView = depthView;
  be[8].binding = 8; be[8].buffer = CloudUni; be[8].size = 8 * 4 * sizeof(float);
  be[9].binding = 9; be[9].textureView = cellView;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = CloudPipe.GetBindGroupLayout(0);
  bg.entryCount = 10;
  bg.entries = be;
  CloudBind = Device.CreateBindGroup(&bg);

  if (HasTimestamp) {   /* 2 timestamps bracket the cloud march; resolved into a readback buffer */
    wgpu::QuerySetDescriptor qd{};
    qd.type = wgpu::QueryType::Timestamp;
    qd.count = 2;
    TsQuery = Device.CreateQuerySet(&qd);
    wgpu::BufferDescriptor tbd{};
    tbd.size = 2 * sizeof(uint64_t);
    tbd.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
    TsResolveBuf = Device.CreateBuffer(&tbd);
    tbd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    TsReadBuf = Device.CreateBuffer(&tbd);
  }
}

void FBCloudMarchStage::Update(const FBFrameContext &ctx) {
  /* Cloud deck from the weather (HudState): a main deck whose base/thickness follow cloud_base + the
   * low/mid/high mix; total coverage drives the fill. EVS-only (the pass self-gates on skyExtra.y). */
  float cover = ctx.CloudCover;
  if (ctx.CloudLow > cover) cover = ctx.CloudLow;
  if (ctx.CloudMid > cover) cover = ctx.CloudMid;
  if (ctx.CloudHigh > cover) cover = ctx.CloudHigh;
  if (cover <= 0.0f) cover = ctx.GroundPhoto ? 0.4f : 0.0f;   /* a pleasant default deck when no weather */
  /* DEFAULT WEATHER = a HIGH broken cell layer (base 8 km): the ~2 km loiter always sees it in the
   * mid/far field (accepted 2026-07-23). open-meteo cloud_base overrides once wired; FB_CLOUD_BASE_M
   * sweeps it. Paired with cells-on + FB_CELL_KM 40 / FB_CELL_DOME 0.5 + moonlight (the shipped look). */
  double baseAGL = ctx.CloudBaseAGL > 0.0f ? ctx.CloudBaseAGL : 8000.0;
  if (const char *cb = getenv("FB_CLOUD_BASE_M")) baseAGL = atof(cb);
  double thick = 2600.0 + 1400.0 * ctx.CloudHigh;
  if (const char *ct = getenv("FB_CLOUD_THICK_M")) thick = atof(ct);
  double topAGL = baseAGL + thick;
  /* Tunable material params (default; the cloud lab / research numbers override via SetCloudLab). */
  float density = 18.0f, extinct = 0.06f, sunI = 18.0f, detail = 1.3f;
  if (CloudLab) { cover = LabCover; density = LabDensity; extinct = LabExtinct; sunI = LabSunI; detail = LabDetail; }
  /* Shell radii are ABSOLUTE, referenced to the REAL ground under the camera (WGS84, not the Hillaire
   * simplified 6360 km): ground radius = |eye| - camera MSL altitude; base/top add the AGL heights. */
  double eyeLen = std::sqrt(ctx.Eye[0] * ctx.Eye[0] + ctx.Eye[1] * ctx.Eye[1] + ctx.Eye[2] * ctx.Eye[2]);
  double groundR = eyeLen - (double)ctx.AltM;
  float p[32];
  p[0] = (float)((groundR + baseAGL) / 1.0e6);   /* rBase, absolute Mm */
  p[1] = (float)((groundR + topAGL) / 1.0e6);     /* rTop */
  p[2] = cover;
  p[3] = (float)CloudQuality;
  p[4] = ctx.CloudLow; p[5] = ctx.CloudMid; p[6] = ctx.CloudHigh;
  p[7] = (float)std::fmod((double)ctx.FrameNo * 0.6180339887, 1.0);   /* per-frame along-ray dither (anti-banding + live AA) */
  double nowSec = ctx.SkyClock > 0 ? ctx.SkyClock : (double)ctx.FrameNo / 60.0;
  double w = nowSec * 8.0;   /* slow wind drift (km) */
  p[8] = (float)(w * 0.001); p[9] = 0.0f; p[10] = (float)(w * 0.0006);
  p[11] = density;   /* density scale -> optically thick, solid cumulus */
  p[12] = CloudW > 0 ? (float)ctx.Width / (float)CloudW : 4.0f;    /* low-res -> full-res depth map */
  p[13] = CloudH > 0 ? (float)ctx.Height / (float)CloudH : 4.0f;
  p[14] = (float)(groundR / 1.0e6);   /* real ground radius Mm — rebase cloud pos into the Hillaire LUT frame */
  /* Per-frame screen jitter (NDC): EXACT 4x4 sub-grid so each full-res sub-pixel gets a direct sample
   * once per 16 frames -> the resolve splat reconstructs full resolution from the quarter-res march. */
  { uint32_t ph = ctx.FrameNo % 16u;
    float jx = ((float)(ph % 4u) + 0.5f) / 4.0f - 0.5f, jy = ((float)(ph / 4u) + 0.5f) / 4.0f - 0.5f;
    p[15] = CloudW > 0 ? jx * 2.0f / (float)CloudW : 0.0f;
    p[16] = extinct; p[17] = sunI; p[18] = detail;
    p[19] = CloudH > 0 ? jy * 2.0f / (float)CloudH : 0.0f; }
  /* p5: fine-detail freq + weight (default 0.28 km / 0.35; env-overridable for the cauliflower sweep). */
  { const char *ef = getenv("FB_D2_FREQ"), *ew = getenv("FB_D2_WEIGHT");
    p[20] = ef ? (float)atof(ef) : 0.45f; p[21] = ew ? (float)atof(ew) : 0.20f;
    const char *bb = getenv("FB_BASE_LOD_BIAS"); p[22] = bb ? (float)atof(bb) : 0.0f; const char *cr = getenv("FB_CONE_R"); p[23] = cr ? (float)atof(cr) : 0.30f; }   /* swept: soft cauliflower */
  /* p6/p7: horizontal tangent basis (from the camera up) + cell span (km/tile) — the 512² F1 cell field
   * is sampled by (dot(pos,t1), dot(pos,t2))/span so cells extrude along LOCAL UP = vertical round puffs. */
  double t1[3] = {ctx.Up[1], -ctx.Up[0], 0.0};
  double l1 = std::sqrt(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
  if (l1 < 1e-6) { t1[0] = 1.0; t1[1] = 0.0; t1[2] = 0.0; l1 = 1.0; }
  t1[0] /= l1; t1[1] /= l1; t1[2] /= l1;
  double t2[3] = {ctx.Up[1] * t1[2] - ctx.Up[2] * t1[1], ctx.Up[2] * t1[0] - ctx.Up[0] * t1[2], ctx.Up[0] * t1[1] - ctx.Up[1] * t1[0]};
  double cellKm = 40.0;   /* accepted cell size (~4 km cells); FB_CELL_KM sweeps it */
  if (const char *ck = getenv("FB_CELL_KM")) cellKm = atof(ck);
  double domeSub = 0.5;   /* cell height-subtraction: lower = taller/more substantial puffs (FB_CELL_DOME) */
  if (const char *cd = getenv("FB_CELL_DOME")) domeSub = atof(cd);
  p[24] = (float)t1[0]; p[25] = (float)t1[1]; p[26] = (float)t1[2]; p[27] = (float)cellKm;
  p[28] = (float)t2[0]; p[29] = (float)t2[1]; p[30] = (float)t2[2]; p[31] = (float)domeSub;
  Queue.WriteBuffer(CloudUni, 0, p, sizeof p);
  CloudMidR = (double)(p[0] + p[1]) * 0.5;   /* mid-shell radius Mm (temporal reprojection depth) */
}

void FBCloudMarchStage::Encode(const FBFrameContext &, wgpu::RenderPassEncoder &pass) {
  if (CloudQuality > 0.0) {
    pass.SetPipeline(CloudPipe);
    pass.SetBindGroup(0, CloudBind);
    pass.Draw(3);
  }
}

void FBCloudMarchStage::ResolveTimestamps(wgpu::CommandEncoder &enc) {
  if (!HasTimestamp) return;
  enc.ResolveQuerySet(TsQuery, 0, 2, TsResolveBuf, 0);
  if (!TsMapPending) enc.CopyBufferToBuffer(TsResolveBuf, 0, TsReadBuf, 0, 2 * sizeof(uint64_t));
}

void FBCloudMarchStage::PollTimestamps(void) {
  if (!HasTimestamp || TsMapPending) return;
  TsMapPending = true;
  TsReadBuf.MapAsync(wgpu::MapMode::Read, 0, 2 * sizeof(uint64_t), wgpu::CallbackMode::AllowSpontaneous,
      [this](wgpu::MapAsyncStatus st, wgpu::StringView) {
        if (st == wgpu::MapAsyncStatus::Success) {
          const uint64_t *ts = static_cast<const uint64_t *>(TsReadBuf.GetConstMappedRange(0, 2 * sizeof(uint64_t)));
          if (ts && ts[1] > ts[0]) { TsAccumMs += (double)(ts[1] - ts[0]) / 1.0e6; TsCount++; }
          TsReadBuf.Unmap();
          if (TsCount >= 120) {
            FBLog::Debug("render", "cloud_march_perf", {{"msPerFrame", TsAccumMs / TsCount}, {"frames", TsCount}});
            TsAccumMs = 0.0; TsCount = 0;
          }
        }
        TsMapPending = false;
      });
}

} // namespace FlightBox
