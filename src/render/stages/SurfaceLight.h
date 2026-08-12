/* THE ONE SCENE-REFERRED SCALE, as WGSL every lit surface splices. Radiance is computed in the units
 * the atmosphere LUTs are in — top-of-atmosphere solar irradiance = 1 — and multiplied by exactly one
 * exposure on the way out. There is no second, independently fitted path: the ambient a surface
 * receives is IrradianceStage's integral of the very sky the sky pass draws.
 *
 * The alpha a lit surface writes is the fraction of its radiance screen-space occlusion may NOT
 * take. Occlusion is only known after the scene pass and must darken sky light rather than sunlight;
 * carrying the fraction in the HDR target's otherwise unused alpha is what lets the composite apply
 * it correctly without a second colour attachment. The sense is that way round and not the obvious
 * one because every stage that has NOT been converted writes 1.0 already, and 1.0 must mean "AO does
 * not apply".
 * Requires SceneScale.h (kSceneExposure, kInvPi) in scope. */
#ifndef SURFACELIGHT_H
#define SURFACELIGHT_H

namespace outshine::Render {

static const char *kSurfaceLightWGSL = R"(
struct Irr { sun : vec4f, sky : vec4f };

/* No auto-exposure exists yet, so a radiometrically correct night clips to black. This floor is a
 * DISPLAY CRUTCH, not radiometry: it reproduces the ground level the previous model showed at night
 * (albedo * 0.4 * 0.08 = 0.032 * albedo), i.e. kSceneExposure * E / PI = 0.032. */
const kNightAmbient : f32 = 0.00914;
/* A cloud deck is non-absorbing in the visible, so the beam it intercepts leaves it again — roughly
 * half of it downward. That is what the ground gains in diffuse for what it loses in direct. */
const kDeckDiffuse : f32 = 0.5;
/* GROUND BOUNCE, the FAR half-space: what a surface sees BELOW its own horizon. Without it a shaded
 * wall is lit by the blue sky alone and comes out navy, which no photograph of a street ever shows.
 * 0.12 is the mean VISIBLE reflectance of Central European land cover (grass ~0.10, tilled soil
 * ~0.12, sealed ~0.09, broadleaf canopy ~0.05, concrete ~0.25) — grey, because what a wall sees is
 * the neighbourhood and not itself. */
const kGroundBounce : f32 = 0.12;
/* GROUND BOUNCE, the NEAR half-space: how much of its own sky a surface's sub-pixel relief hides.
 * The geometric term above is zero on flat ground — a plane cannot see itself — and yet a shadow on
 * soil is a darker soil and not a blue hole, because the millimetre relief that CASTS the micro
 * shadow is also what FILLS it, with light off the same material. [SET] 0.35: the sky fraction a
 * clod, a furrow or a sward hides from a point between them — the quantity SSAO measures, one scale
 * below what SSAO can resolve. */
const kSelfShelter : f32 = 0.35;

/* THE AIR UNDER THE DECK IS DARKER TOO. Aerial perspective is sunlight scattered by the column
 * between eye and subject, and under a deck that column is lit by what the deck lets through, not by
 * the clear sky the LUT was baked for. Without it the far field is inscatter at full clear-sky
 * strength over a shadowed ground, and a forced 98 % shadow beyond 4 km moves the 5-15 km band by
 * 1.4 % of display luminance (MEASURED, control render). The factor is the ratio of the horizontal
 * irradiance under the deck to the one without it — the same quantity litRadiance builds two lines
 * further down, so there is no second model and no new constant. */
fn deckHazeFactor(I : Irr, sunUp : f32, thruMean : f32) -> f32 {
  let yw = vec3f(0.2126, 0.7152, 0.0722);
  let sunY = dot(I.sun.xyz, yw) * max(sunUp, 0.0);
  let skyY = dot(I.sky.xyz, yw);
  let clear = sunY + skyY;
  if (clear <= 1.0e-9) { return 1.0; }
  return (sunY * thruMean + sunY * (1.0 - thruMean) * kDeckDiffuse + skyY) / clear;
}

/* alb      linear reflectance (0..1), NOT a display colour
 * n        unit surface normal, ECEF
 * upv      unit geodetic up at the fragment
 * sunDir   unit ECEF direction to the sun
 * sunVis   0..1 unshadowed fraction of the sun disc (cascaded shadow maps)
 * thruDir  cloud transmittance of the beam (stages/CloudShadow.h cloudMeanThru)
 * thruMean the same, AREA-MEANED over the deck — what the deck re-emits downward is a hemisphere
 *          average, so standing in a sunbeam one still sees the whole lit underside
 * kdDir    1 - F(v.h) for the SUN's half-vector: what the coherent specular takes out of the beam is
 *          what the diffuse under it no longer gets. `alb` carries the AMBIENT complement (1 - the
 *          split-sum environment BRDF) because the sky integral has no single half-vector; a caller
 *          without a specular term passes 1.0 and gets the old behaviour exactly
 * night    ambient floor, kNightAmbient * (1 - daylight) for EVS and 0 for the daylit database view */
fn litRadiance(I : Irr, alb : vec3f, kdDir : f32, n : vec3f, upv : vec3f, sunDir : vec3f,
               sunVis : f32, thruDir : f32, thruMean : f32, night : f32) -> vec4f {
  let sunUp = max(dot(sunDir, upv), 0.0);
  /* Diffuse on a HORIZONTAL surface; a tilted one sees the fraction of the sky dome its normal
   * subtends, which for a uniform dome is exactly (1 + n.up)/2. */
  let skyH = I.sky.xyz + I.sun.xyz * (sunUp * (1.0 - thruMean) * kDeckDiffuse) + vec3f(night);
  let ndu = dot(n, upv);
  /* E_bounce, the one term that was missing — in TWO half-spaces, because "what returns light to
   * this point" is two different things at two scales.
   *
   * FAR, below the normal's horizon: the neighbourhood, grey and statistically lit — a wall does not
   * know what colour the street is, and the deck over that street is averaged, not local.
   * NEAR, the surface's own sub-pixel relief: the SAME material under the SAME local light, so the
   * reflectance is `alb` itself. That is what makes a micro-shadow a darker version of one hue
   * instead of a hole filled with blue sky — Idso's own soil is 10YR 5/3 dry and 10YR 3/3 wet, the
   * same hue at another value. One reflection, no second bounce, no GI. */
  let farE = (skyH + I.sun.xyz * (sunUp * thruMean)) * kGroundBounce;
  let nearE = (skyH + I.sun.xyz * (sunUp * sunVis * thruDir)) * alb;
  let skyW = (0.5 + 0.5 * ndu) * (1.0 - kSelfShelter);
  let ambE = skyH * skyW + farE * (0.5 - 0.5 * ndu) + nearE * ((0.5 + 0.5 * ndu) * kSelfShelter);
  let dirE = I.sun.xyz * (max(dot(n, sunDir), 0.0) * sunVis * thruDir * kdDir);
  let k = alb * (kSceneExposure * kInvPi);
  let ambL = k * ambE;
  let totL = k * (ambE + dirE);
  let ty = dot(totL, vec3f(0.2126, 0.7152, 0.0722));
  let ambFrac = select(0.0, dot(ambL, vec3f(0.2126, 0.7152, 0.0722)) / ty, ty > 1.0e-9);
  /* What occlusion takes out of the sky does not vanish: whatever stood in the way is a SURFACE, and
   * it hands back the same kGroundBounce as any other piece of neighbourhood. A screen-space estimate
   * knows only that something was there, never what, so the grey neighbourhood figure is the whole
   * answer available. Folded into alpha, so the composite stays one mix() and the number stays in
   * one place. */
  return vec4f(totL, 1.0 - ambFrac * (1.0 - kGroundBounce));
}
)";

} // namespace outshine::Render
#endif
