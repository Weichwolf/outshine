/* THE STAND, as ONE declaration: what a population of blades does to the light ON AVERAGE, which is
 * the ground fragment's own colour. Everything it needs is here and nowhere else — the canopy-top
 * field, the leaf-angle population G, the tip-height distribution, the beam's path through the stand
 * and the aggregate radiance. TilesStage splices it and does not own it.
 *
 * WHAT IS NOT HERE: a blade's own geometry. There is no blade — a stand below the size of a tree is
 * never geometry (doc/goal.md), so the declaration has one scale and no hand-off to match up. */
#ifndef SWARD_H
#define SWARD_H

#include <string>

namespace outshine::Render {

inline std::string SwardConstsWGSL(void) {
  std::string s;
  /* [SET] the population mean of the tangent's vertical component over the declared tilt and arch
   * distributions, Monte-Carlo over 4e5 blades x 128 stations. It is the ONE number that turns a
   * declared sward HEIGHT into a blade LENGTH, and therefore the one that turns blades/m^2 and a
   * width into a leaf area index. */
  s += "const kMeanSin : f32 = 0.5696;\n";
  /* THE CANOPY TOP IS A LADDER OF THREE SCALES AND ONLY THE COARSEST TWO ARE GROWTH.
   * stand (kStandCells cells) and patch (one cell) modulate how TALL the plants are, so the drawn
   * blade heights follow them. The TUSSOCK rung does not: a crown's local top is the maximum over
   * the blades that stand in it, which the geometry already realises blade by blade — the field
   * carries it only so that the LIGHT integrates against the same envelope the geometry draws.
   * Multiplying it into a blade height would count the same population twice. */
  s += "const kStandCells : i32 = 10;\n";
  s += "const kPatchAmp : f32 = 0.18;\n";
  s += "const kStandAmp : f32 = 0.12;\n";
  /* DERIVED from the tussock population itself: wiese places 800 blades/m^2 in crowns of kPerTuft = 8,
   * i.e. 100 crowns/m^2 and a spacing of 0.10 m, so the envelope's lattice is ten subdivisions of the
   * 1 m graticule cell. */
  s += "const kTuftCells : f32 = 10.0;\n";
  /* DERIVED: the local top of a crown is the MAXIMUM of kPerTuft tips drawn from the declared uniform
   * height jitter U(1-j, 1+j). For n = 8 that maximum has sd 2j*sqrt(n/((n+1)^2(n+2))) = 0.0894 at
   * j = 0.45; a value-noise octave of amplitude A delivers sd 0.41*A (measured over the lattice this
   * file uses), so A = 0.0894/0.41. This is the rung the earlier round was missing: the field the
   * light was computed against had sd 0.0266 m where the geometry that is DRAWN has 0.0781 m, and a
   * surface flatter than tan(sun elevation) cannot shadow itself. */
  s += "const kTuftAmp : f32 = 0.218;\n";
  /* The field's own ceiling, which is what bounds the beam march: 1 + the three amplitudes. */
  s += "const kTopMax : f32 = 1.518;\n";
  /* [SET] midpoint samples along the sun through the canopy-top field. The run a point near the top
   * has to cover is (kTopMax*(1+jit) - z/h0)/tan(el) and shrinks to nothing at the top, so the step
   * is finest exactly where the visible surface is. */
  s += "const kSunSteps : i32 = 6;\n";
  /* DERIVED, and from the SAME leaf-angle population the geometry is built from: G(el) = E[|n.s|]
   * over tilt, arch and azimuth, Monte-Carlo over 1.3e7 leaf elements, fitted to 0.5 % by
   * G = kG0 + kG1*sin(el)^kGp. At nadir it is 0.6546 = projLAI/LAI exactly. */
  s += "const kG0 : f32 = 0.4050;\n";
  s += "const kG1 : f32 = 0.2496;\n";
  s += "const kGp : f32 = 1.5700;\n";
  /* DERIVED from the same G: the sky is a hemisphere, so its optical depth is the hemispherical
   * integral of G(theta)/cos(theta), fitted to 0.027 over LAI 0-6. */
  s += "const kDiffK0 : f32 = 0.87;\n";
  /* sqrt(1 - omega), Goudriaan: what a leaf does not absorb it scatters onward. omega = 0.185. */
  s += "const kScatCut : f32 = 0.9028;\n";
  s += "const kMinSinEl : f32 = 0.05;\n";
  s += "const kLeafTrans : f32 = 0.85;\n";
  s += "const kTransIso : f32 = 0.35;\n";
  s += "const kTransFwd : f32 = 2.6;\n";
  s += "const kTransP : f32 = 4.0;\n";
  /* [SET] HOW A LEAF DIES, not how one is drawn: a grass blade senesces from the TIP down, so of the
   * blades a template's dryFraction calls dry, kWholeDry are dry end to end and the rest carry a dry
   * tip over kTipRun of their length. The population mean is therefore 0.431 * dryFraction. The pair
   * was written for a blade shader that has since been deleted; what survives the deletion is the
   * senescence, and NEITHER number is measured against a phenological series. */
  s += "const kWholeDry : f32 = 0.35;\n";
  s += "const kTipRun : f32 = 0.25;\n";
  /* [SET] 8, the midpoint of the 3-15 leaves a tussock fans out. */
  /* THE EPOCH AND THE DECAY STEP, and they arrive here because this is the ONE declaration both
   * scales read: a material that later has to weather from intact to overgrown must weather the same
   * way whether it is drawn as geometry or as a ground fragment, and a parameter added to only one of
   * them would already be wrong. Both are small INDICES and not dials — three epochs, three decay
   * steps — so a later reading of them is a shader VARIANT and not a blend, which is why they are
   * baked consts here and not uniforms. NOTHING READS THEM THIS ROUND. That is deliberate: a field
   * that arrives and is ignored is honest, one that is missing is a second visit to this file.
   * DEFAULT 0/0 = the present, no decay; declared, not guessed (doc/render/classification.md's
   * SiteContext). */
  s += "const kEpoch : i32 = 0;\n";
  s += "const kDecay : i32 = 0;\n";
  /* THE MARCHED SLANT PATH, and it is the only term in which the canopy-top field reaches the
   * ground fragment as a PATTERN rather than as a level: without it the far meadow renders as one
   * flat green wash. It integrates the tip profile along the sun over that field, so a stand
   * shadowed by a patch upsun darkens and the darkening is a function of the sun's BEARING, which
   * the column form cannot express at all. On the nadir bench (two sun azimuths 205 deg apart, 80 px
   * tiles) it moves neither the contrast nor the direction — p95/p05 0.326 / 0.402 EV against the
   * column's 0.328 / 0.439 — because that bench measures contrast and this buys structure. */
  return s;
}

/* The shared WGSL, spliced after SwardConstsWGSL. Every name is prefixed `sw` because the stage that
 * splices it brings its own noise and its own hashes. */
static const char *kSwardWGSL = R"(
fn swHashf(seed : u32) -> f32 {
  var s = seed;
  s = s ^ (s >> 16u); s = s * 0x7feb352du;
  s = s ^ (s >> 15u); s = s * 0x846ca68bu;
  s = s ^ (s >> 16u);
  return f32(s & 0x00ffffffu) * (1.0 / 16777216.0);
}

/* IDENTITY IS A PLACE: a world cell and a number inside it, and nothing camera-relative. */
fn swCellHash(cx : u32, cy : u32, k : u32) -> u32 {
  var s = cx * 0x9e3779b1u;
  s = s ^ (cy * 0x85ebca6bu);
  s = s ^ (k * 0xc2b2ae35u);
  s = s ^ (s >> 16u); s = s * 0x7feb352du;
  s = s ^ (s >> 15u); s = s * 0x846ca68bu;
  s = s ^ (s >> 16u);
  return s;
}

/* Value noise ON THE GRATICULE. The lattice index stays an INTEGER all the way into the hash — a
 * point forty metres east of the eye is a million cells east of Greenwich and f32 cannot carry that
 * plus a fraction. */
fn swFloorDiv(a : i32, b : i32) -> i32 { return select((a - b + 1) / b, a / b, a >= 0); }

fn swVNoise(bx : i32, by : i32, fx : f32, fy : f32, salt : u32) -> f32 {
  let ux = bitcast<u32>(bx);
  let uy = bitcast<u32>(by);
  let ux1 = bitcast<u32>(bx + 1);
  let uy1 = bitcast<u32>(by + 1);
  let n00 = swHashf(swCellHash(ux,  uy,  salt));
  let n10 = swHashf(swCellHash(ux1, uy,  salt));
  let n01 = swHashf(swCellHash(ux,  uy1, salt));
  let n11 = swHashf(swCellHash(ux1, uy1, salt));
  let sx = fx * fx * (3.0 - 2.0 * fx);
  let sy = fy * fy * (3.0 - 2.0 * fy);
  return mix(mix(n00, n10, sx), mix(n01, n11, sx), sy);
}

/* HOW TALL THE PLANTS ARE, at the two scales that are growth. `ux`/`uy` are cell coordinates and may
 * leave the cell — a sample a few decimetres upsun is the same field read at another place. */
fn swStandMod(ci : i32, cj : i32, ux : f32, uy : f32) -> f32 {
  let bx = floor(ux);
  let by = floor(uy);
  let cx = ci + i32(bx);
  let cy = cj + i32(by);
  let jx = ux - bx;
  let jy = uy - by;
  let nPatch = swVNoise(cx, cy, jx, jy, 0x51u);
  let sbx = swFloorDiv(cx, kStandCells);
  let sby = swFloorDiv(cy, kStandCells);
  let sfx = (f32(cx - sbx * kStandCells) + jx) / f32(kStandCells);
  let sfy = (f32(cy - sby * kStandCells) + jy) / f32(kStandCells);
  let nStand = swVNoise(sbx, sby, sfx, sfy, 0x93u);
  return clamp(1.0 + kPatchAmp * (nPatch * 2.0 - 1.0) + kStandAmp * (nStand * 2.0 - 1.0), 0.2, 2.0);
}

/* THE REALISED ENVELOPE, which is what the light has to be integrated against: the growth field
 * times the crown-scale maximum the blade population already draws. `res` is the tussock rung's own
 * weight — the caller passes 1 where a crown still covers pixels and lets it fall to 0 below the
 * pixel footprint, at which point the envelope is its own mean and the shadow it casts has become a
 * mean darkening instead of a pattern. */
fn swCanopyTop(ci : i32, cj : i32, ux : f32, uy : f32, res : f32) -> f32 {
  let base = swStandMod(ci, cj, ux, uy);
  if (res <= 0.002) { return base; }
  let tx = ux * kTuftCells;
  let ty = uy * kTuftCells;
  let bx = floor(tx);
  let by = floor(ty);
  let n = swVNoise(ci * i32(kTuftCells) + i32(bx), cj * i32(kTuftCells) + i32(by),
                   tx - bx, ty - by, 0xc7u);
  return clamp(base * (1.0 + res * kTuftAmp * (n * 2.0 - 1.0)), 0.15, 2.4);
}

/* WHAT SHARE OF THE BLADES REACH ABOVE `t` CANOPY TOPS. The template declares a uniform height
 * jitter and this is that declaration read as a profile: the stand does not end in a plane, it
 * thins out over the jitter band, and integrating it from the top down gives the leaf area above a
 * height. Below (1-jit) the profile is exactly the flat-topped (1 - t) the earlier build used, so
 * nothing under the jitter band moved; above it the tallest blades — the ones a nadir view sees —
 * stop sitting at optical depth 0, which is what made them unshadowable by construction. */
fn swTipFrac(t : f32, jit : f32) -> f32 {
  let j = max(jit, 1.0e-3);
  return clamp((1.0 + j - t) / (2.0 * j), 0.0, 1.0);
}
/* Its integral from t upward, in units of one canopy top: leafArea(z) = a * Htop * swAbove(z/Htop). */
fn swAbove(t : f32, jit : f32) -> f32 {
  let j = max(jit, 1.0e-3);
  if (t <= 1.0 - j) { return 1.0 - t; }
  let d = max(1.0 + j - t, 0.0);
  return d * d / (4.0 * j);
}
/* And its inverse, which is how the aggregate finds the height a viewer's own optical depth of one
 * sits at. */
fn swAboveInv(s : f32, jit : f32) -> f32 {
  let j = max(jit, 1.0e-3);
  if (s >= 1.0) { return 0.0; }
  if (s <= j) { return 1.0 + j - 2.0 * sqrt(j * max(s, 0.0)); }
  return 1.0 - s;
}

/* The canopy's own extinction coefficient over a beam at elevation `sEl`, per unit leaf area. */
fn swG(sEl : f32) -> f32 { return kG0 + kG1 * pow(clamp(sEl, 0.0, 1.0), kGp); }

/* THE BEAM'S PATH INSIDE THE STAND, metres, marched over the canopy-top field ALONG THE SUN.
 *
 * This is the whole of the self-shadowing and it is one integral, not a term added to another: the
 * leaf area a beam meets is the profile above, evaluated at the height the beam has climbed to, at
 * the place the beam is over. A point standing at its own patch's top is still in shadow if a patch
 * upsun stands higher than z + r*tan(el) — which the column form could not express at all, because
 * it read only the canopy directly overhead and therefore had no bearing in it.
 *
 * `z` is metres above the ground, `h0` the template's declared sward height (the field's mean),
 * `sunE`/`sunN` the sun's horizontal unit bearing, `cellE`/`cellN` the graticule cell in metres. */
fn swSlantPath(ci : i32, cj : i32, ux : f32, uy : f32, cellE : f32, cellN : f32,
               z : f32, h0 : f32, jit : f32, sunE : f32, sunN : f32, tanEl : f32,
               res : f32) -> f32 {
  let hiM = h0 * kTopMax * (1.0 + jit);
  let rMax = max(hiM - z, 0.0) / max(tanEl, 1.0e-4);
  if (rMax <= 1.0e-4) { return 0.0; }
  let n = max(kSunSteps, 1);
  let dr = rMax / f32(n);
  var acc = 0.0;
  for (var i = 0; i < n; i = i + 1) {
    let r = dr * (f32(i) + 0.5);
    let ht = h0 * swCanopyTop(ci, cj, ux + sunE * r / cellE, uy + sunN * r / cellN, res);
    acc = acc + swTipFrac((z + r * tanEl) / max(ht, 1.0e-4), jit);
  }
  /* ds = dr / cos(el), and cos = 1/sqrt(1 + tan^2). */
  return acc * dr * sqrt(1.0 + tanEl * tanEl);
}

/* The visibility-weighted mean of exp(-kx * leafArea) over the leaf a viewer can still see, which is
 * what an aggregate owes and what evaluating the exponential at the mean depth is not. */
fn swMeanT(kx : f32, kv : f32, lai : f32) -> f32 {
  let s = kx + kv;
  let den = 1.0 - exp(-kv * lai);
  if (den < 1.0e-5) { return 1.0; }
  return kv / s * (1.0 - exp(-s * lai)) / den;
}

/* The same integral WITHOUT a bearing: the column the earlier build carried, kept as the paired
 * control and as the mean the marched version is normalised against. */
fn swColumnPath(z : f32, hTopM : f32, jit : f32, sEl : f32) -> f32 {
  return hTopM * swAbove(z / max(hTopM, 1.0e-4), jit) / max(sEl, kMinSinEl);
}

)";

/* The aggregate: what the stand does to the light when nothing in it covers a pixel any more.
 * Spliced only where litRadiance is in scope. */
static const char *kSwardAggWGSL = R"(
struct SwardAgg { rgb : vec3f, cover : f32, top : f32 };

/* ONE EVALUATION OF THE SAME DECLARATION. Every term below has its counterpart in the blade's own
 * fragment and reads the same constant:
 *   cover     1 - exp(-G(view)/sin(view) * LAI)          the blade's vertical-cover derivation, at
 *                                                        the VIEW's elevation instead of nadir
 *   occ/vis   the two transmittances, weighted by which leaf a viewer can still see
 *   direct    E[max(n.s,0)] = G(el)/2 over an azimuthally uniform leaf population, which is exactly
 *             the G the geometry is built from — so the aggregate cannot light a plant the near
 *             scale does not draw
 *   trans     the same forward lobe the blade carries, because a lobe is a function of the two
 *             directions and of nothing that has a size */
fn swardAggregate(I : Irr, colIn : vec3f, laiDens : f32, hTopM : f32, jit : f32,
                  slantM : f32, nAgg : vec3f, upv : vec3f, sunN : vec3f, vdir : vec3f,
                  sunVis : f32, thruDir : f32, thruMean : f32, night : f32, sEl : f32) -> SwardAgg {
  var o : SwardAgg;
  o.top = hTopM;
  let lai = laiDens * hTopM;
  let sV = clamp(abs(dot(vdir, upv)), kMinSinEl, 1.0);
  let kView = swG(sV) / sV;
  o.cover = 1.0 - exp(-kView * lai);
  /* THE AGGREGATE IS THE MEAN OF A TRANSMITTANCE, NOT THE TRANSMITTANCE OF A MEAN, and the difference
   * is the whole hand-off. The near scale draws every blade at its own depth and the picture averages
   * exp(-k*depth) over them; evaluating the same exponential at the MEAN depth is Jensen's inequality
   * pointing the wrong way and measured 13-24 display codes too dark against the blades it has to
   * meet. The right quantity is the visibility-weighted mean, and for the exponential profile the
   * near scale is built on it is closed:
   *     <T_x> = kv/(kx+kv) * (1 - e^-(kx+kv)L) / (1 - e^-kv L)
   * with kv = G(view)/sin(view), i.e. how deep a viewer sees, and kx the light's own coefficient. */
  let kDiff = kScatCut * kDiffK0;
  /* The direction rides as a RATIO on the column mean, so E[T] is the column's and only its second
   * moment is new: a stationary field has E[marched/column] = 1 by construction. */
  let tStar = swAboveInv(min(1.0 / max(kView * lai, 1.0e-4), 1.0), jit);
  let colM = swColumnPath(tStar * hTopM, hTopM, jit, sEl);
  let dirRatio = select(1.0, clamp(slantM / colM, 0.0, 8.0), colM > 1.0e-5);
  let kBeam = swG(sEl) / max(sEl, kMinSinEl) * dirRatio;
  let tBeam = swMeanT(kBeam, kView, lai);
  let tTot = max(swMeanT(kDiff, kView, lai), tBeam);
  let yw = vec3f(0.2126, 0.7152, 0.0722);
  let sunUpF = max(dot(sunN, upv), 0.0);
  let skyE = dot(I.sky.xyz + I.sunDeck.xyz * (sunUpF * (1.0 - thruMean) * kDeckDiffuse), yw) + night;
  let sunE = dot(I.sun.xyz, yw) * sunUpF * thruDir;
  let occ = clamp((tTot * (skyE + sunE) - sunE * tBeam) / max(skyE, 1.0e-6), tBeam, 1.0);
  let vis = clamp(sunVis * tBeam / max(occ, 1.0e-3), 0.0, 1.0);
  /* THE DIRECT TERM IS THE CANOPY'S AND NOT A SURFACE'S. litRadiance forms it as
   * max(dot(n,sun),0)*sunVis; substituting sunVis -> vis * (G(el)/2) / dot(nAgg,sun) makes that ONE
   * product equal the population's own E[max(n.s,0)]*vis, with no second lighting model and no
   * second constant. The substitution also rides the near-field bounce term inside litRadiance,
   * which is second order and is named in this stage's Gaps. */
  /* HOW THE BEAM SPLITS BETWEEN THE TWO FACES, and it is not half and half — that assumption measured
   * 0.48 EV too dark against the geometry, in a frame that looks INTO an 11 deg sun. The near scale
   * flips every blade's normal toward the camera and then asks whether the sun is on that face or the
   * far one; over the population that is P(n.s > 0 | n.v < 0), and for a spherical normal
   * distribution the two half-spaces cut a lune of solid angle 2(pi - gamma), so
   *     kappa = 1 - gamma/pi,   gamma = angle(sun, fragment->camera)
   * which is 1 with the sun behind the eye and 0 looking into it — the two limits the picture lives
   * between. E[|n.s|] = G(el) is conserved, so the split moves energy between the two terms and never
   * creates any. The population is erectophile and not spherical; that is the approximation, and it
   * is named in this stage's Gaps. */
  let gam = acos(clamp(-dot(sunN, vdir), -1.0, 1.0));
  let kappa = clamp(1.0 - gam * (1.0 / 3.14159265358979), 0.0, 1.0);
  let nds = max(dot(nAgg, sunN), 1.0e-3);
  let visEff = clamp(vis * (swG(sEl) * kappa) / nds, 0.0, 8.0);
  let lit = litRadiance(I, colIn * occ, 1.0, nAgg, upv, sunN, visEff, thruDir, thruMean, night);
  let thru = litRadiance(I, colIn * occ * kLeafTrans, 1.0, -nAgg, upv, sunN, 0.0, 0.0, thruMean, night);
  let lobe = kTransIso + kTransFwd * pow(max(dot(vdir, sunN), 0.0), kTransP);
  let trans = I.sun.xyz * ((swG(sEl) * (1.0 - kappa)) * sunVis * tBeam * thruDir * lobe * kLeafTrans)
            * colIn * (kSceneExposure * kInvPi);
  o.rgb = lit.rgb + thru.rgb + trans;
  return o;
}
)";

} // namespace outshine::Render
#endif
