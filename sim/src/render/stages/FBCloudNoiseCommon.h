/* Shared WGSL noise helpers for the three bakes and the march. A textual splice, not compiled on
 * its own. */
#ifndef FBCLOUDNOISECOMMON_H
#define FBCLOUDNOISECOMMON_H

namespace FlightBox {

static const char *kCloudNoiseCommon = R"(
fn h33(p : vec3f) -> vec3f {
  let q = vec3f(dot(p, vec3f(127.1, 311.7, 74.7)), dot(p, vec3f(269.5, 183.3, 246.1)),
                dot(p, vec3f(113.5, 271.9, 124.6)));
  return fract(sin(q) * 43758.5453123);
}
fn worley2D(uv2 : vec2f, freq : f32) -> f32 {   /* tileable 2D cellular, 1 at cell centres (= 1 - F1) */
  let p = uv2 * freq;
  let id = floor(p);
  let fr = fract(p);
  var mind = 1.0;
  for (var j = -1; j <= 1; j++) { for (var i = -1; i <= 1; i++) {
    let o = vec2f(f32(i), f32(j));
    var c = id + o;
    c = c - floor(c / freq) * freq;   /* wrap -> seamless tiling */
    let fp = o + h33(vec3f(c, 0.0)).xy;
    let dv = fr - fp;
    mind = min(mind, dot(dv, dv));
  }}
  return 1.0 - sqrt(mind);
}
fn worley(uv : vec3f, freq : f32) -> f32 {   /* tileable cellular noise, 1 at cell centres */
  let p = uv * freq;
  let id = floor(p);
  let fr = fract(p);
  var mind = 1.0;
  for (var k = -1; k <= 1; k++) { for (var j = -1; j <= 1; j++) { for (var i = -1; i <= 1; i++) {
    let o = vec3f(f32(i), f32(j), f32(k));
    var c = id + o;
    c = c - floor(c / freq) * freq;   /* wrap -> seamless tiling */
    let fp = o + h33(c);
    let dv = fr - fp;
    mind = min(mind, dot(dv, dv));
  }}}
  return 1.0 - sqrt(mind);
}
fn worleyFbm(uv : vec3f, f : f32) -> f32 {
  return worley(uv, f) * 0.625 + worley(uv, f * 2.0) * 0.25 + worley(uv, f * 4.0) * 0.125;
}
fn gvec(c0 : vec3f, freq : f32) -> vec3f {
  let c = c0 - floor(c0 / freq) * freq;
  return normalize(h33(c) * 2.0 - 1.0);
}
fn perlin(uv : vec3f, freq : f32) -> f32 {
  let p = uv * freq;
  let id = floor(p);
  let fr = fract(p);
  let u = fr * fr * (3.0 - 2.0 * fr);
  var n = 0.0;
  for (var k = 0; k < 2; k++) { for (var j = 0; j < 2; j++) { for (var i = 0; i < 2; i++) {
    let o = vec3f(f32(i), f32(j), f32(k));
    let g = gvec(id + o, freq);
    let w = mix(1.0 - u.x, u.x, o.x) * mix(1.0 - u.y, u.y, o.y) * mix(1.0 - u.z, u.z, o.z);
    n += dot(g, fr - o) * w;
  }}}
  return n * 0.5 + 0.5;
}
fn perlinFbm(uv : vec3f, f : f32) -> f32 {
  return perlin(uv, f) * 0.55 + perlin(uv, f * 2.0) * 0.30 + perlin(uv, f * 4.0) * 0.15;
}
fn remap(v : f32, a : f32, b : f32, c : f32, d : f32) -> f32 { return c + (v - a) / (b - a) * (d - c); }
)";

} // namespace FlightBox
#endif
