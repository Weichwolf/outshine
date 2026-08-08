#include "TilesStage.h"
#include "SceneTargets.h"
#include "Frustum.h"
#include "AtmoCommon.h"
#include "AtmoSample.h"
#include "AtmoHaze.h"
#include "SceneScale.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this stage. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "ShadowSample.h"
#include "SurfaceLight.h"
#include "CloudDensityWGSL.h"
#include "Sward.h"
#include "Graticule.h"
#include "Log.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace outshine::Render {

/* The terrain draw. Per-draw data, RenderBundle signature and the
 * invariant counters: doc/render/renderer.md, Abschnitt 6. */
static const char *kTerrainWGSL = R"(
/* haze:  x = sigma0 (1/m, Koschmieder of the reported visibility), y = camera altitude ASL (m),
 *        z = the WGS84 ground radius under the camera (Mm), w = one pixel as an angle (rad).
 * The DECKS are not here: they are the shared cloud field every lit surface binds (CloudShadow.h). */
struct U { mvp : mat4x4f, sun : vec4f, haze : vec4f,
           /* THE STAND, so that the ground fragment can be it: the frame's ENU basis at the camera,
            * the graticule cell in metres and the eye's own cell. Nothing here depends on time —
            * nothing below the size of a tree moves (doc/goal.md). */
           sax : vec4f,   // ECEF east  axis, w = the graticule cell east (m)
           say : vec4f,   // ECEF north axis, w = the graticule cell north (m)
           saz : vec4f,   // ECEF up at the camera, w = sin(sun elevation)
           sgr : vec4f,   // x,y = the eye's offset inside its own cell (m)
                          // z = the declared bare-rock veg row, w = the slope band in degrees
           sbs : vec4i,   // xy = the eye's own cell on the world graticule
           /* THE CLASS FRAME: the ECEF axes of the class structure's origin, and the camera's own
            * position in it. A fragment adds its camera-relative offset and is on the lattice the
            * CPU rasterised, exactly. */
           cax : vec4f,   // ECEF east  axis of the class origin, w = camera east  (m)
           cay : vec4f }; // ECEF north axis of the class origin, w = camera north (m)
@group(0) @binding(0) var<uniform> u : U;
// per-draw storage entry: a.xyz = camera-relative ECEF offset (origin_ecef - cam_ecef, float),
// a.w = CLASS array layer; b = spare;
// c.xyz = origin_ecef - anchor_ecef, the WORLD-FIXED frame the procedural surface is evaluated in.
// The draw selects entry i via firstInstance, so instance_index == draw index.
struct Tile { a : vec4f, b : vec4f, c : vec4f };
@group(0) @binding(1) var<storage, read> tiles : array<Tile>;
/* The LUT sampler: the sky-view LUT wraps in azimuth (AddressMode::Repeat) and its
 * seam sits at u = 0, which is the SUN's own azimuth — sampling it clamped puts a filtered seam right
 * across the brightest part of the far field. */
@group(0) @binding(4) var lsamp : sampler;
@group(0) @binding(5) var svLUT : texture_2d<f32>;
@group(0) @binding(6) var<uniform> A : Atmo;
@group(0) @binding(9) var<storage, read> I : Irr;
@group(0) @binding(10) var<uniform> C : Csm;
@group(0) @binding(11) var shMap : texture_depth_2d;
@group(0) @binding(12) var shSamp : sampler_comparison;      /* IrradianceStage: the ONE scale */
struct VOut { @builtin(position) pos : vec4f, @location(0) nrm : vec3f,
              @location(1) @interpolate(flat) layer : u32, @location(2) wpos : vec3f,
              @location(3) gpos : vec3f };
@vertex fn vs(@builtin(instance_index) inst : u32,
              @location(0) p : vec3f, @location(1) uv : vec2f, @location(2) n : vec3f) -> VOut {
  var o : VOut;
  let t = tiles[inst];
  let rel = p + t.a.xyz;                    // camera-relative ECEF (metres); a.xyz = origin - cam
  o.pos = u.mvp * vec4f(rel, 1.0);
  o.nrm = n;
  o.layer = u32(t.a.w);
  o.wpos = rel;
  o.gpos = p + t.c.xyz;                     // anchored ECEF: the same point gives the same value for ever
  return o;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let nrmN = normalize(in.nrm);
  // NO TEXTURE, SO NO FILTER: the ground is a function of position, and mip choice, zoom steps,
  // sampling lattices and filter artefacts cannot occur in one (CLAUDE.md principle 2). The mip-bias
  // ramp that fought the grazing-anisotropy streaks went with the raster it was correcting.
  let vdir = normalize(in.wpos);
  let upR = normalize(A.camPosMm.xyz);
  let distM = length(in.wpos);
  // THE RASTER COLOUR IS AN INDEX. What is drawn is the MATERIAL the index selects: linear
  // reflectance, roughness and a procedural surface, all of them world-fixed
  // (doc/render/stages/ground-cover.md 4.1, doc/render/classification.md 0).
  let footM = distM * u.haze.w;                 // one pixel on the ground, metres
  let m = groundMat(in.wpos, in.gpos, nrmN, footM);
  // FB_GROUND_CLASS_VIZ=1: the RESOLVED class index as a colour, before light, haze and AO, so that
  // "is the class at this ground point the same from two camera positions" is a pixel comparison and
  // not an inference. Base-3 digits per channel: 27 separable indices for a 16-class table.
  if (kClsViz > 0.5) {
    let ka = f32(m.cls % 3u); let kb = f32((m.cls / 3u) % 3u); let kc = f32((m.cls / 9u) % 3u);
    return vec4f(0.05 + ka * 0.15, 0.05 + kb * 0.15, 0.05 + kc * 0.15, 1.0);
  }
  let base = m.alb;
  // The fragment's OWN altitude: which decks stand between it and the sun is a property of where it
  // is, and in the Alps a ridge really does poke through a 2 991 m base while the valley does not.
  let fragAltM = (length(A.camPosMm.xyz + in.wpos * 1.0e-6) - u.haze.z) * 1.0e6;
  let sunN = normalize(u.sun.xyz);
  let upN = normalize(A.camPosMm.xyz + in.wpos * 1.0e-6);
  // WEATHER LIGHT as a deck-mean and not a place: the cast cloud shadow is out until the cloud
  // itself is worth casting one (stages/CloudShadow.h).
  let sunThru = 1.0;
  let meanThru = 1.0;
  let sunVis = csmSunVis(shMap, shSamp, C, in.wpos, nrmN, sunN);
  let night = kNightAmbient * (1.0 - A.skyExtra.x);
  // ROUGHNESS IS THE SEPARATOR, not the albedo: asphalt 0.120 and forest floor 0.148 are 0.30 EV
  // apart, so what has to tell them apart is the specular lobe and the relief. Shading uses the
  // PERTURBED normal, the shadow lookup the geometric one — a per-grain normal must not self-shadow.
  let viewTo = -vdir;
  // m.spec is the fraction of this class that presents a coherent dielectric interface at all
  // (ground-materials.json specularModel): a heap of grains has none, a sealed skin or a water film
  // has one. It scales BOTH sides of the same Fresnel split, so at 0 nothing is taken out of the
  // diffuse and the material is fully Lambertian instead of merely dark.
  let fEnv = envBrdf(max(dot(m.nrm, viewTo), 0.0), m.rough) * m.spec * kSpecOn;
  let hSun = normalize(sunN + viewTo);
  let fDir = (0.04 + 0.96 * pow(1.0 - max(dot(viewTo, hSun), 0.0), 5.0)) * m.spec * kSpecOn;
  let lit = litRadiance(I, base * (1.0 - fEnv), 1.0 - fDir,
                        m.nrm, upN, sunN, sunVis, sunThru, meanThru, night);
  // THE ENVIRONMENT SPECULAR IS A DIRECTION, and E_sky/PI is not it: that is the radiance a mirror
  // would return if the dome were UNIFORM, i.e. one number for every pointing. A river seen at 88.8 deg
  // incidence returns the sky 1.2 deg ABOVE the horizon, which at this scene is warm and several times
  // the zenith's luminance, and the dome mean painted it with the zenith's HUE instead — measured, the
  // water carried the dome irradiance's hue to within 4-24 deg over three yaws and sat 1.02-1.37 EV
  // under the sky it mirrors.
  // The mean is still the right answer at the ROUGH end, where the lobe covers the hemisphere, so the
  // two are blended on the roughness: there is no prefiltered environment chain to interpolate between,
  // and one LUT tap plus a lerp is what a single-mip environment can honestly deliver.
  let rSpec = reflect(vdir, m.nrm);
  // A mirror ray that points below the local horizon would ask the sky LUT about the ground; the
  // horizon sample is the nearest thing this pass knows and is what the ray would graze anyway.
  let rSky = normalize(rSpec - upN * min(dot(rSpec, upN), 0.0));
  let envL = mix(skyViewSample(svLUT, lsamp, A, rSky), I.sky.xyz * (kInvPi * kSceneExposure),
                 clamp(m.rough, 0.0, 1.0));
  let specE = envL * fEnv;
  let specD = I.sun.xyz * (ggxSpec(m.nrm, sunN, viewTo, m.rough) * fDir * sunVis * sunThru
                           * kSceneExposure);
  let specL = specE + specD;
  var c = lit.rgb + specL;
  var swCover = 0.0;
  /* THE STAND, AT THE OTHER SCALE. Past the screen-space threshold a blade stops being geometry and
   * becomes this — the same declaration (render/Sward.h), the same leaf-angle population, the same
   * two transmittances, the same transmission lobe, the same wave. There is no boundary between two
   * systems here, so there is nothing to match up: what the blade shader stops drawing, this has
   * been carrying all along, under every blade, at every distance.
   *
   * The cell is the SAME graticule lattice the blades stand on, reached from the fragment's own
   * camera-relative offset — the eye's integer cell crosses in the uniform and only the local
   * remainder is ever an f32, which is what keeps the field world-fixed anywhere on Earth. */
  if (m.gr.w > 0.0 && m.dr.w > 0.0) {
    let ee = dot(in.wpos, u.sax.xyz);
    let swN = dot(in.wpos, u.say.xyz);
    let ux = (ee + u.sgr.x) / max(u.sax.w, 1.0e-4);
    let uy = (swN + u.sgr.y) / max(u.say.w, 1.0e-4);
    let sEl = max(u.saz.w, kMinSinEl);
    let tanEl = sEl / sqrt(max(1.0 - sEl * sEl, 1.0e-4));
    let sbE = dot(sunN, u.sax.xyz);
    let sbN = dot(sunN, u.say.xyz);
    let sbLen = max(sqrt(sbE * sbE + sbN * sbN), 1.0e-6);
    /* The tussock rung is a PATTERN while a crown still covers pixels and its own MEAN below that —
     * the same footprint filter the relief ladder uses, applied to the field the light is integrated
     * against. Nothing is lost when it retires: an unresolved shadow is a mean darkening. */
    let tuftM = u.sax.w / kTuftCells;
    let res = clamp(tuftM / max(4.0 * footM, 1.0e-6) - 1.0, 0.0, 1.0);
    let hRef = m.dr.w;
    let hTop = hRef * swCanopyTop(u.sbs.x, u.sbs.y, ux, uy, res);
    let dens = m.gr.w * m.pr.y / kMeanSin;
    let sVw = clamp(abs(dot(vdir, upN)), kMinSinEl, 1.0);
    let kVw = swG(sVw) / sVw;
    /* The height a viewer's own optical depth of one sits at — the one place the marched shadow is
     * evaluated, so the pattern is read where the picture is. */
    let zStar = swAboveInv(min(1.0 / max(kVw * dens * hTop, 1.0e-4), 1.0), m.pr.x) * hTop;
    let slantM = swSlantPath(u.sbs.x, u.sbs.y, ux, uy, u.sax.w, u.say.w, zStar, hRef, m.pr.x,
                             sbE / sbLen, sbN / sbLen, tanEl, res);
    /* THE AGGREGATE'S NORMAL IS THE GROUND'S, and that is a decision with a measurement behind it.
     * A canopy-top surface normal is a SECOND geometric model laid over the leaf-angle population G
     * already carries: grass grows the same way whether its crown sits in a hollow or on a bump, so
     * tilting the whole BRDF by the envelope's own slope counts the same geometry twice. Measured
     * with the tilt in: the aggregate read a flat 11 display codes (0.49 EV) UNDER the geometry it
     * replaces at every distance from 4 to 20 m, because the envelope's slope at the crown lattice
     * runs past 1.0 and every tilted sample loses sky. What the envelope's shape is allowed to do is
     * change the beam's PATH, and that is what the march does.
     * The one nAgg here is therefore the ground's own normal, and the substitution below is what
     * turns litRadiance's surface term into the canopy's own. */
    let nAgg = nrmN;
    /* THE POPULATION'S MEAN DRYNESS, and it is not the declared share: a grass leaf dies from the
     * TIP down, so a blade that has begun to senesce is dry over kTipRun of its length and only
     * kWholeDry of the affected blades are dry end to end (render/Sward.h). That is a statement
     * about senescence and it outlived the blade geometry that first needed it. */
    let meanDry = m.pr.w * (kWholeDry + (1.0 - kWholeDry) * 0.5 * kTipRun);
    let swCol = mix(m.gr.rgb, m.dr.rgb, meanDry);
    let agg = swardAggregate(I, swCol, dens, hTop, m.pr.x, slantM, nAgg, upN, sunN, vdir,
                             sunVis, sunThru, meanThru, night, sEl);
    c = mix(c, agg.rgb, agg.cover);
    swCover = agg.cover;
  }
  // AERIAL PERSPECTIVE, from the weather rather than from a clear-air table: the same sigma0, the same
  // two scale heights and the same inscatter colour the cloud deck dissolves into (AtmoHaze.h). That
  // is the whole point of the shared header — deck and ground see ONE atmosphere, so a mountain and the
  // cloud above it fade at the same rate and the horizon has no edge. Per channel, so the ridge fifty
  // kilometres out loses its blue on the way here instead of going uniformly grey.
  let hz = hazeTransmittance3(u.haze.x, 0.5 * (u.haze.y + fragAltM), distM);
  let sunUpN = dot(sunN, upN);
  c = c * hz + hazeInscatter(svLUT, lsamp, A, vdir) * deckHazeFactor(I, sunUpN, meanThru)
              * (vec3f(1.0) - hz);
  // Haze is sky light: what it replaces stops being direct, so the AO weight follows the same mix.
  let hy = dot(hz, vec3f(0.2126, 0.7152, 0.0722));
  // The direct FRACTION has to account for the two specular terms as well, or screen-space occlusion
  // would darken a sun glint as if it were sky light.
  let yLit = dot(lit.rgb, vec3f(0.2126, 0.7152, 0.0722));
  let yTot = yLit + dot(specL, vec3f(0.2126, 0.7152, 0.0722));
  let yDir = lit.a * yLit + dot(specD, vec3f(0.2126, 0.7152, 0.0722));
  let dirFrac = select(lit.a, clamp(yDir / yTot, 0.0, 1.0), yTot > 1.0e-9);
  /* Where the stand covers the ground, screen-space occlusion does not apply: the sward has already
   * been occluded analytically by itself, at a scale an SSAO radius of about a metre cannot resolve.
   * That is the same statement the blade's own alpha of 1 makes. */
  return vec4f(c, mix(dirFrac * hy, 1.0, swCover));
}

/* GGX/Smith, height-correlated, WITHOUT the Fresnel factor: F belongs to the caller, which owes the
 * diffuse its complement. The one place roughness becomes an image: at the reference scene's 11.2 deg
 * sun the mirror direction lands on the ground about 8.6 m in front of a 1.70 m eye, so a sealed
 * surface carries a broad glint there and a loose one does not. */
fn ggxSpec(nv : vec3f, ldir : vec3f, vto : vec3f, rough : f32) -> f32 {
  let nl = dot(nv, ldir);
  let nvv = dot(nv, vto);
  if (nl <= 0.0 || nvv <= 0.0) { return 0.0; }
  /* alpha = roughness DIRECTLY, not roughness^2. ground-materials.json declares no BRDF, and its own
   * origins read the field perceptually — "loose fine grains scatter near-Lambertian" at 0.90, water
   * at 0.05. Under alpha = r^2 that 0.90 becomes a 0.81 lobe and asphalt's 0.65 a 0.42 one, which at
   * this scene's 11.2 deg sun puts a 6x-diffuse glare over the whole foreground (measured). Under
   * alpha = r the same geometry gives 2.2x, and 0.05 still reads as water. */
  let a = max(rough, 0.002);
  let a2 = a * a;
  let hv = normalize(ldir + vto);
  let nh = max(dot(nv, hv), 0.0);
  let dn = nh * nh * (a2 - 1.0) + 1.0;
  let dTerm = a2 / (3.14159265 * dn * dn);
  let lv = nl * sqrt(nvv * nvv * (1.0 - a2) + a2);
  let ll = nvv * sqrt(nl * nl * (1.0 - a2) + a2);
  let vis = 0.5 / max(lv + ll, 1.0e-5);
  return dTerm * vis * nl;
}

/* THE SPLIT-SUM ENVIRONMENT BRDF, F0 = 0.04 for every dielectric here. What stood here was the bare
 * Schlick-with-roughness FRESNEL factor used as if it were the whole integral, and that is an energy
 * error rather than an approximation: it has no masking term, so at grazing it returns
 * max(1 - rough, F0) of the sky dome — 0.35 for asphalt. MEASURED consequence at the reference scene:
 * the specular carried 23-39 % of the ground's display luminance across five yaws and the 10-200 m
 * band came out at hue 218 deg, i.e. BLUE ground.
 *
 * The analytic fit is Lazarov's (SIGGRAPH 2013 "Getting More Physical in Call of Duty: Black Ops II"),
 * as published by Karis for mobile UE4 — the same (A, B) a DFG LUT holds, in four MADs. It was fitted
 * with alpha = rough^2, and this shader defines alpha = rough (see ggxSpec), so it is fed sqrt(rough):
 * the fit then sees the roughness whose LOBE matches the one actually shaded. */
fn envBrdf(nvv : f32, rough : f32) -> f32 {
  let pr = sqrt(max(rough, 0.0));
  let c0 = vec4f(-1.0, -0.0275, -0.572, 0.022);
  let c1 = vec4f(1.0, 0.0425, 1.04, -0.04);
  let r = pr * c0 + c1;
  let a004 = min(r.x * r.x, exp2(-9.28 * nvv)) * r.x + r.y;
  return 0.04 * (-1.04 * a004 + r.z) + (1.04 * a004 + r.w);
}
)";


/* [SET] 12, and it is a BOUND and not a design value: the relief ladder ends at the class's grain
 * size, and the longest ladder in the table (sand, 3.0 m down to 0.65 mm at sqrt(6.7) per rung) is
 * ten rungs. A WGSL loop needs a static bound; this one is never the thing that stops it. */
static constexpr int kReliefOctaves = 12;

/* THE GROUND IS A MATERIAL (doc/render/stages/ground-cover.md §4.1). The raster texel is a CLASS
 * INDEX and nothing else; what is drawn is the class's linear reflectance, its roughness, and a
 * surface generated from its grain size and height amplitude over a ladder of octaves. The four texels around
 * the sample are blended as PARAMETERS, not as pixels — that is what leaves no seam where two
 * materials meet, and it is why the table is a table and not an atlas.
 *
 * EVERY noise query is in the tile's ANCHORED frame (Tile.c = origin − anchor, the anchor being the
 * tile's z10 ancestor), so the surface belongs to the ground and not to the viewer. Evaluating it in
 * camera-relative coordinates — which is what the colour field it replaces did — makes the whole
 * ground slide along with the walker: invisible in a still frame, unmissable in motion. */
static const char *kVegOnWGSL = R"(
struct VegRow { ground : vec4f, litter : vec4f, gsurf : vec4f, lsurf : vec4f, mixp : vec4f,
                grass : vec4f, dry : vec4f, par : vec4f, edge : vec4f };
@group(0) @binding(7) var<storage, read> vegTbl : array<VegRow>;
struct GroundMat { alb : vec3f, rough : f32, nrm : vec3f, spec : f32, cls : u32,
                   gr : vec4f, dr : vec4f, pr : vec4f };

/* [SET] 0.8, and it is BRACKETED rather than guessed: RMS relief grows as (L/grain)^H, so H = 1 makes
 * the slope scale-invariant at the grain's own aspect ratio (14 deg for dry earth — a ploughed field
 * everywhere) and H = 0.5 puts 8 mm of relief on a half-metre of soil, which renders flat. 0.8 lands
 * the half-metre patch at the few centimetres a photograph shows. It is the ONLY bridge from the
 * grain-scale amplitude the material table carries to the scale the eye resolves, and no measurement
 * of the exponent was found. */
const kReliefH : f32 = 0.8;
/* DERIVED from Idso et al. 1975: a raked plot of ~1 cm relief reads 0.04 below the smooth plot of the
 * same drying soil, whose dry albedo is ~0.31 — 0.129 of the reflectance per centimetre of relief,
 * from self-shading alone. Applied against the GRAIN amplitude, not the octave amplitude. */
const kSelfShadow : f32 = 0.129;

fn gHash2(pv : vec2f) -> f32 {
  var qv = fract(vec3f(pv.xyx) * vec3f(0.1031, 0.1030, 0.0973));
  qv = qv + vec3f(dot(qv, qv.yzx + vec3f(33.33)));
  return fract((qv.x + qv.y) * qv.z);
}
/* Value noise with its ANALYTIC gradient: x = the value, yz = d/dp in lattice units. Finite
 * differences would need three evaluations per octave and this needs one, which is the whole
 * difference between six noise lookups per ground fragment and two. */
fn gNoise2(pv : vec2f) -> vec3f {
  let iv = floor(pv);
  let fv = pv - iv;
  let wv = fv * fv * (3.0 - 2.0 * fv);
  let dw = 6.0 * fv * (1.0 - fv);
  let n00 = gHash2(iv);
  let n10 = gHash2(iv + vec2f(1.0, 0.0));
  let n01 = gHash2(iv + vec2f(0.0, 1.0));
  let n11 = gHash2(iv + vec2f(1.0, 1.0));
  let a = n10 - n00;
  let b = n01 - n00;
  let c = n00 - n10 - n01 + n11;
  return vec3f(n00 + a * wv.x + b * wv.y + c * wv.x * wv.y,
               dw.x * (a + c * wv.y),
               dw.y * (b + c * wv.x));
}

/* Amplitude of one octave from the grain: amp(L) = ampGrain * (L/grain)^H. */
fn octAmp(grain : f32, ampGrain : f32, scale : f32) -> f32 {
  return ampGrain * pow(max(scale, 1.0e-4) / max(grain, 1.0e-5), kReliefH);
}
/* An octave whose period is under four pixels carries no image, only aliasing; it fades out on its own
 * screen footprint instead of on a written-down distance. The fade takes a whole octave — nothing at
 * four pixels, everything at eight — because the amplitude law puts the STEEPEST slopes on the finest
 * rung (amp/L grows as L^(H-1), H < 1), so a rung that switches on at full weight one pixel above
 * Nyquist paints per-pixel normal noise. What the ramp takes out is not lost: it lands in the
 * Toksvig variance, which is the same relief expressed as roughness. */
fn octWeight(scale : f32, footM : f32) -> f32 {
  return clamp(scale / max(4.0 * footM, 1.0e-6) - 1.0, 0.0, 1.0);
}

/* THE RELIEF IS A LADDER, NOT A PAIR, and the pair is what made it a grid. detailScaleModel declares
 * two scales 6.7 apart; between them lay the whole decade the eye actually resolves at 1.70 m, so the
 * finer of the two stood alone in the resolved band and showed its own lattice as a TONE — measured,
 * peak-to-median power in the 6-70 px band 22 / 46 / 131 at three ground distances, i.e. one frequency
 * at up to 130x its neighbours. A ground surface has a power-law spectrum, not a note.
 *
 * BOTH ENDS AND THE STEP COME OUT OF THE MATERIAL TABLE, so no rung count is asserted. The ladder
 * starts at the declared coarse scale, steps by sqrt(6.7) — which puts the declared FINE scale on
 * rung 2 exactly, i.e. the table is continued and not reinterpreted — and stops at the GRAIN, below
 * which the material has no relief to have. kOctN is only the loop's safety bound (sand, the longest
 * ladder here, needs ten rungs from 3.0 m to 0.65 mm). octWeight retires every rung the pixel cannot
 * resolve, so a rung's noise is a near-field cost and the far field pays for arithmetic alone; its
 * unresolved slope still counts, which is what makes the Toksvig term the whole spectrum under the
 * pixel rather than one arbitrary octave of it.
 *
 * Each rung is turned against the one above it, because without that they reinforce ONE grid instead
 * of filling a spectrum. A value lattice is symmetric under 90 deg and the ladder's length is a
 * property of the class, so the turn is the golden fraction of that 90 deg: every prefix of the
 * sequence is then as evenly spread as a sequence of that length can be, whichever rung the grain
 * stops it on. */
struct Relief { n : f32, sx : f32, sy : f32, unres : f32 };
const kOctStepRad : f32 = 1.5707963268 * 0.6180339887;

fn reliefField(pu : f32, pv : f32, grain : f32, ampGrain : f32,
               sC : f32, sF : f32, footM : f32) -> Relief {
  var o = Relief(0.0, 0.0, 0.0, 0.0);
  var wsum = 0.0;
  let ratio = sqrt(max(sF, 1.0e-4) / max(sC, 1.0e-4));
  let bottom = max(grain, 1.0e-4);
  var scale = max(sC, 0.05);
  var ca = cos(0.5 * kOctStepRad);
  var sa = sin(0.5 * kOctStepRad);
  let cs = cos(kOctStepRad);
  let ss = sin(kOctStepRad);
  for (var i = 0; i < kOctN; i = i + 1) {
    if (scale < bottom) { break; }
    let amp = octAmp(grain, ampGrain, scale);
    let wv = octWeight(scale, footM);
    if (amp > 0.0) {
      if (wv > 0.002) {
        let g = gNoise2(vec2f(ca * pu + sa * pv, ca * pv - sa * pu) / scale);
        let w = amp * wv;
        /* the gradient comes back out of the turned frame, or the tilt would point the wrong way */
        o.sx = o.sx + (ca * g.y - sa * g.z) * (w / scale);
        o.sy = o.sy + (sa * g.y + ca * g.z) * (w / scale);
        o.n = o.n + g.x * w;
        wsum = wsum + w;
      }
      /* Toksvig: the slope variance of what the pixel could NOT resolve */
      let un = (1.0 - wv) * (amp / scale);
      o.unres = o.unres + un * un;
    }
    scale = scale * ratio;
    let nc = ca * cs - sa * ss;
    sa = sa * cs + ca * ss;
    ca = nc;
  }
  /* The visible modulation follows the SAME amplitude law as the relief that casts it, which is what
   * a fixed 0.62/0.38 pair could not do: the fine octave now carries the share its own amplitude
   * gives it and stops being half the image. */
  o.n = select(0.5, o.n / wsum, wsum > 1.0e-9);
  return o;
}

/* THE CLASS IS EVALUATED, NOT SAMPLED. There is no class texture and therefore no resolution that
 * could make the answer depend on where the camera stands — the failure this file used to warn about
 * is not expressible any more. The structure is world/ClassField.h verbatim:
 *   cell   -> base class (every feature that covers this cell without a boundary in it) + its seeds
 *   seed   -> one feature with a boundary here: its winding at the cell's south-west corner and the
 *             range of this cell's edges of it
 * The fragment walks corner -> (px, cy) -> p, two axis-aligned legs that cannot leave the cell, so
 * only this cell's edges can cross them. MEASURED over the reference block: 3.87 edges and 2.11
 * features per cell on average, p99 16 edges, worst cell 45.
 *
 * TWO TIERS, finer first, and beyond the coarse one there is no OSM datum at all — the declared
 * default is the honest answer there and it is COUNTED on the CPU, never guessed here. */
@group(0) @binding(13) var<storage, read> clsBuf : array<u32>;

fn clsF32(i : u32) -> f32 { return bitcast<f32>(clsBuf[i]); }

/* Sign conventions identical to ClassField.cpp's CrossX/CrossY. Only their CONSISTENCY matters —
 * the winding is tested against zero and never read as a number. */
fn clsCrossX(e : vec4f, cy : f32, xa : f32, xb : f32) -> i32 {
  if ((e.y <= cy) == (e.w <= cy)) { return 0; }
  let xi = e.x + (cy - e.y) * (e.z - e.x) / (e.w - e.y);
  if (xi < xa || xi >= xb) { return 0; }
  return select(-1, 1, e.w > e.y);
}
fn clsCrossY(e : vec4f, cx : f32, ya : f32, yb : f32) -> i32 {
  if ((e.x <= cx) == (e.z <= cx)) { return 0; }
  let yi = e.y + (cx - e.x) * (e.w - e.y) / (e.z - e.x);
  if (yi < ya || yi >= yb) { return 0; }
  return select(1, -1, e.z > e.x);
}
fn clsSegDist(px : f32, py : f32, e : vec4f) -> f32 {
  let d = vec2f(e.z - e.x, e.w - e.y);
  let l2 = dot(d, d);
  let t = clamp(select(0.0, dot(vec2f(px - e.x, py - e.y), d) / max(l2, 1.0e-12), l2 > 0.0), 0.0, 1.0);
  return length(vec2f(e.x, e.y) + d * t - vec2f(px, py));
}

struct ClsHit { best : u32, second : u32, dist : f32, have : bool };

fn clsTier(h : u32, pe : f32, pn : f32) -> ClsHit {
  var o = ClsHit(0u, 0u, 1.0e30, false);
  if (h == 0u) { return o; }
  let gw = i32(clsBuf[h + 0u]);
  let gh = i32(clsBuf[h + 1u]);
  let org = vec2f(clsF32(h + 2u), clsF32(h + 3u));
  let cm = clsF32(h + 4u);
  let gi = i32(floor((pe - org.x) / cm));
  let gj = i32(floor((pn - org.y) / cm));
  if (gi < 0 || gj < 0 || gi >= gw || gj >= gh) { return o; }
  let cellOfs = clsBuf[h + 5u];
  let seedOfs = clsBuf[h + 6u];
  let refOfs = clsBuf[h + 7u];
  let edgeOfs = clsBuf[h + 8u];
  let ci = cellOfs + u32(gj * gw + gi) * 2u;
  let c0 = clsBuf[ci];
  var bestRank : i32 = -1;
  var secondRank : i32 = -1;
  if ((c0 & 0xFFu) != 0xFFu) { o.best = c0 & 0xFFu; bestRank = i32((c0 >> 8u) & 0xFFu); o.have = true; }
  let nseed = (c0 >> 16u) & 0xFFu;
  let seedFirst = clsBuf[ci + 1u];
  let cx = org.x + f32(gi) * cm;
  let cy = org.y + f32(gj) * cm;
  for (var s = 0u; s < nseed; s = s + 1u) {
    let w0 = clsBuf[seedOfs + (seedFirst + s) * 3u];
    let refFirst = clsBuf[seedOfs + (seedFirst + s) * 3u + 1u];
    let halfW = clsF32(seedOfs + (seedFirst + s) * 3u + 2u);
    let tpl = w0 & 0xFFu;
    let rank = i32((w0 >> 8u) & 0xFFu);
    let nref = (w0 >> 16u) & 0xFFu;
    var wind = i32((w0 >> 24u) & 0xFFu) - 128;
    var d = 1.0e30;
    /* halfW > 0 marks a LINE, and a line is its centreline: the test is the distance against half the
     * declared width, which yields a coverage instead of an inside/outside. */
    for (var r = 0u; r < nref; r = r + 1u) {
      let ei = edgeOfs + clsBuf[refOfs + refFirst + r] * 4u;
      let ed = vec4f(clsF32(ei), clsF32(ei + 1u), clsF32(ei + 2u), clsF32(ei + 3u));
      if (halfW <= 0.0) { wind = wind + clsCrossX(ed, cy, cx, pe) + clsCrossY(ed, pe, cy, pn); }
      d = min(d, clsSegDist(pe, pn, ed));
    }
    if (halfW > 0.0) {
      if (d > halfW) { continue; }
      d = halfW - d;
    } else if (wind == 0) { continue; }
    if (rank > bestRank) {
      o.second = o.best; secondRank = bestRank;
      o.best = tpl; bestRank = rank; o.dist = d; o.have = true;
    } else if (rank > secondRank) { o.second = tpl; secondRank = rank; }
  }
  return o;
}

fn groundMat(wposIn : vec3f, gposIn : vec3f, nrmIn : vec3f, footM : f32) -> GroundMat {
  var o : GroundMat;
  o.nrm = nrmIn;
  let pe = u.cax.w + dot(wposIn, u.cax.xyz);
  let pn = u.cay.w + dot(wposIn, u.cay.xyz);
  var hit = clsTier(clsBuf[0], pe, pn);
  if (!hit.have) { hit = clsTier(clsBuf[1], pe, pn); }
  let kDef = clsBuf[2];
  let win = select(kDef, hit.best, hit.have);
  let los = select(kDef, hit.second, hit.have && hit.second != win);
  /* THE TRANSITION BELONGS TO THE PAIR, and a BUILT edge and a GROWN edge are not the same thing
   * (doc/goal.md). A built edge is a COMPONENT: tarmac ends where it was laid, and a diffuse
   * neighbour cannot widen a seam — so if either class is built, the pair takes the smallest built
   * reach. Two grown edges are two overlapping GRADIENTS, so the zone is their union and the pair
   * takes the larger. That is why road-against-meadow is 0.05 m and wood-against-meadow 0.90 m out
   * of one expression. footM keeps it at least one fragment wide, because narrower than a pixel is
   * aliasing and not a hard edge; kClsFray is the floor under a class that declares nothing. */
  let ea = vegTbl[win].edge;
  let eb = vegTbl[los].edge;
  let builtA = select(1.0e9, ea.x, ea.y > 0.5);
  let builtB = select(1.0e9, eb.x, eb.y > 0.5);
  let seam = min(builtA, builtB);
  let zone = max(ea.x, eb.x);
  let half = max(max(kClsFray, select(zone, seam, seam < 1.0e8)), footM);
  let mixW = clamp(0.5 + hit.dist / (2.0 * half), 0.0, 1.0);
  o.cls = select(los, win, mixW >= 0.5);
  let ra = vegTbl[win];
  let rb = vegTbl[los];
  var gnd = mix(rb.ground, ra.ground, mixW);
  var ltr = mix(rb.litter, ra.litter, mixW);
  var gsf = mix(rb.gsurf, ra.gsurf, mixW);
  var lsf = mix(rb.lsurf, ra.lsurf, mixW);
  var mxp = mix(rb.mixp, ra.mixp, mixW);
  o.gr = mix(rb.grass, ra.grass, mixW);
  o.dr = mix(rb.dry, ra.dry, mixW);
  o.pr = mix(rb.par, ra.par, mixW);

  /* THE DEM ANSWERS WHERE OSM DID NOT: on a face steeper than the class's own declared
   * slope.plausibleDeg[1] there is no soil to hold anything, so the ground turns to the declared
   * bare-rock row over a band the width of the angle-of-repose range (world/AlpineLimit.h — the SAME
   * expression the stand scatter uses, so a tree is never left standing on a pixel painted rock).
   * A FRACTION AND NOT A SECOND CLASS: a fraction has no boundary, and the slope field at the mesh's
   * own resolution is already ragged, so nothing here can draw a contour. */
  let upB = normalize(A.camPosMm.xyz + wposIn * 1.0e-6);
  let slopeDeg = degrees(acos(clamp(dot(normalize(nrmIn), upB), 0.0, 1.0)));
  let slopeMax = mix(rb.edge.w, ra.edge.w, mixW);
  let bare = select(0.0, smoothstep(slopeMax, slopeMax + u.sgr.w, slopeDeg), u.sgr.w > 0.0);
  if (bare > 0.0) {
    let rr = vegTbl[u32(u.sgr.z)];
    gnd = mix(gnd, rr.ground, bare);
    ltr = mix(ltr, rr.litter, bare);
    gsf = mix(gsf, rr.gsurf, bare);
    lsf = mix(lsf, rr.lsurf, bare);
    mxp = mix(mxp, rr.mixp, bare);
    o.gr = mix(o.gr, rr.grass, bare);
    o.dr = mix(o.dr, rr.dry, bare);
    o.pr = mix(o.pr, rr.par, bare);
    o.cls = select(o.cls, u32(u.sgr.z), bare >= 0.5);
  }

  /* pe/pn, not a frame off the normal: a frame off the normal shears the pattern with the slope. */
  let nn = normalize(nrmIn);
  let rel = reliefField(pe, pn, gsf.x, gsf.y, max(gsf.z, 0.05), max(gsf.w, 0.01), footM);
  var we = u.cax.xyz - nn * dot(nn, u.cax.xyz);
  let wl = length(we);
  we = select(vec3f(1.0, 0.0, 0.0), we / max(wl, 1.0e-6), wl > 1.0e-4);
  let wn = cross(nn, we);
  o.nrm = normalize(nn - we * rel.sx - wn * rel.sy);

  /* The detail field also decides WHICH of the two materials is on top here: litter is a coverage
   * fraction, not a second class, so it lands in the hollows the same field carves. */
  let det = clamp(0.5 + (rel.n - 0.5) * (mxp.y * 2.0), 0.0, 1.0);
  let lm = clamp(mxp.x * 2.0 * det, 0.0, 1.0);
  o.alb = mix(gnd.rgb, ltr.rgb, lm);
  /* TOKSVIG: relief the pixel cannot resolve does not vanish, it becomes ROUGHNESS. Without this the
   * far field sparkles, because a narrow lobe is evaluated against a normal that is an average of
   * many. alpha = roughness (see ggxSpec), so alpha^2 = r^2 and the unresolved slope VARIANCE adds
   * to it directly. */
  o.rough = min(sqrt(mix(gnd.w, ltr.w, lm) * mix(gnd.w, ltr.w, lm) + 2.0 * rel.unres), 1.0);
  o.spec = mix(mxp.z, mxp.w, lm);
  let ampGrain = mix(gsf.y, lsf.y, lm);
  let occ = 1.0 - kSelfShadow * (ampGrain * 100.0) * (1.0 - rel.n) * octWeight(gsf.z, footM);
  o.alb = o.alb * clamp(occ, 0.2, 1.0);
  return o;
}
)";

/* The same entry point with nothing behind it: no binding is declared, so the pipeline layout has two
 * fewer slots and the whole branch dead-strips into one neutral material. */
static const char *kVegOffWGSL = R"(
struct GroundMat { alb : vec3f, rough : f32, nrm : vec3f, spec : f32, cls : u32,
                   gr : vec4f, dr : vec4f, pr : vec4f };
fn groundMat(wposIn : vec3f, gposIn : vec3f, nrmIn : vec3f, footM : f32) -> GroundMat {
  return GroundMat(vec3f(0.15), 0.9, nrmIn, 0.0, 0u, vec4f(0.0), vec4f(0.0), vec4f(0.0));
}
)";

/* The storage-buffer and draw-loop bound; a multi-LOD cut to 240 km stays well under it. */
static const int kMaxDraws = 4096;

void TilesStage::Configure(const Gpu &gpu, wgpu::Sampler lutSamp,
                             wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf, int maxLayers,
                             wgpu::Buffer vegTable, const SceneLight &light) {
  VegBuf = vegTable;
  Light = light;
  Device = gpu.Device;
  Queue = gpu.Queue;
  HdrFormat = gpu.HdrFormat;
  LutSamp = lutSamp;
  SkyLutView = skyLutView;
  AtmoBuf = atmoBuf;
  MaxLayers = maxLayers;

  {
    wgpu::BufferDescriptor cb{};
    cb.size = 256;
    cb.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    ClassBuf = Device.CreateBuffer(&cb);
  }

  /* osmmesh emits a triangle SOUP (6 verts/quad), so the draw is non-indexed.
   * TODO: weld + index on upload — it would halve the traffic. */
  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  /* Storage, not uniform: it scales past the 64 KB uniform limit. Rewritten every frame. */
  bd.size = (uint64_t)kMaxDraws * kTileFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  TileBuf = Device.CreateBuffer(&bd);

  /* Atmo struct + sky-view sampling + THE shared air (constants, extinction law, inscatter colour) —
   * the same three splices the cloud pass makes, in the same order. */
  const std::string terrSrc = std::string(kSceneScaleWGSL) + kAtmoCommon + kAtmoSample
                            + HazeConstsWGSL() + kHazeWGSL + kSurfaceLightWGSL + ShadowSampleWGSL()
                            /* [SET] 0.05 m: the world-fixed FLOOR under the boundary fray, so a
                             * boundary a fragment could resolve to nothing still has a width. Under
                             * the 0.140 m mean residual the vector source carries against raw OSM. */
                            + "const kClsFray : f32 = 0.05;\n"
                            + "const kOctN : i32 = " + std::to_string(kReliefOctaves) + ";\n"
                            + "const kSpecOn : f32 = 1.0;\n"
                            /* THE ONE DIAGNOSTIC LEFT IN THIS SHADER (doc/goal.md): the RESOLVED class
                             * index as a colour, before light, haze and AO. It answers a question no
                             * picture can — which class a fragment decided it was. */
                            + "const kClsViz : f32 = " + std::string(getenv("FB_GROUND_CLASS_VIZ") &&
                                atoi(getenv("FB_GROUND_CLASS_VIZ")) != 0 ? "1.0" : "0.0") + ";\n"
                            + SwardConstsWGSL() + kSwardWGSL + kSwardAggWGSL
                            + kTerrainWGSL + (VegBuf ? kVegOnWGSL : kVegOffWGSL);
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = terrSrc.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attrs[3] = {};
  attrs[0].format = wgpu::VertexFormat::Float32x3; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
  attrs[1].format = wgpu::VertexFormat::Float32x2; attrs[1].offset = 12; attrs[1].shaderLocation = 1;
  attrs[2].format = wgpu::VertexFormat::Float32x3; attrs[2].offset = 20; attrs[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 32;
  vbl.attributeCount = 3;
  vbl.attributes = attrs;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;  /* [0,1] reversed-Z: nearer = larger */

  wgpu::ColorTargetState ct{};
  ct.format = HdrFormat;   /* scene renders into the offscreen HDR target, not the swapchain */

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
  rp.primitive.cullMode = wgpu::CullMode::None;  /* TODO: cull once the ECEF soup winding is pinned */
  Pipe = Device.CreateRenderPipeline(&rp);

  RebuildBind();
}

void TilesStage::CollectCasters(std::vector<Caster> &out) const {
  out.clear();
  for (const DynTile &d : DynTiles) {
    if (!d.Used || !d.Vtx || d.NVerts == 0) continue;
    Caster c{};
    c.Vtx = d.Vtx;
    c.Idx = d.Idx;
    c.NVerts = d.NVerts;
    c.NIdx = d.NIdx;
    c.Clusters = d.Clusters.data();
    c.NClusters = (int)d.Clusters.size();
    for (int i = 0; i < 3; i++) { c.Origin[i] = d.Origin[i]; c.BoundCtr[i] = d.BoundCtr[i]; }
    c.BoundRad = d.BoundRad;
    out.push_back(c);
  }
}

void TilesStage::SetSward(double lat, double lon, const double east[3], const double north[3],
                          const double up[3]) {
  const Graticule cg = Graticule::At(lat, lon);
  SwCellE = cg.CellE;
  SwCellN = Graticule::kCellM;
  SwFracE = cg.FracE;
  SwFracN = cg.FracN;
  SwBaseI = cg.BaseI;
  SwBaseJ = cg.BaseJ;
  for (int i = 0; i < 3; i++) { SwEast[i] = east[i]; SwNorth[i] = north[i]; SwUp[i] = up[i]; }
  SwHave = true;
}

void TilesStage::SetClassFrame(const double east[3], const double north[3],
                               const double camOffset[2]) {
  for (int i = 0; i < 3; i++) { ClsEast[i] = east[i]; ClsNorth[i] = north[i]; }
  ClsCam[0] = camOffset[0];
  ClsCam[1] = camOffset[1];
}

/* One buffer, rewritten whole when the structure changes. It is rebuilt only when the camera leaves
 * its grid or a vector tile lands, so this is not a per-frame cost. */
void TilesStage::WriteClassBuffer(const uint32_t *words, size_t bytes) {
  if (!Device || bytes == 0) return;
  const uint64_t need = ((uint64_t)bytes + 255u) & ~(uint64_t)255u;
  if (!ClassBuf || ClassBuf.GetSize() < need) {
    wgpu::BufferDescriptor bd{};
    bd.size = need;
    bd.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    ClassBuf = Device.CreateBuffer(&bd);
    if (Pipe) RebuildBind();
  }
  Queue.WriteBuffer(ClassBuf, 0, words, (bytes + 3u) & ~(size_t)3u);
  ClassBytes = (long)need;
}

/* Recreate at double the cap and copy the resident layers over. Rare. */
void TilesStage::RebuildBind(void) {
  wgpu::BindGroupEntry be[15] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = TileBuf; be[1].size = TileBuf.GetSize();
  be[2].binding = 4; be[2].sampler = LutSamp;            /* azimuth-wrapping, for the sky-view LUT */
  be[3].binding = 5; be[3].textureView = SkyLutView;     /* the haze's inscatter colour */
  be[4].binding = 6; be[4].buffer = AtmoBuf; be[4].size = kAtmoUniformBytes;
  int nbe = 5;
  be[nbe].binding = 10; be[nbe].buffer = Light.Cascades;   be[nbe].size = kShadowUniFloats * sizeof(float); nbe++;
  be[nbe].binding = 11; be[nbe].textureView = Light.ShadowAtlas; nbe++;
  be[nbe].binding = 12; be[nbe].sampler = Light.ShadowCompare;   nbe++;
  if (VegBuf) { be[nbe].binding = 7; be[nbe].buffer = VegBuf; be[nbe].size = wgpu::kWholeSize; nbe++; }
  be[nbe].binding = 9;  be[nbe].buffer = Light.Irradiance; be[nbe].size = wgpu::kWholeSize; nbe++;
  if (VegBuf) { be[nbe].binding = 13; be[nbe].buffer = ClassBuf; be[nbe].size = wgpu::kWholeSize; nbe++; }
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = (uint32_t)nbe;
  bgd.entries = be;
  Bind = Device.CreateBindGroup(&bgd);
}

/* No mip building here — that already ran off-thread or synchronously. */
/* EMA-smoothed, so the far field does not flicker as tiles stream and evict. */
int TilesStage::UploadTile(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                             const DagCluster *clusters, int nclusters,
                             const double origin[3], const double anchor[3]) {
  int slot = -1;
  for (int i = 0; i < (int)DynTiles.size(); i++)
    if (!DynTiles[i].Used) { slot = i; break; }
  if (slot < 0) { slot = (int)DynTiles.size(); DynTiles.push_back(DynTile{}); }

  DynTile &d = DynTiles[slot];
  wgpu::BufferDescriptor bd{};
  bd.size = (uint64_t)nverts * 8 * sizeof(float);
  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  d.Vtx = Device.CreateBuffer(&bd);
  Queue.WriteBuffer(d.Vtx, 0, verts, (size_t)nverts * 8 * sizeof(float));
  d.NVerts = nverts;
  MeshBytes += (long)bd.size;
  wgpu::BufferDescriptor id{};
  id.size = (uint64_t)nidx * sizeof(uint32_t);
  id.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
  d.Idx = Device.CreateBuffer(&id);
  Queue.WriteBuffer(d.Idx, 0, idx, (size_t)nidx * sizeof(uint32_t));
  d.NIdx = nidx;
  MeshBytes += (long)id.size;
  BoundingSphere(verts, nverts, 8, d.BoundCtr, &d.BoundRad);
  d.Clusters.assign(clusters, clusters + (nclusters > 0 ? nclusters : 0));
  if (d.Clusters.empty()) {   /* no DAG: one root cluster over the whole soup keeps the cut uniform */
    DagCluster c{};
    c.Count = nidx;
    c.ParentErr = kDagRootErr;
    for (int a = 0; a < 3; a++) c.SelfCenter[a] = d.BoundCtr[a];
    c.SelfRadius = d.BoundRad;   /* the frustum test must mean the same thing with and without a DAG */
    d.Clusters.push_back(c);
  }
  for (int a = 0; a < 3; a++) { d.Origin[a] = origin[a]; d.Anchor[a] = (float)(origin[a] - anchor[a]); }
  d.Used = true;
  return slot;
}

void TilesStage::ReleaseTile(int slot) {
  if (slot < 0 || slot >= (int)DynTiles.size() || !DynTiles[slot].Used) return;
  DynTile &d = DynTiles[slot];
  MeshBytes -= (long)d.NVerts * 8 * (long)sizeof(float) + (long)d.NIdx * (long)sizeof(uint32_t);
  d.Vtx = nullptr;   /* drop the ref -> buffer freed */
  d.Idx = nullptr;
  d.NVerts = 0;
  d.NIdx = 0;
  d.Clusters.clear();
  d.Used = false;
}

void TilesStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {

  static std::vector<float> off;
  int nDraw;
  DrawnVerts = 0;
  for (int i = 0; i < kLevelBins; i++) DrawnByLevel[i] = 0;
  Ranges.clear();
  CutClusters = 0;
  /* THE CUT, per cluster and not per tile: one tile can answer at several levels at once, which is
   * the whole point of the DAG (doc/render/lod.md). f_px carries resolution AND zoom, so no distance
   * is written down anywhere. */
  const float fPx = 0.5f * (float)ctx.Height / std::tan(0.5f * (float)ctx.FovDeg * 3.14159265358979f / 180.0f);
  Frustum fr;
  fr.Set(ctx.Mvp20);
  VisibleTiles = 0;
  {
    nDraw = (int)DrawList.size();
    if (nDraw > kMaxDraws) nDraw = kMaxDraws;
    off.assign((size_t)nDraw * kTileFloats, 0.0f);
    for (int i = 0; i < nDraw; i++) {
      const DynTile &d = DynTiles[DrawList[i]];
      const double rel[3] = {d.Origin[0] - ctx.Eye[0], d.Origin[1] - ctx.Eye[1],
                             d.Origin[2] - ctx.Eye[2]};
      const double eyeLocal[3] = {-rel[0], -rel[1], -rel[2]};
      /* The tile first, then its clusters: a whole tile leaves on one test, and only the tiles the
       * frustum actually straddles pay the per-cluster one. */
      const bool tileIn = fr.Visible(rel, d.BoundCtr, d.BoundRad);
      if (tileIn) VisibleTiles++;
      /* Geocentric up at the tile centre — the direction the DAG measured its error along. */
      const double oLen = std::sqrt(d.Origin[0] * d.Origin[0] + d.Origin[1] * d.Origin[1] +
                                    d.Origin[2] * d.Origin[2]);
      const float up[3] = {(float)(d.Origin[0] / oLen), (float)(d.Origin[1] / oLen),
                           (float)(d.Origin[2] / oLen)};
      for (const DagCluster &c : d.Clusters) {
        if (!tileIn) break;
        if (!DagSelect(c, eyeLocal, fPx, SseTauPx(), up)) continue;
        if (!fr.Visible(rel, c.SelfCenter, c.SelfRadius)) continue;
        CutClusters++;
        DrawnVerts += (long)c.Count;
        DrawnByLevel[c.Level < kLevelBins ? c.Level : kLevelBins - 1] += (long)c.Count / 3;
        /* Clusters are laid down level by level in Morton order, so a run of neighbours at one level
         * is contiguous and collapses into ONE draw. */
        if (!Ranges.empty() && Ranges.back().Slot == (uint32_t)i &&
            Ranges.back().First + Ranges.back().Count == c.First)
          Ranges.back().Count += c.Count;
        else
          Ranges.push_back(DrawRange{(uint32_t)i, c.First, c.Count});
      }
      float *e = &off[(size_t)i * kTileFloats];
      e[0] = (float)(d.Origin[0] - ctx.Eye[0]);
      e[1] = (float)(d.Origin[1] - ctx.Eye[1]);
      e[2] = (float)(d.Origin[2] - ctx.Eye[2]);
      e[3] = 0.0f;
      e[8] = d.Anchor[0];
      e[9] = d.Anchor[1];
      e[10] = d.Anchor[2];
    }
  }
  if (nDraw > 0) Queue.WriteBuffer(TileBuf, 0, off.data(), off.size() * sizeof(float));

  /* The camera-relative MVP + sun (0..19, as the frame context carries them) plus the ATMOSPHERE the
   * fragment shader needs as numbers: one Koschmieder sigma0 and the geometry to turn a fragment
   * offset into an altitude. Reused stack array — no per-frame heap. */
  float uni[kUniFloats];
  std::memcpy(uni, ctx.Mvp20, sizeof ctx.Mvp20);
  const double eyeLen = std::sqrt(ctx.Eye[0] * ctx.Eye[0] + ctx.Eye[1] * ctx.Eye[1] + ctx.Eye[2] * ctx.Eye[2]);
  uni[20] = HazeSigma0(Sky.VisibilityM);
  uni[21] = ctx.AltM;
  uni[22] = (float)((eyeLen - (double)ctx.AltM) / 1.0e6);   /* the REAL WGS84 radius under the camera */
  /* One pixel as an ANGLE: the ground shader fades a detail octave out on its own screen footprint,
   * so no distance in metres is written down anywhere. */
  uni[23] = (float)(2.0 * std::tan(0.5 * (double)ctx.FovDeg * 3.14159265358979 / 180.0) /
                    (double)(ctx.Height > 0 ? ctx.Height : 1));
  const float sunUp = (float)(ctx.SunDir[0] * ctx.Up[0] + ctx.SunDir[1] * ctx.Up[1] + ctx.SunDir[2] * ctx.Up[2]);
  for (int i = 0; i < 3; i++) {
    uni[24 + i] = (float)SwEast[i];
    uni[28 + i] = (float)SwNorth[i];
    uni[32 + i] = (float)SwUp[i];
  }
  uni[27] = (float)SwCellE;
  uni[31] = (float)SwCellN;
  uni[35] = SwHave ? std::max(0.0f, std::min(1.0f, sunUp)) : 0.0f;
  uni[36] = (float)SwFracE;
  uni[37] = (float)SwFracN;
  uni[38] = (float)BareRockRow;
  uni[39] = BareSlopeBandDeg;
  const int32_t swBase[4] = {(int32_t)SwBaseI, (int32_t)SwBaseJ, 0, 0};
  std::memcpy(&uni[40], swBase, sizeof swBase);
  for (int i = 0; i < 3; i++) { uni[44 + i] = (float)ClsEast[i]; uni[48 + i] = (float)ClsNorth[i]; }
  uni[47] = (float)ClsCam[0];
  uni[51] = (float)ClsCam[1];
  float groundThru = 1.0f, tauLog[3] = {};
  for (int i = 0; i < 3; i++) {
    const CloudDeckParams &d = Sky.Deck[i];
    tauLog[i] = DeckSunOpticalDepth(d, sunUp);
    groundThru *= DeckSunTransmittance(tauLog[i], d.Cover, 1.0f);   /* the AREA MEAN; the picture is per pixel */
  }
  /* The air and the light this frame, as NUMBERS: what the picture does with them is measurable off a
   * PNG, but why it does it is not. Logged when the atmosphere actually changes, like cloud_decks. */
  if (std::fabs(uni[20] - LoggedSigma0) > 1e-9f || std::fabs(groundThru - LoggedSunThru) > 1e-4f) {
    LoggedSigma0 = uni[20];
    LoggedSunThru = groundThru;
    Log::Debug("render", "terrain_light",
                 {{"visM", (double)Sky.VisibilityM}, {"sigma0PerM", (double)uni[20]},
                  {"camAltM", (double)ctx.AltM}, {"sunUp", (double)sunUp},
                  {"lowTau", (double)tauLog[0]}, {"midTau", (double)tauLog[1]}, {"highTau", (double)tauLog[2]},
                  {"groundSunThru", (double)groundThru},
                  {"groundLitFrac", (double)(3.0f * groundThru / (0.4f + 0.15f * (1.0f - groundThru) + 3.0f * groundThru))}});
  }
  Queue.WriteBuffer(Uni, 0, uni, sizeof uni);

  /* Signature = the draw STRUCTURE only. TileBuf/uniform CONTENTS change every frame, but the bundle
   * references those buffers by HANDLE — so only a structural change forces a re-record. */
  if (!Ranges.empty()) {
    uint64_t sig = 1469598103934665603ULL;
    auto mix = [&sig](uint64_t v) { sig ^= v; sig *= 1099511628211ULL; };
    mix((uint64_t)nDraw);
    mix((uint64_t)(uintptr_t)Bind.Get());
    for (int i = 0; i < nDraw; i++) mix((uint64_t)(uintptr_t)DynTiles[DrawList[i]].Vtx.Get());
    for (const DrawRange &r : Ranges) { mix(r.Slot); mix(r.First); mix(r.Count); }
    if (sig != BundleSig || !Bundle) {
      /* A bundle's attachment formats must be the PASS's, both of them — the terrain writes only
       * the first, but a bundle recorded against one format cannot be replayed into a two-attachment
       * pass at all. */
      wgpu::TextureFormat cf[2] = {HdrFormat, kVelocityFormat};
      wgpu::RenderBundleEncoderDescriptor rbd{};
      rbd.colorFormatCount = 2;
      rbd.colorFormats = cf;
      rbd.depthStencilFormat = wgpu::TextureFormat::Depth32Float;
      wgpu::RenderBundleEncoder rbe = Device.CreateRenderBundleEncoder(&rbd);
      rbe.SetPipeline(Pipe);
      rbe.SetBindGroup(0, Bind);
      uint32_t bound = 0xffffffffu;
      for (const DrawRange &r : Ranges) {
        if (r.Slot != bound) {
          const DynTile &t = DynTiles[DrawList[r.Slot]];
          rbe.SetVertexBuffer(0, t.Vtx);
          rbe.SetIndexBuffer(t.Idx, wgpu::IndexFormat::Uint32);
          bound = r.Slot;
        }
        rbe.DrawIndexed(r.Count, 1, r.First, 0, r.Slot);
      }
      Bundle = rbe.Finish();
      BundleSig = sig;
      TerrainBundleRecords++;
    }
  }

  /* per-tile buffers baked into Bundle; firstInstance = draw index -> storage entry */
  if (Bundle && !Ranges.empty()) pass.ExecuteBundles(1, &Bundle);
}

} // namespace outshine::Render
