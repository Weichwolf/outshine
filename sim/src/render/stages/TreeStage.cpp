#include "TreeStage.h"

#include "PixelFocalLength.h"
#include "SceneTargets.h"
#include "SceneScale.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this stage. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "ShadowSample.h"
#include "SurfaceLight.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace outshine::Render {


static const char *kTreeWGSL = R"(
const kCardLeaves : i32 = 16;
/* The stretch of shoot the laminae are rooted along, in units of ONE lamina's length. [SET] — a
 * beech short shoot carries its year's leaves over about four fifths of a leaf length. */
const kCardShoot : f32 = 0.82;
struct T {
  mvp  : mat4x4f,
  ax   : vec4f,   // ECEF east axis,  w = the single stand's east offset from the camera (m)
  ay   : vec4f,   // ECEF north axis, w = the single stand's north offset (m)
  az   : vec4f,   // ECEF up axis,    w = the eye's height over the single stand's foot (m)
  sun  : vec4f,   // ECEF sun direction, w = this frame's ambient night floor
  bark : vec4f,   // rgb bark reflectance, w = the furrow's share of it
  bpar : vec4f,   // x = furrow cycles per radian, y = furrow depth, z = tree height (m), w = leaf scale (m)
  leaf : vec4f,   // rgb lamina reflectance (tint x base), w = this draw's first stand index
  lsh  : vec4f,   // lamina outline: width, widest, tip, base fill
  lsh2 : vec4f,   // lamina outline: lobes, lobe depth, serration, fold
  cpar : vec4f,   // x = card leaf length (m), y = fan half angle (rad), z = needle width, w = cards per tree
  lpro : vec4f,   // the lamina profile's constants: exponent a, exponent b, 1/peak, w = this rank's first card
  ipar : vec4f,   // x = octahedral cells per side, y = crown half width, z = crown centre, w = crown half size
};
@group(0) @binding(0) var<uniform> b : T;
@group(0) @binding(1) var<storage, read> I : Irr;
@group(0) @binding(2) var<uniform> C : Csm;
@group(0) @binding(3) var shMap : texture_depth_2d;
@group(0) @binding(4) var shSamp : sampler_comparison;
@group(0) @binding(5) var<storage, read> St : array<f32>;
@group(0) @binding(6) var<storage, read> Cd : array<f32>;
@group(0) @binding(7) var impSamp : sampler;
@group(0) @binding(8) var impAlb : texture_2d<f32>;
@group(0) @binding(9) var impNrm : texture_2d<f32>;

/* Tree space is y-up and metric once multiplied by the declared height; the stand maps it onto the
 * ECEF triad the whole frame is built in. One place, every net. */
fn standOrigin() -> vec3f {
  return b.ax.xyz * b.ax.w + b.ay.xyz * b.ay.w - b.az.xyz * b.az.w;
}
/* TREE Z POINTS SOUTH, and that is not a taste: (east, up, north) is LEFT-handed (east x up = -north),
 * so mapping a right-handed y-up model onto it MIRRORS the tree and inverts every triangle's winding.
 * Negating one axis makes the map a proper rotation — the mesh keeps its handedness and back-face
 * culling keeps its meaning. */
fn toEcef(local : vec3f) -> vec3f {
  return b.ax.xyz * local.x - b.ay.xyz * local.z + b.az.xyz * local.y;
}
fn yawRot(v : vec3f, cy : f32, sy : f32) -> vec3f {
  return vec3f(v.x * cy - v.z * sy, v.y, v.x * sy + v.z * cy);
}

struct TOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) nrm : vec3f,
              @location(2) loc : vec3f };

/* Ein Stand: xy Ost/Nord in Metern vom Kamerastandpunkt, z der Fuss ueber der Augenhoehe, w die
 * Gierung. b.bpar.z ist die Hoehe der Art, `sf` die des einzelnen Standes darin. */
@vertex fn vsBark(@location(0) p : vec3f, @location(1) n : vec3f,
                  @builtin(instance_index) ii : u32) -> TOut {
  var o : TOut;
  let si = (ii + u32(b.leaf.w)) * 5u;
  let st = vec4f(St[si], St[si + 1u], St[si + 2u], St[si + 3u]);
  let sf = St[si + 4u];
  let cy = cos(st.w); let sy = sin(st.w);
  let pm = yawRot(p * (b.bpar.z * sf), cy, sy);
  let rel = standOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + b.az.xyz * st.z + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  let nr = yawRot(n, cy, sy);
  o.nrm = toEcef(nr);
  o.loc = vec3f(nr.x, pm.y, nr.z);   /* the furrow reads a circumferential angle and a height, no uv */
  return o;
}

/* THE LAMINA ROLLS FREELY ABOUT ITS STALK and that is the same assumption world/LeafAngleDistribution
 * closes G(el) under: midrib on the measured stalk direction, normal anywhere on the circle
 * perpendicular to it. A tilt invented here would draw a canopy the far stage no longer computes. */
fn leafFrame(dir : vec3f, roll : f32, xA : ptr<function, vec3f>, yA : ptr<function, vec3f>,
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

@vertex fn vsLeaf(@location(0) v : vec3f, @location(1) n : vec3f,
                  @location(2) ip : vec4f, @location(3) idir : vec4f) -> TOut {
  var o : TOut;
  var xAx : vec3f; var yAx : vec3f; var zAx : vec3f;
  leafFrame(idir.xyz, ip.w, &xAx, &yAx, &zAx);
  let pm = ip.xyz * b.bpar.z + (xAx * v.x + yAx * v.y + zAx * v.z) * b.bpar.w;
  let nl = xAx * n.x + yAx * n.y + zAx * n.z;
  let rel = standOrigin() + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  o.nrm = toEcef(nl);
  o.loc = vec3f(0.0);
  return o;
}

/* THE CLUSTER CARD. Two triangles standing for kLeavesPerCard laminae fanned off one shoot point: the
 * fragment cuts the declared outline out of the quad, so the silhouette is the species' leaf and the
 * cost is two triangles instead of the lamina's hundred-odd. `loc` carries the card's own (u, v). */
struct COut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) nrm : vec3f,
              @location(2) loc : vec3f };

fn quadCorner(vi : u32) -> vec2f {
  var q = vec2f(-1.0, 0.0);
  if (vi == 1u || vi == 3u) { q = vec2f(1.0, 0.0); }
  if (vi == 2u || vi == 4u) { q = vec2f(1.0, 1.0); }
  if (vi == 5u) { q = vec2f(-1.0, 1.0); }
  return q;
}

@vertex fn vsCard(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> COut {
  var o : COut;
  let nc = u32(b.cpar.w);
  let ci = (u32(b.lpro.w) + ii % nc) * 8u;
  let si = (ii / nc + u32(b.leaf.w)) * 5u;
  let st = vec4f(St[si], St[si + 1u], St[si + 2u], St[si + 3u]);
  let sf = St[si + 4u];

  let q = quadCorner(vi);

  var xAx : vec3f; var yAx : vec3f; var zAx : vec3f;
  leafFrame(vec3f(Cd[ci + 4u], Cd[ci + 5u], Cd[ci + 6u]), Cd[ci + 3u], &xAx, &yAx, &zAx);
  /* THE CARD IS A SHOOT, and its two extents follow from the lamina rather than from taste: it is as
   * wide as a leaf swung to the edge of the fan plus that leaf's own half width, and as long as the
   * stretch of shoot the leaves sit on plus one leaf. Both in units of the leaf's length. */
  let L = b.cpar.x * sf;
  let hw = L * (sin(b.cpar.y) + b.lsh.x);
  let hh = L * (kCardShoot + 1.0);
  let anchor = vec3f(Cd[ci], Cd[ci + 1u], Cd[ci + 2u]) * (b.bpar.z * sf);
  let cyw = cos(st.w); let syw = sin(st.w);
  let pm = yawRot(anchor + xAx * (q.x * hw) + yAx * (q.y * hh), cyw, syw);
  let rel = standOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + b.az.xyz * st.z + toEcef(pm);
  o.pos = b.mvp * vec4f(rel, 1.0);
  o.rel = rel;
  o.nrm = toEcef(yawRot(zAx, cyw, syw));
  o.loc = vec3f(q.x * hw / L, q.y * hh / L, 0.0);
  return o;
}

/* world/TreeLeaf's ProfileWidth, verbatim: the half width of the lamina at t along the midrib, as a
 * fraction of its length. Two languages, ONE curve — a second outline here would draw a leaf the
 * subject bench never judged. */
fn profileWidth(t : f32) -> f32 {
  if (b.cpar.z > 0.0) {
    var wn = b.cpar.z * (1.0 - 0.12 * t);
    if (t > 0.82) { wn = wn * (1.0 - t) / 0.18; }
    return wn;
  }
  var w = pow(t, b.lpro.x) * pow(1.0 - t, b.lpro.y) * b.lpro.z;
  w = w * b.lsh.x;
  if (b.lsh.w > 0.0) { w = w + b.lsh.w * b.lsh.x * exp(-t * 9.0); }
  if (b.lsh2.x > 0.0) {
    let lob = 0.5 + 0.5 * cos(6.2831853 * b.lsh2.x * t);
    w = w * (1.0 - b.lsh2.y * lob);
  }
  if (b.lsh2.z > 0.0) {
    let ff = select(7.0, b.lsh2.x, b.lsh2.x > 0.0) * 2.0;
    let saw = abs(2.0 * (t * ff - floor(t * ff + 0.5)));
    w = w * (1.0 - b.lsh2.z * 0.45 * saw);
  }
  return w;
}

/* THE FURROW, and its metric is the declared one: `bpar.x` counts cycles per RADIAN of circumference,
 * so the pitch on a 0.4 m trunk radius is 0.4/f metres and a twig — whose radius is a tenth of that —
 * comes out smooth, which is what bark does. The second sine breaks the comb into ridges of unequal
 * width; the meander with height keeps them from being drawn lines. */
fn furrow(nlx : f32, nlz : f32, y : f32, f : f32) -> f32 {
  let ang = atan2(nlz, nlx) + 0.22 * sin(y * 2.1) + 0.11 * sin(y * 5.3 + 1.3);
  let s1 = sin(ang * f) * 0.5 + 0.5;
  let s2 = sin(ang * f * 2.37 + 1.7) * 0.5 + 0.5;
  return pow(mix(s1, s1 * s2, 0.55), 1.4);
}

@fragment fn fsBark(in : TOut) -> @location(0) vec4f {
  let upB = normalize(b.az.xyz);
  let sunB = normalize(b.sun.xyz);
  let nB = normalize(in.nrm);
  /* A furrow runs ALONG the axis, so a face looking down the axis has none. The local normal's radial
   * length is that fade and it also keeps atan2 out of its degenerate direction. */
  let radial = length(vec2f(in.loc.x, in.loc.z));
  let fr = furrow(in.loc.x, in.loc.z, in.loc.y, b.bpar.x) * radial;
  let alb = b.bark.rgb * mix(b.bark.w, 1.0, mix(1.0, fr, b.bpar.y));
  let sunVis = csmSunVis(shMap, shSamp, C, in.rel, upB, sunB);
  return litRadiance(I, alb, 1.0, nB, upB, sunB, sunVis,
                     1.0, 1.0, b.sun.w);
}

/* THE ONE TERM AN OPAQUE BRDF HAS NO PLACE FOR. A leaf is 0.2 mm of mesophyll: in the visible it
 * transmits about as much as it reflects (LOPEX/PROSPECT, tau ~ rho at 550 and 650 nm), so the
 * transmittance IS the albedo and this introduces no constant. It fires only when sun and eye stand
 * on opposite faces, which is the whole of what `backlit` exists to show. */
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

fn laminaShade(rel : vec3f, nrm : vec3f) -> vec4f { return shadeSheet(rel, nrm, b.leaf.rgb); }

@fragment fn fsLeaf(in : TOut) -> @location(0) vec4f {
  return laminaShade(in.rel, in.nrm);
}

/* The card's own cut-out: kCardLeaves laminae rooted along the shoot, sides alternating and the fan
 * angle stepped by the golden fraction so no two neighbours lie on top of each other. */
fn cardHit(px : f32, py : f32) -> bool {
  for (var k = 0; k < kCardLeaves; k = k + 1) {
    let fk = f32(k);
    let base = kCardShoot * (fk + 0.5) / f32(kCardLeaves);
    let a = b.cpar.y * (2.0 * fract(fk * 0.6180339 + 0.13) - 1.0);
    let ca = cos(a); let sa = sin(a);
    let dy = py - base;
    let t = px * sa + dy * ca;
    let s = px * ca - dy * sa;
    if (t > 0.02 && t < 1.0 && abs(s) < profileWidth(t)) { return true; }
  }
  return false;
}

@fragment fn fsCard(in : COut) -> @location(0) vec4f {
  let hit = cardHit(in.loc.x, in.loc.y);
  /* Shaded BEFORE the cut, not after: `discard` inside a branch would put the shadow comparison in
   * non-uniform control flow and the module would not compile. */
  let c = laminaShade(in.rel, in.nrm);
  if (!hit) { discard; }
  return c;
}

/* ---- the impostor: bake, then draw ---- */

struct BOut { @location(0) alb : vec4f, @location(1) nrm : vec4f };

@fragment fn fsBarkBake(in : TOut) -> BOut {
  var o : BOut;
  let radial = length(vec2f(in.loc.x, in.loc.z));
  let fr = furrow(in.loc.x, in.loc.z, in.loc.y, b.bpar.x) * radial;
  o.alb = vec4f(b.bark.rgb * mix(b.bark.w, 1.0, mix(1.0, fr, b.bpar.y)), 1.0);
  o.nrm = vec4f(normalize(in.nrm) * 0.5 + 0.5, 1.0);
  return o;
}

@fragment fn fsCardBake(in : COut) -> BOut {
  var o : BOut;
  o.alb = vec4f(b.leaf.rgb, 1.0);
  o.nrm = vec4f(normalize(in.nrm) * 0.5 + 0.5, 1.0);
  if (!cardHit(in.loc.x, in.loc.y)) { discard; }
  return o;
}

struct IOut { @builtin(position) pos : vec4f, @location(0) rel : vec3f, @location(1) uv : vec2f,
              @location(2) vdl : vec3f, @location(3) yw : vec2f };

@vertex fn vsImp(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> IOut {
  var o : IOut;
  let si = (ii + u32(b.leaf.w)) * 5u;
  let st = vec4f(St[si], St[si + 1u], St[si + 2u], St[si + 3u]);
  let sf = St[si + 4u];
  let h = b.bpar.z * sf;
  let hs = b.ipar.w * h;
  let foot = standOrigin() + b.ax.xyz * st.x + b.ay.xyz * st.y + b.az.xyz * st.z;
  let ctr = foot + b.az.xyz * (b.ipar.z * h);
  let vd = normalize(-ctr);
  var right = cross(b.az.xyz, vd);
  let rl = length(right);
  right = select(b.ax.xyz, right / max(rl, 1.0e-6), rl > 1.0e-4);
  let q = quadCorner(vi);
  let wp = ctr + right * (q.x * hs) + b.az.xyz * ((q.y * 2.0 - 1.0) * hs);
  o.pos = b.mvp * vec4f(wp, 1.0);
  o.rel = wp;
  /* v = 0 is the atlas's TOP row and the bake put the crown there, so the quad's top edge reads 0. */
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
  let N = b.ipar.x;
  let oc = clamp(hemiOctEnc(normalize(vd)), vec2f(0.0), vec2f(0.999));
  let cell = floor(oc * N);
  /* Inset by half a texel plus a guard: a bilinear tap at a cell's edge would otherwise reach into
   * the neighbouring view and hang a sliver of another silhouette on the crown. */
  let uv = (cell + clamp(in.uv, vec2f(0.006), vec2f(0.994))) / N;
  let t = textureSampleLevel(impAlb, impSamp, uv, 0.0);
  let nt = textureSampleLevel(impNrm, impSamp, uv, 0.0).xyz * 2.0 - 1.0;
  let nE = toEcef(yawRot(nt, in.yw.x, in.yw.y));
  let c = shadeSheet(in.rel, nE, t.rgb);
  if (t.a < 0.35) { discard; }
  return c;
}
)";

void TreeStage::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + ShadowSampleWGSL()
                        + kTreeWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(false)};

  /* EXPLICIT, because three pipelines share one bind group: a default layout belongs to the pipeline
   * that produced it, and a group built from the bark's would be rejected by the card's. */
  wgpu::BindGroupLayoutEntry ble[10] = {};
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
  wgpu::BindGroupLayoutDescriptor bgld{};
  bgld.entryCount = 10;
  bgld.entries = ble;
  Bgl = Device.CreateBindGroupLayout(&bgld);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &Bgl;
  wgpu::PipelineLayout pl = Device.CreatePipelineLayout(&pld);

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;   /* reversed-Z, as every scene-pass surface */

  wgpu::VertexAttribute barkAttr[2] = {};
  barkAttr[0].format = wgpu::VertexFormat::Float32x3; barkAttr[0].offset = 0;  barkAttr[0].shaderLocation = 0;
  barkAttr[1].format = wgpu::VertexFormat::Float32x3; barkAttr[1].offset = 12; barkAttr[1].shaderLocation = 1;
  wgpu::VertexBufferLayout barkBuf{};
  barkBuf.arrayStride = kBarkFloats * sizeof(float);
  barkBuf.attributeCount = 2;
  barkBuf.attributes = barkAttr;

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = pl;
  rp.vertex.module = m;
  rp.vertex.entryPoint = "vsBark";
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &barkBuf;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.entryPoint = "fsBark";
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  rp.primitive.cullMode = wgpu::CullMode::Back;
  BarkPipe = Device.CreateRenderPipeline(&rp);

  wgpu::VertexAttribute leafAttr[2] = {};
  leafAttr[0].format = wgpu::VertexFormat::Float32x3; leafAttr[0].offset = 0;  leafAttr[0].shaderLocation = 0;
  leafAttr[1].format = wgpu::VertexFormat::Float32x3; leafAttr[1].offset = 12; leafAttr[1].shaderLocation = 1;
  wgpu::VertexAttribute instAttr[2] = {};
  instAttr[0].format = wgpu::VertexFormat::Float32x4; instAttr[0].offset = 0;  instAttr[0].shaderLocation = 2;
  instAttr[1].format = wgpu::VertexFormat::Float32x4; instAttr[1].offset = 16; instAttr[1].shaderLocation = 3;
  wgpu::VertexBufferLayout leafBufs[2] = {};
  leafBufs[0].arrayStride = kLeafFloats * sizeof(float);
  leafBufs[0].attributeCount = 2;
  leafBufs[0].attributes = leafAttr;
  leafBufs[1].arrayStride = kInstFloats * sizeof(float);
  leafBufs[1].stepMode = wgpu::VertexStepMode::Instance;
  leafBufs[1].attributeCount = 2;
  leafBufs[1].attributes = instAttr;

  rp.vertex.entryPoint = "vsLeaf";
  rp.vertex.bufferCount = 2;
  rp.vertex.buffers = leafBufs;
  fs.entryPoint = "fsLeaf";
  /* A lamina has no back: it is one sheet with a face on either side, and the fragment picks the one
   * that faces the eye. Culling it would delete half the canopy at every sun angle. */
  rp.primitive.cullMode = wgpu::CullMode::None;
  LeafPipe = Device.CreateRenderPipeline(&rp);

  rp.vertex.entryPoint = "vsCard";
  rp.vertex.bufferCount = 0;
  rp.vertex.buffers = nullptr;
  fs.entryPoint = "fsCard";
  CardPipe = Device.CreateRenderPipeline(&rp);

  rp.vertex.entryPoint = "vsImp";
  fs.entryPoint = "fsImp";
  ImpPipe = Device.CreateRenderPipeline(&rp);

  /* THE BAKE'S OWN TARGETS: albedo with coverage in alpha, and the tree-space normal. Two 8-bit
   * surfaces and no velocity — the atlas is a picture of a mesh, not a frame. */
  wgpu::ColorTargetState bct[2] = {};
  bct[0].format = wgpu::TextureFormat::RGBA8Unorm;
  bct[1].format = wgpu::TextureFormat::RGBA8Unorm;
  fs.targetCount = 2;
  fs.targets = bct;
  rp.vertex.entryPoint = "vsBark";
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &barkBuf;
  fs.entryPoint = "fsBarkBake";
  rp.primitive.cullMode = wgpu::CullMode::Back;
  BarkBakePipe = Device.CreateRenderPipeline(&rp);
  rp.vertex.entryPoint = "vsCard";
  rp.vertex.bufferCount = 0;
  rp.vertex.buffers = nullptr;
  fs.entryPoint = "fsCardBake";
  rp.primitive.cullMode = wgpu::CullMode::None;
  CardBakePipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = (kRanks + 1) * kUniFloats * sizeof(float);   /* one slot per mesh rank plus the impostor */
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
  const float oneStand[8] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
  StandBuf = Upload(oneStand, sizeof oneStand, wgpu::BufferUsage::Storage);
  CardInst = Upload(oneStand, sizeof oneStand, wgpu::BufferUsage::Storage);
  Rebind();
}

void TreeStage::Rebind() {
  if (!Bgl) return;
  wgpu::BindGroupEntry be[10] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = Light.Irradiance; be[1].size = wgpu::kWholeSize;
  be[2].binding = 2; be[2].buffer = Light.Cascades;   be[2].size = kShadowUniFloats * sizeof(float);
  be[3].binding = 3; be[3].textureView = Light.ShadowAtlas;
  be[4].binding = 4; be[4].sampler = Light.ShadowCompare;
  be[5].binding = 5; be[5].buffer = StandBuf;  be[5].size = wgpu::kWholeSize;
  be[6].binding = 6; be[6].buffer = CardInst;  be[6].size = wgpu::kWholeSize;
  be[7].binding = 7; be[7].sampler = ImpSamp;
  be[8].binding = 8; be[8].textureView = ImpAlbedoView;
  be[9].binding = 9; be[9].textureView = ImpNormalView;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Bgl;
  bg.entryCount = 10;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
  if (BakeUni) {
    be[0].buffer = BakeUni;
    BakeBind = Device.CreateBindGroup(&bg);
  }
}

void TreeStage::SetCrown(float halfWidth, float top, float bottom) {
  CrownHalf = halfWidth;
  CrownTop = top;
  CrownBot = bottom;
}

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

wgpu::Buffer TreeStage::Upload(const void *data, size_t bytes, wgpu::BufferUsage usage) {
  wgpu::BufferDescriptor bd{};
  bd.size = (bytes + 3u) & ~size_t(3);
  bd.usage = usage | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer b = Device.CreateBuffer(&bd);
  Queue.WriteBuffer(b, 0, data, bytes);
  return b;
}

void TreeStage::SetBark(int rank, const float *verts, uint32_t nverts, const uint32_t *idx,
                        uint32_t nidx) {
  if (rank < 0 || rank >= kRanks) return;
  BarkCount[rank] = 0;
  if (!Device || !verts || nverts == 0 || nidx == 0) return;
  BarkVtx[rank] =
      Upload(verts, (size_t)nverts * kBarkFloats * sizeof(float), wgpu::BufferUsage::Vertex);
  BarkIdx[rank] = Upload(idx, (size_t)nidx * sizeof(uint32_t), wgpu::BufferUsage::Index);
  BarkCount[rank] = nidx;
}

void TreeStage::SetLeaf(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                        const float *inst, uint32_t ninst, float scaleM) {
  LeafCount = 0;
  InstCount = 0;
  if (!Device || !verts || nverts == 0 || nidx == 0 || !inst || ninst == 0) return;
  LeafVtx = Upload(verts, (size_t)nverts * kLeafFloats * sizeof(float), wgpu::BufferUsage::Vertex);
  LeafIdx = Upload(idx, (size_t)nidx * sizeof(uint32_t), wgpu::BufferUsage::Index);
  LeafInst = Upload(inst, (size_t)ninst * kInstFloats * sizeof(float), wgpu::BufferUsage::Vertex);
  LeafCount = nidx;
  InstCount = ninst;
  LeafScaleM = scaleM;
}

/* All ranks live in ONE storage buffer with a per-rank base, because the bind group is built once and
 * a second card buffer would mean a second group for a draw that differs in a single integer. */
void TreeStage::SetCards(int rank, const float *inst, uint32_t n, float leafLenM, float spreadDeg) {
  if (rank < 0 || rank >= kRanks) return;
  CardStage[rank].assign(inst && n ? inst : nullptr,
                         inst && n ? inst + (size_t)n * kInstFloats : nullptr);
  CardLeafM[rank] = leafLenM;
  CardSpreadDeg = spreadDeg;
  if (!Device) return;
  std::vector<float> all;
  uint32_t base = 0;
  for (int k = 0; k < kRanks; k++) {
    CardBase[k] = base;
    CardCount[k] = (uint32_t)(CardStage[k].size() / kInstFloats);
    all.insert(all.end(), CardStage[k].begin(), CardStage[k].end());
    base += CardCount[k];
  }
  if (all.empty()) return;
  CardInst = Upload(all.data(), all.size() * sizeof(float), wgpu::BufferUsage::Storage);
  Rebind();
}

void TreeStage::SetStand(double eastM, double northM, double eyeAglM, double heightM) {
  EastM = eastM;
  NorthM = northM;
  EyeAglM = eyeAglM;
  HeightM = heightM;
}

void TreeStage::SetStands(const float *inst, uint32_t n, const float *distM) {
  StandCount = 0;
  StandDist.clear();
  if (n == 0 || !Device || !inst || !distM) return;
  StandBuf = Upload(inst, (size_t)n * kStandFloats * sizeof(float), wgpu::BufferUsage::Storage);
  StandDist.assign(distM, distM + n);
  StandCount = n;
  Rebind();
}

void TreeStage::CreateImpostor() {
  if (!Device || BarkCount[0] == 0) return;
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
  Rebind();
}

void TreeStage::EncodeBake(wgpu::RenderPassEncoder &pass) {
  if (!ImpAlbedo || !BarkBakePipe) return;
  /* The tree's own axes, so `toEcef` is the identity and the bake sees the mesh in the frame it was
   * grown in: east = +x, up = +y, and north = -z because tree z points south. */
  const float hs = std::fmax(CrownHalf, (CrownTop - CrownBot) * 0.5f) * (float)HeightM;
  const float ctrY = (CrownTop + CrownBot) * 0.5f * (float)HeightM;
  float u[kUniFloats] = {};
  u[16] = 1.0f;                                     /* ax */
  u[22] = -1.0f;                                    /* ay = (0, 0, -1) */
  u[25] = 1.0f;                                     /* az */
  u[32] = Look.BarkRgb[0]; u[33] = Look.BarkRgb[1]; u[34] = Look.BarkRgb[2]; u[35] = Look.BarkDark;
  u[36] = Look.BarkFreq; u[37] = Look.BarkRidge; u[38] = (float)HeightM; u[39] = LeafScaleM;
  u[40] = Look.LeafRgb[0]; u[41] = Look.LeafRgb[1]; u[42] = Look.LeafRgb[2]; u[43] = 0.0f;
  u[44] = Look.LeafWidth; u[45] = Look.LeafWidest; u[46] = Look.LeafTip; u[47] = Look.LeafBaseFill;
  u[48] = Look.LeafLobes; u[49] = Look.LeafLobeDepth; u[50] = Look.LeafSerration; u[51] = Look.LeafFold;
  u[52] = CardLeafM[0]; u[53] = CardSpreadDeg * 0.017453292f * 0.5f; u[54] = Look.NeedleWidth;
  u[55] = (float)CardCount[0];
  const float ea = Look.LeafWidest * 1.5f + 0.80f;
  float eb = (1.0f - Look.LeafWidest) * 1.5f + 0.95f - Look.LeafTip * 1.25f;
  if (eb < 0.55f) eb = 0.55f;
  const float peak = std::pow(Look.LeafWidest, ea) * std::pow(1.0f - Look.LeafWidest, eb);
  u[56] = ea; u[57] = eb; u[58] = peak > 1.0e-6f ? 1.0f / peak : 0.0f;
  u[59] = (float)CardBase[0];

  for (int j = 0; j < kCells; j++) {
    for (int i = 0; i < kCells; i++) {
      float dir[3];
      HemiOctDecode(((float)i + 0.5f) / (float)kCells, ((float)j + 0.5f) / (float)kCells, dir);
      BakeMvp(dir, ctrY, hs, u);
      const uint32_t off = (uint32_t)(j * kCells + i) * (uint32_t)(kUniFloats * sizeof(float));
      Queue.WriteBuffer(BakeUni, off, u, sizeof u);
      pass.SetViewport((float)(i * kCellSize), (float)(j * kCellSize), (float)kCellSize,
                       (float)kCellSize, 0.0f, 1.0f);
      pass.SetPipeline(BarkBakePipe);
      pass.SetBindGroup(0, BakeBind, 1, &off);
      pass.SetVertexBuffer(0, BarkVtx[0]);
      pass.SetIndexBuffer(BarkIdx[0], wgpu::IndexFormat::Uint32);
      pass.DrawIndexed(BarkCount[0], 1);
      if (CardCount[0] > 0) {
        pass.SetPipeline(CardBakePipe);
        pass.SetBindGroup(0, BakeBind, 1, &off);
        pass.Draw(6, CardCount[0]);
      }
    }
  }
}

void TreeStage::FinishBake() {
  ImpDepth = nullptr;
  ImpDepthView = nullptr;
  ImpAlbedoView = ImpAlbedo.CreateView();
  ImpNormalView = ImpNormal.CreateView();
  Rebind();
}

void TreeStage::SetSun(const double sunEcef[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) SunDir[i] = sunEcef[i];
  NightAmbient = nightAmbient;
}

void TreeStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  Drawn = 0;
  MeshN = 0;
  MeshRadius = 0.0;
  for (int k = 0; k < kRanks; k++) RankN[k] = 0;
  /* Ein Stand aus SetStand ODER ein Feld aus SetStands; ohne beides zeichnet die Stage nichts. */
  if (!BarkPipe || BarkCount[0] == 0 || (HeightM <= 0.0 && StandCount == 0)) return;

  double east[3], north[3];
  const double up[3] = {ctx.Up[0], ctx.Up[1], ctx.Up[2]};
  east[0] = -up[1]; east[1] = up[0]; east[2] = 0.0;   /* z_ecef x up, before normalising */
  double el = std::sqrt(east[0] * east[0] + east[1] * east[1]);
  if (el < 1.0e-12) { east[0] = 1.0; east[1] = 0.0; el = 1.0; }
  for (int a = 0; a < 3; a++) east[a] /= el;
  north[0] = up[1] * east[2] - up[2] * east[1];
  north[1] = up[2] * east[0] - up[0] * east[2];
  north[2] = up[0] * east[1] - up[1] * east[0];

  /* THE ONLY DISTANCE IN THIS FILE, and it is derived rather than declared: the impostor's texel is
   * the model-space error, and the mesh rank ends where that error reaches one pixel. Resolution and
   * FOV therefore move the rank on their own. */
  const double fPx = PixelFocalLength(ctx.Height, (double)ctx.FovDeg);
  MeshRadius = HeightM * fPx / (double)kCellPx;
  /* Rank k ends where rank k+1's own error first stays under a pixel; the last one ends at the
   * impostor. The stands arrive sorted, so every rank is a contiguous range. */
  uint32_t first[kRanks] = {}, cnt[kRanks] = {};
  uint32_t nMesh = StandCount;
  if (StandCount > 0) {
    uint32_t lo = 0;
    for (int k = 0; k < kRanks; k++) {
      const double edge = HeightM * fPx * (double)RankPixel(k + 1);
      const auto e = std::upper_bound(StandDist.begin(), StandDist.end(), (float)edge);
      const uint32_t hi = (uint32_t)(e - StandDist.begin());
      first[k] = lo;
      cnt[k] = hi > lo ? hi - lo : 0u;
      lo = hi > lo ? hi : lo;
    }
    nMesh = lo;
  } else {
    cnt[0] = 1;
  }
  MeshN = (long)nMesh;
  for (int k = 0; k < kRanks; k++) RankN[k] = (long)cnt[k];

  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  u[16] = (float)east[0];  u[17] = (float)east[1];  u[18] = (float)east[2];  u[19] = (float)EastM;
  u[20] = (float)north[0]; u[21] = (float)north[1]; u[22] = (float)north[2]; u[23] = (float)NorthM;
  u[24] = (float)up[0];    u[25] = (float)up[1];    u[26] = (float)up[2];    u[27] = (float)EyeAglM;
  u[28] = (float)SunDir[0]; u[29] = (float)SunDir[1]; u[30] = (float)SunDir[2]; u[31] = NightAmbient;
  u[32] = Look.BarkRgb[0]; u[33] = Look.BarkRgb[1]; u[34] = Look.BarkRgb[2]; u[35] = Look.BarkDark;
  u[36] = Look.BarkFreq; u[37] = Look.BarkRidge; u[38] = (float)HeightM; u[39] = LeafScaleM;
  u[40] = Look.LeafRgb[0]; u[41] = Look.LeafRgb[1]; u[42] = Look.LeafRgb[2]; u[43] = 0.0f;
  u[44] = Look.LeafWidth; u[45] = Look.LeafWidest; u[46] = Look.LeafTip; u[47] = Look.LeafBaseFill;
  u[48] = Look.LeafLobes; u[49] = Look.LeafLobeDepth; u[50] = Look.LeafSerration; u[51] = Look.LeafFold;
  u[53] = CardSpreadDeg * 0.017453292f * 0.5f; u[54] = Look.NeedleWidth;
  const float ea = Look.LeafWidest * 1.5f + 0.80f;
  float eb = (1.0f - Look.LeafWidest) * 1.5f + 0.95f - Look.LeafTip * 1.25f;
  if (eb < 0.55f) eb = 0.55f;
  const float peak = std::pow(Look.LeafWidest, ea) * std::pow(1.0f - Look.LeafWidest, eb);
  u[56] = ea; u[57] = eb; u[58] = peak > 1.0e-6f ? 1.0f / peak : 0.0f;
  u[60] = (float)kCells;
  u[61] = CrownHalf;
  u[62] = (CrownTop + CrownBot) * 0.5f;
  u[63] = std::fmax(CrownHalf, (CrownTop - CrownBot) * 0.5f);
  /* ONE SLOT PER RANK, ALL WRITTEN BEFORE ANY DRAW. A queue write is ordered against the SUBMIT, not
   * against the draw it was recorded next to, so writing one slot twice would reach every draw in the
   * pass. A rank differs from its neighbours in four fields: its first stand, its card range and the
   * leaf its cards draw. */
  uint32_t off[kRanks + 1] = {};
  for (int k = 0; k < kRanks; k++) {
    off[k] = (uint32_t)(k * kUniFloats * sizeof(float));
    u[43] = (float)first[k];
    u[52] = CardLeafM[k];
    u[55] = (float)CardCount[k];
    u[59] = (float)CardBase[k];
    Queue.WriteBuffer(Uni, off[k], u, sizeof u);
  }
  off[kRanks] = (uint32_t)(kRanks * kUniFloats * sizeof(float));
  u[43] = (float)nMesh;
  Queue.WriteBuffer(Uni, off[kRanks], u, sizeof u);

  for (int k = 0; k < kRanks; k++) {
    if (BarkCount[k] == 0 || cnt[k] == 0) continue;
    pass.SetPipeline(BarkPipe);
    pass.SetBindGroup(0, Bind, 1, &off[k]);
    pass.SetVertexBuffer(0, BarkVtx[k]);
    pass.SetIndexBuffer(BarkIdx[k], wgpu::IndexFormat::Uint32);
    pass.DrawIndexed(BarkCount[k], cnt[k]);
    Drawn += (long)BarkCount[k] / 3 * (long)cnt[k];
  }

  if (!LeavesOn) return;
  if (StandCount > 0) {
    for (int k = 0; k < kRanks; k++) {
      if (CardCount[k] == 0 || cnt[k] == 0) continue;
      pass.SetPipeline(CardPipe);
      pass.SetBindGroup(0, Bind, 1, &off[k]);
      pass.Draw(6, CardCount[k] * cnt[k]);
      Drawn += 2 * (long)CardCount[k] * (long)cnt[k];
    }
    ImpN = (long)(StandCount - nMesh);
    if (ImpPipe && ImpAlbedo && ImpN > 0) {
      pass.SetPipeline(ImpPipe);
      pass.SetBindGroup(0, Bind, 1, &off[kRanks]);
      pass.Draw(6, (uint32_t)ImpN);
      Drawn += 2 * ImpN;
    }
    return;
  }
  if (LeafCount == 0 || InstCount == 0) return;
  pass.SetPipeline(LeafPipe);
  pass.SetBindGroup(0, Bind, 1, &off[0]);
  pass.SetVertexBuffer(0, LeafVtx);
  pass.SetVertexBuffer(1, LeafInst);
  pass.SetIndexBuffer(LeafIdx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(LeafCount, InstCount);
  Drawn += (long)LeafCount / 3 * (long)InstCount;
}

} // namespace outshine::Render
