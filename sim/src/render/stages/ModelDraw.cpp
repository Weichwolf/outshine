#include "ModelDraw.h"

#include "ClusterDag.h"
#include "SceneTargets.h"
#include "SceneScale.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this unit. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "ShadowSample.h"
#include "SurfaceLight.h"
#include "SurfaceState.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace outshine::Render {


static const char *kModelWGSL = R"(
const kSheetElements : i32 = 16;
/* The stretch of shoot the elements are rooted along, in units of ONE element's length. [SET] — a
 * short shoot carries its year's growth over about four fifths of an element length. */
const kSheetRoot : f32 = 0.82;
struct M {
  mvp  : mat4x4f,
  ax   : vec4f,   // ECEF east axis  of the instance frame, w = x of the frame's origin from the eye (m)
  ay   : vec4f,   // ECEF north axis of the instance frame, w = y
  az   : vec4f,   // ECEF up axis    of the instance frame, w = z
  sun  : vec4f,   // ECEF sun direction, w = this frame's ambient night floor
  lvl  : vec4f,   // x = model height (m), y = sheet element length (m), z = fan half angle (rad),
                  // w = detail scale (m)
  cut  : vec4f,   // x = this draw's first instance, y = sheets per instance,
                  // z = this level's first sheet, w = octahedral cells per side
  box  : vec4f,   // model box in units of the height: x = half width, y = centre, z = half size
  mat  : array<vec4f, 5>,   // THE MATERIAL ROW, verbatim; the accessors below ARE its field names
};
@group(0) @binding(0) var<uniform> b : M;
@group(0) @binding(1) var<storage, read> I : Irr;
@group(0) @binding(2) var<uniform> C : Csm;
@group(0) @binding(3) var shMap : texture_depth_2d;
@group(0) @binding(4) var shSamp : sampler_comparison;
@group(0) @binding(5) var<storage, read> P : array<f32>;
@group(0) @binding(6) var<storage, read> Sh : array<f32>;
@group(0) @binding(7) var impSamp : sampler;
@group(0) @binding(8) var impAlb : texture_2d<f32>;
@group(0) @binding(9) var impNrm : texture_2d<f32>;
@group(0) @binding(10) var<storage, read> Or : array<u32>;

/* THE MATERIAL ROW'S FIELDS, and this block is the only place they are named. Linear reflectance
 * throughout; `furrowFreq` counts groove cycles per RADIAN of circumference, so a stem a tenth the
 * radius comes out smooth on its own. */
fn solidRgb()        -> vec3f { return b.mat[0].xyz; }
fn solidGroove()     -> f32   { return b.mat[0].w; }
fn furrowFreq()      -> f32   { return b.mat[1].x; }
fn furrowDepth()     -> f32   { return b.mat[1].y; }
fn spineWidth()      -> f32   { return b.mat[1].z; }
fn profileA()        -> f32   { return b.mat[1].w; }
fn sheetRgb()        -> vec3f { return b.mat[2].xyz; }
fn profileB()        -> f32   { return b.mat[2].w; }
fn outlineWidth()    -> f32   { return b.mat[3].x; }
fn outlineBaseFill() -> f32   { return b.mat[3].y; }
fn outlineLobes()    -> f32   { return b.mat[3].z; }
fn outlineLobeDepth()-> f32   { return b.mat[3].w; }
fn outlineSerration()-> f32   { return b.mat[4].x; }
fn profilePeakInv()  -> f32   { return b.mat[4].y; }

/* Model space is y-up and metric once multiplied by the declared height; the instance maps it onto
 * the ECEF triad the whole frame is built in. One place, every net. */
fn frameOrigin() -> vec3f {
  return vec3f(b.ax.w, b.ay.w, b.az.w);
}
/* MODEL Z POINTS SOUTH, and that is not a taste: (east, up, north) is LEFT-handed (east x up =
 * -north), so mapping a right-handed y-up model onto it MIRRORS it and inverts every triangle's
 * winding. Negating one axis makes the map a proper rotation — the mesh keeps its handedness and
 * back-face culling keeps its meaning. */
fn toEcef(local : vec3f) -> vec3f {
  return b.ax.xyz * local.x - b.ay.xyz * local.z + b.az.xyz * local.y;
}
fn yawRot(v : vec3f, cy : f32, sy : f32) -> vec3f {
  return vec3f(v.x * cy - v.z * sy, v.y, v.x * sy + v.z * cy);
}

struct MOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) nrm : vec3f,
              @location(2) loc : vec3f };

/* One instance: xy east/north in metres in the instance frame, z the foot's height over that
 * frame's anchor, w the yaw. b.lvl.x is the model's height and `sf` this instance's factor on it. */
@vertex fn vsSolid(@location(0) p : vec3f, @location(1) n : vec3f,
                   @builtin(instance_index) ii : u32) -> MOut {
  var o : MOut;
  let si = Or[ii + u32(b.cut.x)] * 5u;
  let st = vec4f(P[si], P[si + 1u], P[si + 2u], P[si + 3u]);
  let sf = P[si + 4u];
  let cy = cos(st.w); let sy = sin(st.w);
  let pm = yawRot(p * (b.lvl.x * sf), cy, sy);
  let rel = frameOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + b.az.xyz * st.z + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  let nr = yawRot(n, cy, sy);
  o.nrm = toEcef(nr);
  o.loc = vec3f(nr.x, pm.y, nr.z);   /* the groove reads a circumferential angle and a height, no uv */
  return o;
}

/* THE SHEET ROLLS FREELY ABOUT ITS STALK and that is the same assumption the generator's angle
 * distribution closes G(el) under: axis on the measured direction, normal anywhere on the circle
 * perpendicular to it. A tilt invented here would draw a surface the far level no longer computes. */
fn sheetFrame(dir : vec3f, roll : f32, xA : ptr<function, vec3f>, yA : ptr<function, vec3f>,
              zA : ptr<function, vec3f>) {
  let yAx = normalize(dir);
  let refA = select(vec3f(0.0, 1.0, 0.0), vec3f(1.0, 0.0, 0.0), abs(yAx.y) > 0.9);
  let uAx = normalize(cross(refA, yAx));
  let wAx = cross(yAx, uAx);
  let zAx = uAx * cos(roll) + wAx * sin(roll);
  *yA = yAx;
  *zA = zAx;
  *xA = cross(yAx, zAx);
}

@vertex fn vsDetail(@location(0) v : vec3f, @location(1) n : vec3f,
                    @location(2) ip : vec4f, @location(3) idir : vec4f) -> MOut {
  var o : MOut;
  var xAx : vec3f; var yAx : vec3f; var zAx : vec3f;
  sheetFrame(idir.xyz, ip.w, &xAx, &yAx, &zAx);
  let pm = ip.xyz * b.lvl.x + (xAx * v.x + yAx * v.y + zAx * v.z) * b.lvl.w;
  let nl = xAx * n.x + yAx * n.y + zAx * n.z;
  let rel = frameOrigin() + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  o.nrm = toEcef(nl);
  o.loc = vec3f(0.0);
  return o;
}

/* THE SHEET. Two triangles standing for kSheetElements outlines fanned off one root point: the
 * fragment cuts the declared outline out of the quad, so the silhouette is the declared shape and
 * the cost is two triangles instead of the element's hundred-odd. `loc` carries the sheet's own
 * (u, v). */
struct SOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) nrm : vec3f,
              @location(2) loc : vec3f };

fn quadCorner(vi : u32) -> vec2f {
  var q = vec2f(-1.0, 0.0);
  if (vi == 1u || vi == 3u) { q = vec2f(1.0, 0.0); }
  if (vi == 2u || vi == 4u) { q = vec2f(1.0, 1.0); }
  if (vi == 5u) { q = vec2f(-1.0, 1.0); }
  return q;
}

@vertex fn vsSheet(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> SOut {
  var o : SOut;
  let nc = u32(b.cut.y);
  let ci = (u32(b.cut.z) + ii % nc) * 8u;
  let si = Or[ii / nc + u32(b.cut.x)] * 5u;
  let st = vec4f(P[si], P[si + 1u], P[si + 2u], P[si + 3u]);
  let sf = P[si + 4u];

  let q = quadCorner(vi);

  var xAx : vec3f; var yAx : vec3f; var zAx : vec3f;
  sheetFrame(vec3f(Sh[ci + 4u], Sh[ci + 5u], Sh[ci + 6u]), Sh[ci + 3u], &xAx, &yAx, &zAx);
  /* THE SHEET IS A SHOOT, and its two extents follow from the element rather than from taste: it is
   * as wide as one element swung to the edge of the fan plus that element's own half width, and as
   * long as the stretch of shoot they sit on plus one element. Both in units of its length. */
  let L = b.lvl.y * sf;
  let hw = L * (sin(b.lvl.z) + outlineWidth());
  let hh = L * (kSheetRoot + 1.0);
  let anchor = vec3f(Sh[ci], Sh[ci + 1u], Sh[ci + 2u]) * (b.lvl.x * sf);
  let cyw = cos(st.w); let syw = sin(st.w);
  let pm = yawRot(anchor + xAx * (q.x * hw) + yAx * (q.y * hh), cyw, syw);
  let rel = frameOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + b.az.xyz * st.z + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  o.nrm = toEcef(yawRot(zAx, cyw, syw));
  o.loc = vec3f(q.x * hw / L, q.y * hh / L, 0.0);
  return o;
}

/* The generator's own outline profile, verbatim: the half width at t along the axis, as a fraction
 * of the length. Two languages, ONE curve — a second outline here would draw a shape the subject
 * bench never judged. */
fn profileWidth(t : f32) -> f32 {
  if (spineWidth() > 0.0) {
    var wn = spineWidth() * (1.0 - 0.12 * t);
    if (t > 0.82) { wn = wn * (1.0 - t) / 0.18; }
    return wn;
  }
  var w = pow(t, profileA()) * pow(1.0 - t, profileB()) * profilePeakInv();
  w = w * outlineWidth();
  if (outlineBaseFill() > 0.0) { w = w + outlineBaseFill() * outlineWidth() * exp(-t * 9.0); }
  if (outlineLobes() > 0.0) {
    let lob = 0.5 + 0.5 * cos(6.2831853 * outlineLobes() * t);
    w = w * (1.0 - outlineLobeDepth() * lob);
  }
  if (outlineSerration() > 0.0) {
    let ff = select(7.0, outlineLobes(), outlineLobes() > 0.0) * 2.0;
    let saw = abs(2.0 * (t * ff - floor(t * ff + 0.5)));
    w = w * (1.0 - outlineSerration() * 0.45 * saw);
  }
  return w;
}

/* THE GROOVE, and its metric is the declared one: `furrowFreq` counts cycles per RADIAN of
 * circumference, so the pitch on a 0.4 m radius is 0.4/f metres. The second sine breaks the comb
 * into ridges of unequal width; the meander with height keeps them from being drawn lines. */
fn furrow(nlx : f32, nlz : f32, y : f32, f : f32) -> f32 {
  let ang = atan2(nlz, nlx) + 0.22 * sin(y * 2.1) + 0.11 * sin(y * 5.3 + 1.3);
  let s1 = sin(ang * f) * 0.5 + 0.5;
  let s2 = sin(ang * f * 2.37 + 1.7) * 0.5 + 0.5;
  return pow(mix(s1, s1 * s2, 0.55), 1.4);
}

@fragment fn fsSolid(in : MOut) -> @location(0) vec4f {
  let upB = normalize(b.az.xyz);
  let sunB = normalize(b.sun.xyz);
  let nB = normalize(in.nrm);
  /* A groove runs ALONG the axis, so a face looking down the axis has none. The local normal's
   * radial length is that fade and it also keeps atan2 out of its degenerate direction. */
  let radial = length(vec2f(in.loc.x, in.loc.z));
  let fr = furrow(in.loc.x, in.loc.z, in.loc.y, furrowFreq()) * radial;
  let alb = solidRgb() * mix(solidGroove(), 1.0, mix(1.0, fr, furrowDepth()));
  let sunVis = csmSunVis(shMap, shSamp, C, in.rel, upB, sunB);
  return litRadiance(I, alb, 1.0, nB, upB, sunB, sunVis,
                     1.0, 1.0, b.sun.w);
}

/* THE ONE TERM AN OPAQUE BRDF HAS NO PLACE FOR. A sheet declares no thickness, so what it does not
 * reflect it passes: the transmittance IS the albedo and this introduces no constant. It fires only
 * when sun and eye stand on opposite faces, which is the whole of what backlighting is. */
fn shadeSheet(rel : vec3f, nrm : vec3f, alb : vec3f) -> vec4f {
  let upB = normalize(b.az.xyz);
  let sunB = normalize(b.sun.xyz);
  let n0 = normalize(nrm);
  let vB = normalize(-rel);
  let viewSide = dot(n0, vB);
  let nB = select(-n0, n0, viewSide >= 0.0);
  let sunVis = csmSunVis(shMap, shSamp, C, rel, upB, sunB);
  let lit = litRadiance(I, alb, 1.0, nB, upB, sunB, sunVis, 1.0, 1.0, b.sun.w);

  let sunSide = dot(n0, sunB);
  let through = select(0.0, abs(sunSide), sunSide * viewSide < 0.0);
  let trans = alb * (kSceneExposure * kInvPi) * I.sun.xyz * (through * sunVis);

  let yw = vec3f(0.2126, 0.7152, 0.0722);
  let rgb = lit.rgb + trans;
  let outY = dot(rgb, yw);
  let dirY = dot(lit.rgb, yw) * lit.a + dot(trans, yw);
  return vec4f(rgb, select(1.0, clamp(dirY / outY, 0.0, 1.0), outY > 1.0e-9));
}

fn elementShade(rel : vec3f, nrm : vec3f) -> vec4f { return shadeSheet(rel, nrm, sheetRgb()); }

@fragment fn fsDetail(in : MOut) -> @location(0) vec4f {
  return elementShade(in.rel, in.nrm);
}

/* The sheet's own cut-out: kSheetElements outlines rooted along the shoot, sides alternating and the
 * fan angle stepped by the golden fraction so no two neighbours lie on top of each other. */
fn sheetHit(px : f32, py : f32) -> bool {
  for (var k = 0; k < kSheetElements; k = k + 1) {
    let fk = f32(k);
    let base = kSheetRoot * (fk + 0.5) / f32(kSheetElements);
    let a = b.lvl.z * (2.0 * fract(fk * 0.6180339 + 0.13) - 1.0);
    let ca = cos(a); let sa = sin(a);
    let dy = py - base;
    let t = px * sa + dy * ca;
    let s = px * ca - dy * sa;
    if (t > 0.02 && t < 1.0 && abs(s) < profileWidth(t)) { return true; }
  }
  return false;
}

@fragment fn fsSheet(in : SOut) -> @location(0) vec4f {
  let hit = sheetHit(in.loc.x, in.loc.y);
  /* Shaded BEFORE the cut, not after: `discard` inside a branch would put the shadow comparison in
   * non-uniform control flow and the module would not compile. */
  let c = elementShade(in.rel, in.nrm);
  if (!hit) { discard; }
  return c;
}

/* ---- the impostor: bake, then draw ---- */

struct BOut { @location(0) alb : vec4f, @location(1) nrm : vec4f };

@fragment fn fsSolidBake(in : MOut) -> BOut {
  var o : BOut;
  let radial = length(vec2f(in.loc.x, in.loc.z));
  let fr = furrow(in.loc.x, in.loc.z, in.loc.y, furrowFreq()) * radial;
  o.alb = vec4f(solidRgb() * mix(solidGroove(), 1.0, mix(1.0, fr, furrowDepth())), 1.0);
  o.nrm = vec4f(normalize(in.nrm) * 0.5 + 0.5, 1.0);
  return o;
}

@fragment fn fsSheetBake(in : SOut) -> BOut {
  var o : BOut;
  o.alb = vec4f(sheetRgb(), 1.0);
  o.nrm = vec4f(normalize(in.nrm) * 0.5 + 0.5, 1.0);
  if (!sheetHit(in.loc.x, in.loc.y)) { discard; }
  return o;
}

struct IOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) uv : vec2f,
              @location(2) vdl : vec3f, @location(3) yw : vec2f };

@vertex fn vsImp(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> IOut {
  var o : IOut;
  let si = Or[ii + u32(b.cut.x)] * 5u;
  let st = vec4f(P[si], P[si + 1u], P[si + 2u], P[si + 3u]);
  let sf = P[si + 4u];
  let h = b.lvl.x * sf;
  let hs = b.box.z * h;
  let foot = frameOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + b.az.xyz * st.z;
  let ctr = foot + b.az.xyz * (b.box.y * h);
  let vd = normalize(-ctr);
  var right = cross(b.az.xyz, vd);
  let rl = length(right);
  right = select(b.ax.xyz, right / max(rl, 1.0e-6), rl > 1.0e-4);
  let q = quadCorner(vi);
  let wp = ctr + right * (q.x * hs) + b.az.xyz * ((q.y * 2.0 - 1.0) * hs);
  o.pos = b.mvp * vec4f(wp, 1.0);
  o.rel = wp;
  /* v = 0 is the atlas's TOP row and the bake put the top there, so the quad's top edge reads 0. */
  o.uv = vec2f(q.x * 0.5 + 0.5, 1.0 - q.y);
  o.yw = vec2f(cos(st.w), sin(st.w));
  let ve = vec3f(dot(vd, b.ax.xyz), dot(vd, b.az.xyz), -dot(vd, b.ay.xyz));
  o.vdl = yawRot(ve, o.yw.x, -o.yw.y);
  return o;
}

fn hemiOctEnc(nin : vec3f) -> vec2f {
  let n = nin / (abs(nin.x) + abs(nin.y) + abs(nin.z));
  return vec2f(n.x + n.z, n.x - n.z) * 0.5 + 0.5;
}

@fragment fn fsImp(in : IOut) -> @location(0) vec4f {
  var vd = normalize(in.vdl);
  vd.y = max(vd.y, 0.03);
  let N = b.cut.w;
  let oc = clamp(hemiOctEnc(normalize(vd)), vec2f(0.0), vec2f(0.999));
  let cell = floor(oc * N);
  /* Inset by half a texel plus a guard: a bilinear tap at a cell's edge would otherwise reach into
   * the neighbouring view and hang a sliver of another silhouette on the model. */
  let uv = (cell + clamp(in.uv, vec2f(0.006), vec2f(0.994))) / N;
  let t = textureSampleLevel(impAlb, impSamp, uv, 0.0);
  let nt = textureSampleLevel(impNrm, impSamp, uv, 0.0).xyz * 2.0 - 1.0;
  let nE = toEcef(yawRot(nt, in.yw.x, in.yw.y));
  let c = shadeSheet(in.rel, nE, t.rgb);
  if (t.a < 0.35) { discard; }
  return c;
}
)";

namespace {

/* THE DERIVATION TABLE PICKS THE STATE, here and not by opinion. The solid part is opaque over a
 * mesh whose winding a generator grew and can be trusted; a sheet declares transmission, which is
 * exactly what says it has no back to cull. */
constexpr Material kSolidMaterial{};
constexpr Material kSheetMaterial{{0.5f, 0.5f, 0.5f}, 1.0f, 1.0f, 0.5f, 1.0f, {0.0f, 0.0f, 0.0f}};
constexpr SurfaceState kSolidState = StateOf(kSolidMaterial);
constexpr SurfaceState kSheetState = StateOf(kSheetMaterial);
static_assert(kSolidState.CullsBack(), "a solid model culls its back faces");
static_assert(kSheetState.Kind() == SurfaceKind::ThinTransmissive, "a sheet is lit on both sides");

wgpu::CullMode Facing(const SurfaceState &state, Winding winding) {
  return CullsBackFaces(state, winding) ? wgpu::CullMode::Back : wgpu::CullMode::None;
}

}  // namespace

void ModelDraw::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + ShadowSampleWGSL()
                        + kModelWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(false)};

  /* EXPLICIT, because four pipelines share one bind group: a default layout belongs to the pipeline
   * that produced it, and a group built from the solid's would be rejected by the sheet's. */
  wgpu::BindGroupLayoutEntry ble[11] = {};
  ble[0].binding = 0;
  ble[0].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  ble[0].buffer.type = wgpu::BufferBindingType::Uniform;
  ble[0].buffer.minBindingSize = kUniFloats * sizeof(float);
  /* DYNAMIC, and only the bake needs it: sixty-four cells are recorded into ONE pass and submitted
   * once, so a Queue.WriteBuffer per cell would land before the whole command buffer and every cell
   * would draw the last matrix. One slot per cell in one buffer is the only ordering that holds. */
  ble[0].buffer.hasDynamicOffset = true;
  ble[1].binding = 1;
  ble[1].visibility = wgpu::ShaderStage::Fragment;
  ble[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
  ble[2].binding = 2;
  ble[2].visibility = wgpu::ShaderStage::Fragment;
  ble[2].buffer.type = wgpu::BufferBindingType::Uniform;
  ble[3].binding = 3;
  ble[3].visibility = wgpu::ShaderStage::Fragment;
  ble[3].texture.sampleType = wgpu::TextureSampleType::Depth;
  ble[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  ble[4].binding = 4;
  ble[4].visibility = wgpu::ShaderStage::Fragment;
  ble[4].sampler.type = wgpu::SamplerBindingType::Comparison;
  ble[5].binding = 5;
  ble[5].visibility = wgpu::ShaderStage::Vertex;
  ble[5].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
  ble[6].binding = 6;
  ble[6].visibility = wgpu::ShaderStage::Vertex;
  ble[6].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
  ble[7].binding = 7;
  ble[7].visibility = wgpu::ShaderStage::Fragment;
  ble[7].sampler.type = wgpu::SamplerBindingType::Filtering;
  ble[8].binding = 8;
  ble[8].visibility = wgpu::ShaderStage::Fragment;
  ble[8].texture.sampleType = wgpu::TextureSampleType::Float;
  ble[8].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  ble[9].binding = 9;
  ble[9].visibility = wgpu::ShaderStage::Fragment;
  ble[9].texture.sampleType = wgpu::TextureSampleType::Float;
  ble[9].texture.viewDimension = wgpu::TextureViewDimension::e2D;
  ble[10].binding = 10;
  ble[10].visibility = wgpu::ShaderStage::Vertex;
  ble[10].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
  wgpu::BindGroupLayoutDescriptor bgld{};
  bgld.entryCount = 11;
  bgld.entries = ble;
  Bgl = Device.CreateBindGroupLayout(&bgld);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &Bgl;
  wgpu::PipelineLayout pl = Device.CreatePipelineLayout(&pld);

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = kSolidState.WritesDepth();
  ds.depthCompare = wgpu::CompareFunction::Greater;   /* reversed-Z, as every scene-pass surface */

  wgpu::VertexAttribute solidAttr[2] = {};
  solidAttr[0].format = wgpu::VertexFormat::Float32x3; solidAttr[0].offset = 0;  solidAttr[0].shaderLocation = 0;
  solidAttr[1].format = wgpu::VertexFormat::Float32x3; solidAttr[1].offset = 12; solidAttr[1].shaderLocation = 1;
  wgpu::VertexBufferLayout solidBuf{};
  solidBuf.arrayStride = kSolidFloats * sizeof(float);
  solidBuf.attributeCount = 2;
  solidBuf.attributes = solidAttr;

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = pl;
  rp.vertex.module = m;
  rp.vertex.entryPoint = "vsSolid";
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &solidBuf;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.entryPoint = "fsSolid";
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  rp.primitive.cullMode = Facing(kSolidState, Winding::Trusted);
  SolidPipe = Device.CreateRenderPipeline(&rp);

  wgpu::VertexAttribute detailAttr[2] = {};
  detailAttr[0].format = wgpu::VertexFormat::Float32x3; detailAttr[0].offset = 0;  detailAttr[0].shaderLocation = 0;
  detailAttr[1].format = wgpu::VertexFormat::Float32x3; detailAttr[1].offset = 12; detailAttr[1].shaderLocation = 1;
  wgpu::VertexAttribute instAttr[2] = {};
  instAttr[0].format = wgpu::VertexFormat::Float32x4; instAttr[0].offset = 0;  instAttr[0].shaderLocation = 2;
  instAttr[1].format = wgpu::VertexFormat::Float32x4; instAttr[1].offset = 16; instAttr[1].shaderLocation = 3;
  wgpu::VertexBufferLayout detailBufs[2] = {};
  detailBufs[0].arrayStride = kDetailFloats * sizeof(float);
  detailBufs[0].attributeCount = 2;
  detailBufs[0].attributes = detailAttr;
  detailBufs[1].arrayStride = kInstFloats * sizeof(float);
  detailBufs[1].stepMode = wgpu::VertexStepMode::Instance;
  detailBufs[1].attributeCount = 2;
  detailBufs[1].attributes = instAttr;

  rp.vertex.entryPoint = "vsDetail";
  rp.vertex.bufferCount = 2;
  rp.vertex.buffers = detailBufs;
  fs.entryPoint = "fsDetail";
  rp.primitive.cullMode = Facing(kSheetState, Winding::Trusted);
  DetailPipe = Device.CreateRenderPipeline(&rp);

  rp.vertex.entryPoint = "vsSheet";
  rp.vertex.bufferCount = 0;
  rp.vertex.buffers = nullptr;
  fs.entryPoint = "fsSheet";
  SheetPipe = Device.CreateRenderPipeline(&rp);

  rp.vertex.entryPoint = "vsImp";
  fs.entryPoint = "fsImp";
  ImpPipe = Device.CreateRenderPipeline(&rp);

  /* THE BAKE'S OWN TARGETS: albedo with coverage in alpha, and the model-space normal. Two 8-bit
   * surfaces and no velocity — the atlas is a picture of a mesh, not a frame. */
  wgpu::ColorTargetState bct[2] = {};
  bct[0].format = wgpu::TextureFormat::RGBA8Unorm;
  bct[1].format = wgpu::TextureFormat::RGBA8Unorm;
  fs.targetCount = 2;
  fs.targets = bct;
  rp.vertex.entryPoint = "vsSolid";
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &solidBuf;
  fs.entryPoint = "fsSolidBake";
  rp.primitive.cullMode = Facing(kSolidState, Winding::Trusted);
  SolidBakePipe = Device.CreateRenderPipeline(&rp);
  rp.vertex.entryPoint = "vsSheet";
  rp.vertex.bufferCount = 0;
  rp.vertex.buffers = nullptr;
  fs.entryPoint = "fsSheetBake";
  rp.primitive.cullMode = Facing(kSheetState, Winding::Trusted);
  SheetBakePipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = (kLevels + 1) * kUniFloats * sizeof(float);   /* one slot per level plus the impostor */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::SamplerDescriptor sd{};
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  ImpSamp = Device.CreateSampler(&sd);

  /* The bind group is built once and its texture slots have to be filled at that moment, so the
   * pre-bake state is a real 1x1 rather than a null the layout would reject. */
  wgpu::TextureDescriptor td{};
  td.size = {1, 1, 1};
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
  ImpAlbedoView = Device.CreateTexture(&td).CreateView();
  ImpNormalView = Device.CreateTexture(&td).CreateView();

  /* A bind group is built once and the two storage slots must be filled at that moment, so the empty
   * state is a real one-element buffer rather than a null the layout would reject. */
  const float onePlace[8] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
  PlaceBuf = Upload(onePlace, sizeof onePlace, wgpu::BufferUsage::Storage);
  SheetBuf = Upload(onePlace, sizeof onePlace, wgpu::BufferUsage::Storage);
  const uint32_t firstPlace[1] = {0u};
  OrderBuf = Upload(firstPlace, sizeof firstPlace, wgpu::BufferUsage::Storage);
  Rebind();
}

void ModelDraw::Rebind() {
  if (!Bgl) return;
  wgpu::BindGroupEntry be[11] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = Light.Irradiance; be[1].size = wgpu::kWholeSize;
  be[2].binding = 2; be[2].buffer = Light.Cascades;   be[2].size = kShadowUniFloats * sizeof(float);
  be[3].binding = 3; be[3].textureView = Light.ShadowAtlas;
  be[4].binding = 4; be[4].sampler = Light.ShadowCompare;
  be[5].binding = 5; be[5].buffer = PlaceBuf;  be[5].size = wgpu::kWholeSize;
  be[6].binding = 6; be[6].buffer = SheetBuf;  be[6].size = wgpu::kWholeSize;
  be[7].binding = 7; be[7].sampler = ImpSamp;
  be[8].binding = 8; be[8].textureView = ImpAlbedoView;
  be[9].binding = 9; be[9].textureView = ImpNormalView;
  be[10].binding = 10; be[10].buffer = OrderBuf; be[10].size = wgpu::kWholeSize;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Bgl;
  bg.entryCount = 11;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
  if (BakeUni) {
    be[0].buffer = BakeUni;
    BakeBind = Device.CreateBindGroup(&bg);
  }
}

void ModelDraw::SetBounds(const ModelBounds &bounds) { Bounds = bounds; }

namespace {

/* The hemi-octahedral cell's own direction, the inverse of the WGSL `hemiOctEnc`. */
void HemiOctDecode(float u, float v, float d[3]) {
  const float fx = u * 2.0f - 1.0f, fy = v * 2.0f - 1.0f;
  const float nx = (fx + fy) * 0.5f, nz = (fx - fy) * 0.5f;
  const float ny = 1.0f - std::fabs(nx) - std::fabs(nz);
  const float l = std::sqrt(nx * nx + ny * ny + nz * nz);
  d[0] = nx / l; d[1] = ny / l; d[2] = nz / l;
}

/* Orthographic, reversed-Z, looking at (0, ctrY, 0) from `dir` — the same depth convention every
 * scene surface uses, so the bake shares the pipelines' depth state instead of needing its own. */
void BakeMvp(const float dir[3], float ctrY, float hs, float out[16]) {
  const bool polar = std::fabs(dir[1]) > 0.95f;
  const float up[3] = {0.0f, polar ? 0.0f : 1.0f, polar ? 1.0f : 0.0f};
  const float z[3] = {dir[0], dir[1], dir[2]};
  float x[3] = {up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]};
  const float xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
  for (int i = 0; i < 3; i++) x[i] /= xl;
  const float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
  const float r = hs * 3.0f;
  const float eye[3] = {z[0] * r, ctrY + z[1] * r, z[2] * r};
  const float zn = r - hs * 1.5f, zf = r + hs * 1.5f, dz = zf - zn;
  const float ex = eye[0] * x[0] + eye[1] * x[1] + eye[2] * x[2];
  const float ey = eye[0] * y[0] + eye[1] * y[1] + eye[2] * y[2];
  const float ez = eye[0] * z[0] + eye[1] * z[1] + eye[2] * z[2];
  const float row[4][4] = {{x[0] / hs, x[1] / hs, x[2] / hs, -ex / hs},
                           {y[0] / hs, y[1] / hs, y[2] / hs, -ey / hs},
                           {z[0] / dz, z[1] / dz, z[2] / dz, (zf - ez) / dz},
                           {0.0f, 0.0f, 0.0f, 1.0f}};
  for (int c = 0; c < 4; c++) {
    for (int rr = 0; rr < 4; rr++) { out[c * 4 + rr] = row[rr][c]; }
  }
}

} // namespace

wgpu::Buffer ModelDraw::Upload(const void *data, size_t bytes, wgpu::BufferUsage usage) {
  wgpu::BufferDescriptor bd{};
  bd.size = (bytes + 3u) & ~size_t(3);
  bd.usage = usage | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer b = Device.CreateBuffer(&bd);
  Queue.WriteBuffer(b, 0, data, bytes);
  return b;
}

size_t ModelDraw::PrototypeBytes() const {
  size_t b = DetailBytes_ + SheetBytes_;
  for (int k = 0; k < kLevels; k++) b += LevelBytes_[k];
  return b;
}

void ModelDraw::SetLevel(int level, const LevelMesh &mesh) {
  if (level < 0 || level >= kLevels) return;
  LevelIdxCount[level] = 0;
  LevelBytes_[level] = 0;
  if (!Device || !mesh.Verts || mesh.VertCount == 0 || mesh.IdxCount == 0) return;
  LevelVtx[level] = Upload(mesh.Verts, (size_t)mesh.VertCount * kSolidFloats * sizeof(float),
                           wgpu::BufferUsage::Vertex);
  LevelIdx[level] = Upload(mesh.Idx, (size_t)mesh.IdxCount * sizeof(uint32_t),
                           wgpu::BufferUsage::Index);
  LevelIdxCount[level] = mesh.IdxCount;
  LevelBytes_[level] = (size_t)mesh.VertCount * kSolidFloats * sizeof(float) +
                       (size_t)mesh.IdxCount * sizeof(uint32_t);
}

void ModelDraw::SetDetail(const DetailMesh &detail) {
  DetailIdxCount = 0;
  DetailInstCount = 0;
  if (!Device || !detail.Verts || detail.VertCount == 0 || detail.IdxCount == 0 ||
      !detail.Instances || detail.InstanceCount == 0)
    return;
  DetailVtx = Upload(detail.Verts, (size_t)detail.VertCount * kDetailFloats * sizeof(float),
                     wgpu::BufferUsage::Vertex);
  DetailIdx = Upload(detail.Idx, (size_t)detail.IdxCount * sizeof(uint32_t),
                     wgpu::BufferUsage::Index);
  DetailInst = Upload(detail.Instances, (size_t)detail.InstanceCount * kInstFloats * sizeof(float),
                      wgpu::BufferUsage::Vertex);
  DetailIdxCount = detail.IdxCount;
  DetailInstCount = detail.InstanceCount;
  DetailScaleM = detail.ScaleM;
  DetailBytes_ = (size_t)detail.VertCount * kDetailFloats * sizeof(float) +
                 (size_t)detail.IdxCount * sizeof(uint32_t) +
                 (size_t)detail.InstanceCount * kInstFloats * sizeof(float);
}

/* All levels live in ONE storage buffer with a per-level base, because the bind group is built once
 * and a second sheet buffer would mean a second group for a draw that differs in a single integer. */
void ModelDraw::SetSheets(int level, const SheetSet &sheets) {
  if (level < 0 || level >= kLevels) return;
  const bool have = sheets.Sheets && sheets.Count;
  SheetStage[level].assign(have ? sheets.Sheets : nullptr,
                           have ? sheets.Sheets + (size_t)sheets.Count * kInstFloats : nullptr);
  SheetSizeM[level] = sheets.SizeM;
  SheetFanDeg = sheets.FanDeg;
  if (!Device) return;
  std::vector<float> all;
  uint32_t base = 0;
  for (int k = 0; k < kLevels; k++) {
    SheetBase[k] = base;
    SheetCount[k] = (uint32_t)(SheetStage[k].size() / kInstFloats);
    all.insert(all.end(), SheetStage[k].begin(), SheetStage[k].end());
    base += SheetCount[k];
  }
  if (all.empty()) return;
  SheetBuf = Upload(all.data(), all.size() * sizeof(float), wgpu::BufferUsage::Storage);
  SheetBytes_ = all.size() * sizeof(float);
  Rebind();
}

void ModelDraw::SetMaterial(const float row[kMaterialRowFloats]) {
  std::memcpy(MaterialRow, row, sizeof MaterialRow);
}

void ModelDraw::SetHeightM(double heightM) { HeightM = heightM; }

void ModelDraw::SetSubject(double eastM, double northM, double eyeAglM) {
  EastM = eastM;
  NorthM = northM;
  EyeAglM = eyeAglM;
}

void ModelDraw::SetInstances(const float *instances, uint32_t n, const TangentFrame &frame) {
  PlaceCount = 0;
  InstanceBytes_ = 0;
  Places.clear();
  Order.clear();
  if (n == 0 || !Device || !instances) return;
  Places.assign(instances, instances + (size_t)n * kPlaceFloats);
  Order.resize(n);
  PlaceBuf = Upload(instances, (size_t)n * kPlaceFloats * sizeof(float), wgpu::BufferUsage::Storage);
  OrderBuf = Upload(Order.data(), Order.size() * sizeof(uint32_t), wgpu::BufferUsage::Storage);
  Frame = frame;
  PlaceCount = n;
  InstanceBytes_ = (size_t)n * kPlaceFloats * sizeof(float) + Order.size() * sizeof(uint32_t);
  Rebind();
}

void ModelDraw::CreateImpostor() {
  if (!Device || LevelIdxCount[0] == 0) return;
  const uint32_t side = (uint32_t)(kCells * kCellSize);
  wgpu::TextureDescriptor td{};
  td.size = {side, side, 1};
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
  ImpAlbedo = Device.CreateTexture(&td);
  ImpNormal = Device.CreateTexture(&td);
  td.format = wgpu::TextureFormat::Depth32Float;
  td.usage = wgpu::TextureUsage::RenderAttachment;
  ImpDepth = Device.CreateTexture(&td);
  ImpDepthView = ImpDepth.CreateView();

  wgpu::BufferDescriptor bd{};
  bd.size = (size_t)(kCells * kCells) * kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  BakeUni = Device.CreateBuffer(&bd);
  /* Two RGBA8 atlases and, until the bake is finished, a depth target of the same extent. */
  ImpostorBytes_ = (size_t)side * (size_t)side * (4u + 4u + 4u) + (size_t)bd.size;
  Rebind();
}

void ModelDraw::EncodeBake(wgpu::RenderPassEncoder &pass) {
  if (!ImpAlbedo || !SolidBakePipe) return;
  /* The model's own axes, so `toEcef` is the identity and the bake sees the mesh in the frame it was
   * grown in: east = +x, up = +y, and north = -z because model z points south. */
  const float hs = std::fmax(Bounds.HalfWidth, (Bounds.Top - Bounds.Bottom) * 0.5f) * (float)HeightM;
  const float ctrY = (Bounds.Top + Bounds.Bottom) * 0.5f * (float)HeightM;
  float u[kUniFloats] = {};
  u[kUniFrame + 0] = 1.0f;                    /* ax = (1, 0, 0) */
  u[kUniFrame + 6] = -1.0f;                   /* ay = (0, 0, -1) */
  u[kUniFrame + 9] = 1.0f;                    /* az = (0, 1, 0) */
  u[kUniLevel + 0] = (float)HeightM;
  u[kUniLevel + 1] = SheetSizeM[0];
  u[kUniLevel + 2] = SheetFanDeg * 0.017453292f * 0.5f;
  u[kUniLevel + 3] = DetailScaleM;
  u[kUniCut + 1] = (float)SheetCount[0];
  u[kUniCut + 2] = (float)SheetBase[0];
  std::memcpy(&u[kUniMaterial], MaterialRow, sizeof MaterialRow);

  for (int j = 0; j < kCells; j++) {
    for (int i = 0; i < kCells; i++) {
      float dir[3];
      HemiOctDecode(((float)i + 0.5f) / (float)kCells, ((float)j + 0.5f) / (float)kCells, dir);
      BakeMvp(dir, ctrY, hs, u);
      const uint32_t off = (uint32_t)(j * kCells + i) * (uint32_t)(kUniFloats * sizeof(float));
      Queue.WriteBuffer(BakeUni, off, u, sizeof u);
      pass.SetViewport((float)(i * kCellSize), (float)(j * kCellSize), (float)kCellSize,
                       (float)kCellSize, 0.0f, 1.0f);
      pass.SetPipeline(SolidBakePipe);
      pass.SetBindGroup(0, BakeBind, 1, &off);
      pass.SetVertexBuffer(0, LevelVtx[0]);
      pass.SetIndexBuffer(LevelIdx[0], wgpu::IndexFormat::Uint32);
      pass.DrawIndexed(LevelIdxCount[0], 1);
      if (SheetCount[0] > 0) {
        pass.SetPipeline(SheetBakePipe);
        pass.SetBindGroup(0, BakeBind, 1, &off);
        pass.Draw(6, SheetCount[0]);
      }
    }
  }
}

void ModelDraw::FinishBake() {
  const size_t side = (size_t)(kCells * kCellSize);
  ImpostorBytes_ -= side * side * 4u;
  ImpDepth = nullptr;
  ImpDepthView = nullptr;
  ImpAlbedoView = ImpAlbedo.CreateView();
  ImpNormalView = ImpNormal.CreateView();
  Rebind();
}

void ModelDraw::SetSun(const double sunEcef[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) SunDir[i] = sunEcef[i];
  NightAmbient = nightAmbient;
}

/* THE FRAME THE INSTANCES ARE MEASURED IN, and it is a place and not the eye: a field anchored on
 * the camera would travel with it. The lone subject has no frame of its own and keeps the eye's. */
void ModelDraw::Aim(const FrameContext &ctx, double east[3], double north[3], double up[3],
                    double origin[3]) const {
  if (PlaceCount > 0) {
    for (int a = 0; a < 3; a++) {
      east[a] = Frame.EastEcef()[a];
      north[a] = Frame.NorthEcef()[a];
      origin[a] = Frame.OriginEcef()[a] - ctx.Eye[a];
    }
    up[0] = east[1] * north[2] - east[2] * north[1];
    up[1] = east[2] * north[0] - east[0] * north[2];
    up[2] = east[0] * north[1] - east[1] * north[0];
    return;
  }
  for (int a = 0; a < 3; a++) up[a] = ctx.Up[a];
  east[0] = -up[1]; east[1] = up[0]; east[2] = 0.0;   /* z_ecef x up, before normalising */
  double el = std::sqrt(east[0] * east[0] + east[1] * east[1]);
  if (el < 1.0e-12) { east[0] = 1.0; east[1] = 0.0; el = 1.0; }
  for (int a = 0; a < 3; a++) east[a] /= el;
  north[0] = up[1] * east[2] - up[2] * east[1];
  north[1] = up[2] * east[0] - up[0] * east[2];
  north[2] = up[0] * east[1] - up[1] * east[0];
  for (int a = 0; a < 3; a++)
    origin[a] = east[a] * EastM + north[a] * NorthM - up[a] * EyeAglM;
}

/* Which instance lands on which level is decided HERE, every frame, out of THIS frame's eye — the
 * order the instances arrived in states nothing about distance and must not. The ladder is
 * `DagSelect` for a point body, so the comparison is squared distances against squared edges and no
 * root is taken over the field. */
uint32_t ModelDraw::Sort(const ClusterCut &cut, const double east[3], const double north[3],
                         const double up[3], const double origin[3], LevelCut cuts[kLevels]) {
  const float tau = SseTauPx();
  double edgeSq[kLevels];
  for (int k = 0; k < kLevels; k++)
    edgeSq[k] = DagEdgeSq(HeightM * (double)ModelLadder::Error(k + 1), cut.PixelFocal(), tau);
  uint32_t into[kLevels + 1] = {};
  LevelOf.resize(PlaceCount);
  for (uint32_t s = 0; s < PlaceCount; s++) {
    const float *p = &Places[(size_t)s * kPlaceFloats];
    double d[3];
    for (int a = 0; a < 3; a++)
      d[a] = origin[a] + east[a] * (double)p[0] + north[a] * (double)p[1] + up[a] * (double)p[2];
    const double distSq = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    int k = 0;
    while (k < kLevels && distSq > edgeSq[k]) k++;
    LevelOf[s] = (uint8_t)k;
    into[k]++;
  }
  uint32_t at = 0;
  for (int k = 0; k <= kLevels; k++) {
    const uint32_t n = into[k];
    into[k] = at;
    at += n;
    if (k < kLevels) { cuts[k].First = into[k]; cuts[k].Count = n; }
  }
  const uint32_t meshN = into[kLevels];
  for (uint32_t s = 0; s < PlaceCount; s++) Order[into[LevelOf[s]]++] = s;
  Queue.WriteBuffer(OrderBuf, 0, Order.data(), Order.size() * sizeof(uint32_t));
  return meshN;
}

/* ONE SLOT PER LEVEL, ALL WRITTEN BEFORE ANY DRAW. A queue write is ordered against the SUBMIT, not
 * against the draw it was recorded next to, so writing one slot twice would reach every draw in the
 * pass. A level differs from its neighbours in four numbers: its first instance, its sheet range and
 * the element its sheets draw. */
void ModelDraw::WriteUniforms(const FrameContext &ctx, const double east[3], const double north[3],
                              const double up[3], const double origin[3],
                              const LevelCut cuts[kLevels], uint32_t meshN,
                              uint32_t offsets[kLevels + 1]) {
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  for (int a = 0; a < 3; a++) {
    u[kUniFrame + 0 + a] = (float)east[a];
    u[kUniFrame + 4 + a] = (float)north[a];
    u[kUniFrame + 8 + a] = (float)up[a];
    u[kUniFrame + 3 + 4 * a] = (float)origin[a];
    u[kUniSun + a] = (float)SunDir[a];
  }
  u[kUniSun + 3] = NightAmbient;
  u[kUniLevel + 0] = (float)HeightM;
  u[kUniLevel + 2] = SheetFanDeg * 0.017453292f * 0.5f;
  u[kUniLevel + 3] = DetailScaleM;
  u[kUniCut + 3] = (float)kCells;
  u[kUniBox + 0] = Bounds.HalfWidth;
  u[kUniBox + 1] = (Bounds.Top + Bounds.Bottom) * 0.5f;
  u[kUniBox + 2] = std::fmax(Bounds.HalfWidth, (Bounds.Top - Bounds.Bottom) * 0.5f);
  std::memcpy(&u[kUniMaterial], MaterialRow, sizeof MaterialRow);

  for (int k = 0; k < kLevels; k++) {
    offsets[k] = (uint32_t)(k * kUniFloats * sizeof(float));
    u[kUniLevel + 1] = SheetSizeM[k];
    u[kUniCut + 0] = (float)cuts[k].First;
    u[kUniCut + 1] = (float)SheetCount[k];
    u[kUniCut + 2] = (float)SheetBase[k];
    Queue.WriteBuffer(Uni, offsets[k], u, sizeof u);
  }
  offsets[kLevels] = (uint32_t)(kLevels * kUniFloats * sizeof(float));
  u[kUniCut + 0] = (float)meshN;
  Queue.WriteBuffer(Uni, offsets[kLevels], u, sizeof u);
}

void ModelDraw::DrawSolids(wgpu::RenderPassEncoder &pass, const LevelCut cuts[kLevels],
                           const uint32_t offsets[kLevels + 1]) {
  for (int k = 0; k < kLevels; k++) {
    if (LevelIdxCount[k] == 0 || cuts[k].Count == 0) continue;
    pass.SetPipeline(SolidPipe);
    pass.SetBindGroup(0, Bind, 1, &offsets[k]);
    pass.SetVertexBuffer(0, LevelVtx[k]);
    pass.SetIndexBuffer(LevelIdx[k], wgpu::IndexFormat::Uint32);
    pass.DrawIndexed(LevelIdxCount[k], cuts[k].Count);
    Drawn += (long)LevelIdxCount[k] / 3 * (long)cuts[k].Count;
  }
}

void ModelDraw::DrawSheets(wgpu::RenderPassEncoder &pass, const LevelCut cuts[kLevels],
                           const uint32_t offsets[kLevels + 1], uint32_t meshN) {
  if (PlaceCount > 0) {
    for (int k = 0; k < kLevels; k++) {
      if (SheetCount[k] == 0 || cuts[k].Count == 0) continue;
      pass.SetPipeline(SheetPipe);
      pass.SetBindGroup(0, Bind, 1, &offsets[k]);
      pass.Draw(6, SheetCount[k] * cuts[k].Count);
      Drawn += 2 * (long)SheetCount[k] * (long)cuts[k].Count;
    }
    ImpN = (long)(PlaceCount - meshN);
    if (ImpPipe && ImpAlbedo && ImpN > 0) {
      pass.SetPipeline(ImpPipe);
      pass.SetBindGroup(0, Bind, 1, &offsets[kLevels]);
      pass.Draw(6, (uint32_t)ImpN);
      Drawn += 2 * ImpN;
    }
    return;
  }
  if (DetailIdxCount == 0 || DetailInstCount == 0) return;
  pass.SetPipeline(DetailPipe);
  pass.SetBindGroup(0, Bind, 1, &offsets[0]);
  pass.SetVertexBuffer(0, DetailVtx);
  pass.SetVertexBuffer(1, DetailInst);
  pass.SetIndexBuffer(DetailIdx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(DetailIdxCount, DetailInstCount);
  Drawn += (long)DetailIdxCount / 3 * (long)DetailInstCount;
}

void ModelDraw::Encode(const FrameContext &ctx, ClusterCut &cut, wgpu::RenderPassEncoder &pass) {
  Drawn = 0;
  MeshN = 0;
  MeshRadius = 0.0;
  for (int k = 0; k < kLevels; k++) LevelN[k] = 0;
  /* One subject from SetSubject OR a field from SetInstances; without either this unit draws
   * nothing. */
  if (!SolidPipe || LevelIdxCount[0] == 0 || (HeightM <= 0.0 && PlaceCount == 0)) return;

  double east[3], north[3], up[3], origin[3] = {0.0, 0.0, 0.0};
  Aim(ctx, east, north, up, origin);

  /* THE ONLY DISTANCE IN THIS FILE, and it is derived rather than declared: the impostor's texel is
   * the model-space error, and the mesh ladder ends where that error reaches the declared
   * tolerance. Resolution and FOV therefore move it on their own. */
  MeshRadius = std::sqrt(
      DagEdgeSq(HeightM * (double)ModelLadder::Error(kLevels), cut.PixelFocal(), SseTauPx()));

  LevelCut cuts[kLevels] = {};
  uint32_t meshN = PlaceCount;
  if (PlaceCount > 0) meshN = Sort(cut, east, north, up, origin, cuts);
  else cuts[0].Count = 1;
  MeshN = (long)meshN;
  for (int k = 0; k < kLevels; k++) LevelN[k] = (long)cuts[k].Count;

  uint32_t offsets[kLevels + 1] = {};
  WriteUniforms(ctx, east, north, up, origin, cuts, meshN, offsets);
  DrawSolids(pass, cuts, offsets);
  if (SheetsOn) DrawSheets(pass, cuts, offsets, meshN);
}

} // namespace outshine::Render
