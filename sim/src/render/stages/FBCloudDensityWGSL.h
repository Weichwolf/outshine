/* The WGSL half of core/FBCloudDensity.h — the SAME formula, transliterated. Two rules keep the two
 * halves from drifting: (a) the numeric constants are not typed here at all, they are emitted from the
 * C++ ones by FBCloudDensityConstsWGSL(); (b) `gpu_native --cloudcheck` evaluates both over a sample set
 * and prints the largest difference. A textual splice, like FBAtmoCommon.h — never compiled alone. */
#ifndef FBCLOUDDENSITYWGSL_H
#define FBCLOUDDENSITYWGSL_H

#include <cstdio>
#include <string>
#include "FBCloudDensity.h"

namespace FlightBox::Render {

/* Mirrors FBCloudDeckParams field for field, in declaration order — the uniform/storage upload is a
 * straight memcpy of the C++ struct, so the two layouts must agree (16 tight f32 = 64 B, which is the
 * stride a UNIFORM array element needs; the two pads at the end are what make it so). */
static const char *kCloudDensityWGSL = R"(
struct CloudDeck {
  /* @align(16): as an array element in a UNIFORM buffer the stride must be a multiple of 16. The 16
   * tight f32 are 64 B already, but the struct's own alignment has to say so. */
  @align(16) baseM : f32, topM : f32, cover : f32,
  driftE : f32, driftN : f32,
  windE : f32, windN : f32,
  stretch : f32, featureM : f32, warp : f32, erosion : f32, sigma : f32,
  remapEdge : f32, remapWidth : f32, erodeFlat : f32, pad1 : f32,
};
fn fbSmooth(e0 : f32, e1 : f32, x : f32) -> f32 {
  let t = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}
fn cdHash2(ix : i32, iy : i32, seed : u32) -> u32 {
  var h : u32 = bitcast<u32>(ix) * 0x27d4eb2du + bitcast<u32>(iy) * 0x9e3779b1u + seed * 0x85ebca6bu;
  h = h ^ (h >> 15u);
  h = h * 0x2c1b3c6du;
  h = h ^ (h >> 12u);
  h = h * 0x297a2d39u;
  h = h ^ (h >> 15u);
  return h;
}
fn cdHash3(ix : i32, iy : i32, iz : i32, seed : u32) -> u32 {
  var h : u32 = bitcast<u32>(ix) * 0x27d4eb2du + bitcast<u32>(iy) * 0x9e3779b1u
              + bitcast<u32>(iz) * 0x165667b1u + seed * 0x85ebca6bu;
  h = h ^ (h >> 15u);
  h = h * 0x2c1b3c6du;
  h = h ^ (h >> 12u);
  h = h * 0x297a2d39u;
  h = h ^ (h >> 15u);
  return h;
}
fn cdFade(t : f32) -> f32 { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
fn cdGrad2(ix : i32, iy : i32, seed : u32, dx : f32, dy : f32) -> f32 {
  let k = cdHash2(ix, iy, seed) & 7u;
  var gx = 0.0;
  var gy = 0.0;
  if (k < 4u) {
    let sgn = select(1.0, -1.0, (k & 1u) != 0u);
    gx = select(sgn, 0.0, (k & 2u) != 0u);
    gy = select(0.0, sgn, (k & 2u) != 0u);
  } else {
    gx = select(0.7071068, -0.7071068, (k & 1u) != 0u);
    gy = select(0.7071068, -0.7071068, (k & 2u) != 0u);
  }
  return gx * dx + gy * dy;
}
fn cdGrad3(ix : i32, iy : i32, iz : i32, seed : u32, dx : f32, dy : f32, dz : f32) -> f32 {
  let k = cdHash3(ix, iy, iz, seed) & 15u;
  let u = select(dy, dx, k < 8u);
  var v = dz;
  if (k < 4u) { v = dy; } else if (k == 12u || k == 14u) { v = dx; }
  let a = select(u, -u, (k & 1u) != 0u);
  let b = select(v, -v, (k & 2u) != 0u);
  return a + b;
}
fn cdNoise2(px : f32, py : f32, seed : u32) -> f32 {
  let fx = floor(px); let fy = floor(py);
  let ix = i32(fx); let iy = i32(fy);
  let dx = px - fx; let dy = py - fy;
  let ux = cdFade(dx); let uy = cdFade(dy);
  let n00 = cdGrad2(ix, iy, seed, dx, dy);
  let n10 = cdGrad2(ix + 1, iy, seed, dx - 1.0, dy);
  let n01 = cdGrad2(ix, iy + 1, seed, dx, dy - 1.0);
  let n11 = cdGrad2(ix + 1, iy + 1, seed, dx - 1.0, dy - 1.0);
  let n = mix(mix(n00, n10, ux), mix(n01, n11, ux), uy);
  return n + 0.5;
}
fn cdNoise3(px : f32, py : f32, pz : f32, seed : u32) -> f32 {
  let fx = floor(px); let fy = floor(py); let fz = floor(pz);
  let ix = i32(fx); let iy = i32(fy); let iz = i32(fz);
  let dx = px - fx; let dy = py - fy; let dz = pz - fz;
  let ux = cdFade(dx); let uy = cdFade(dy); let uz = cdFade(dz);
  let n000 = cdGrad3(ix, iy, iz, seed, dx, dy, dz);
  let n100 = cdGrad3(ix + 1, iy, iz, seed, dx - 1.0, dy, dz);
  let n010 = cdGrad3(ix, iy + 1, iz, seed, dx, dy - 1.0, dz);
  let n110 = cdGrad3(ix + 1, iy + 1, iz, seed, dx - 1.0, dy - 1.0, dz);
  let n001 = cdGrad3(ix, iy, iz + 1, seed, dx, dy, dz - 1.0);
  let n101 = cdGrad3(ix + 1, iy, iz + 1, seed, dx - 1.0, dy, dz - 1.0);
  let n011 = cdGrad3(ix, iy + 1, iz + 1, seed, dx, dy - 1.0, dz - 1.0);
  let n111 = cdGrad3(ix + 1, iy + 1, iz + 1, seed, dx - 1.0, dy - 1.0, dz - 1.0);
  let x00 = mix(n000, n100, ux); let x10 = mix(n010, n110, ux);
  let x01 = mix(n001, n101, ux); let x11 = mix(n011, n111, ux);
  let n = mix(mix(x00, x10, uy), mix(x01, x11, uy), uz);
  return n * 0.8 + 0.5;
}
fn cdFbmShear(ax : f32, bx : f32, seed : u32) -> f32 {
  var qa = ax; var qb = bx;
  var sum = 0.0; var amp = 1.0; var norm = 0.0;
  for (var i = 0; i < kCloudOctaves; i = i + 1) {
    let sa = qa + kCloudShear * f32(i) * qb;
    sum = sum + amp * cdNoise2(sa, qb, seed + u32(i) * 131u);
    norm = norm + amp;
    amp = amp * kCloudGain;
    let ra = qa * kCloudRotC - qb * kCloudRotS;
    let rb = qa * kCloudRotS + qb * kCloudRotC;
    qa = ra * kCloudLacunarity;
    qb = rb * kCloudLacunarity;
  }
  return sum / norm;
}
struct CloudSample { coverage : f32, a : f32, b : f32 };
fn cloudCoverage(d : CloudDeck, eastM : f32, northM : f32) -> CloudSample {
  var s : CloudSample;
  s.coverage = 0.0; s.a = 0.0; s.b = 0.0;
  if (d.cover <= 0.0) { return s; }
  let inv = 1.0 / d.featureM;
  let x = (eastM - d.driftE) * inv;
  let y = (northM - d.driftN) * inv;
  var a = x * d.windE + y * d.windN;
  var b = -x * d.windN + y * d.windE;
  a = a / d.stretch;
  if (d.warp > 0.0) {
    let w1 = cdNoise2(a * kCloudWarpFreq + 17.3, b * kCloudWarpFreq + 5.9, 7u);
    let w2 = cdNoise2(a * kCloudWarpFreq - 3.1, b * kCloudWarpFreq + 23.7, 13u);
    a = a + (w1 - 0.5) * d.warp;
    b = b + (w2 - 0.5) * d.warp * kCloudWarpCross;
  }
  let f = cdFbmShear(a, b, 3u);
  s.coverage = fbSmooth(d.remapEdge - d.remapWidth, d.remapEdge + d.remapWidth, f);
  s.a = a;
  s.b = b;
  return s;
}
fn cloudShape(c : f32, h : f32) -> f32 {
  if (c <= 0.0) { return 0.0; }
  let top = mix(kCloudTopMin, 1.0, c);
  let hn = h / top;
  if (hn >= 1.0) { return 0.0; }
  return c * fbSmooth(0.0, kCloudProfBase, hn) * fbSmooth(1.0, 1.0 - kCloudProfTop, hn);
}
fn cdErosionFbm(ax : f32, bx : f32, h : f32) -> f32 {
  var qa = ax; var qb = bx; var qz = h;
  var sum = 0.0; var amp = 1.0; var norm = 0.0;
  for (var i = 0; i < kCloudErodeOct; i = i + 1) {
    sum = sum + amp * cdNoise3(qa, qb, qz, 41u + u32(i) * 977u);
    norm = norm + amp;
    amp = amp * kCloudGain;
    let ra = qa * kCloudRotC - qb * kCloudRotS;
    let rb = qa * kCloudRotS + qb * kCloudRotC;
    qa = ra * kCloudLacunarity;
    qb = rb * kCloudLacunarity;
    qz = qz * kCloudLacunarity;
  }
  return sum / norm;
}
fn cloudDensity(d : CloudDeck, eastM : f32, northM : f32, h : f32) -> f32 {
  let s = cloudCoverage(d, eastM, northM);
  if (s.coverage <= 0.0) { return 0.0; }
  var dens = cloudShape(s.coverage, h);
  if (d.erosion > 0.0 && dens > 0.0) {
    var e = kCloudErodeMean;
    if (d.erodeFlat < 1.0) {
      let raw = cdErosionFbm(s.a * kCloudErodeFreq, s.b * kCloudErodeFreq, h * kCloudErodeVert);
      e = mix(raw, kCloudErodeMean, d.erodeFlat);
    }
    dens = dens - e * d.erosion * mix(kCloudErodeBase, 1.0, h);
  }
  return clamp(dens, 0.0, 1.0);
}
)";

/* ONE source of truth for the numbers: printed from core/FBCloudDensity.h with enough digits to be
 * exact for a float. Prepended ahead of kCloudDensityWGSL by every consumer. */
inline std::string FBCloudDensityConstsWGSL(void) {
  char buf[1024];
  snprintf(buf, sizeof buf,
           "const kCloudOctaves : i32 = %d;\n"
           "const kCloudLacunarity : f32 = %.9g;\n"
           "const kCloudGain : f32 = %.9g;\n"
           "const kCloudShear : f32 = %.9g;\n"
           "const kCloudRotC : f32 = %.9g;\n"
           "const kCloudRotS : f32 = %.9g;\n"
           "const kCloudWarpFreq : f32 = %.9g;\n"
           "const kCloudWarpCross : f32 = %.9g;\n"
           "const kCloudTopMin : f32 = %.9g;\n"
           "const kCloudErodeOct : i32 = %d;\n"
           "const kCloudProfBase : f32 = %.9g;\n"
           "const kCloudProfTop : f32 = %.9g;\n"
           "const kCloudErodeFreq : f32 = %.9g;\n"
           "const kCloudErodeVert : f32 = %.9g;\n"
           "const kCloudErodeBase : f32 = %.9g;\n"
           "const kCloudErodeMean : f32 = %.9g;\n",
           kCloudOctaves, (double)kCloudLacunarity, (double)kCloudGain, (double)kCloudShear,
           (double)kCloudRotC, (double)kCloudRotS,
           (double)kCloudWarpFreq, (double)kCloudWarpCross,
           (double)kCloudTopMin, kCloudErodeOct, (double)kCloudProfBase, (double)kCloudProfTop,
           (double)kCloudErodeFreq, (double)kCloudErodeVert, (double)kCloudErodeBase,
           (double)kCloudErodeMean);
  return std::string(buf);
}

} // namespace FlightBox::Render
#endif
