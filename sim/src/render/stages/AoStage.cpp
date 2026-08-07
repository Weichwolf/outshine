#include "AoStage.h"
#include "AtmoCommon.h"
#include "GeometryIsolation.h"
#include <cstdlib>
#include <string>

namespace outshine::Render {

static const char *kAoWGSL = R"(
@group(0) @binding(0) var depthTex : texture_depth_2d;
@group(0) @binding(1) var<uniform> A : Atmo;

/* [SET] 0.9 m: the contact scale a walker reads — a wall foot, a kerb, the seam where a prism meets
 * the ground. Larger radii start shading whole facades, which is the shadow map's job. */
const kAoRadiusM : f32 = 0.9;
const kAoSamples : i32 = 16;
const kAoStrength : f32 = 1.0;
/* Below this the whole sample disc lands inside one depth texel and the estimate is noise, not
 * occlusion — measured: a town at 3 km came out with 2.2 % of the frame at pure BLACK, because a
 * fully sky-lit wall carries direct fraction 0 and the composite handed it the whole bogus value. */
const kAoMinPx : f32 = 2.5;
/* No real hemisphere is fully closed, and a screen-space estimate cannot tell a corner from a
 * silhouette. The floor is what keeps a wrong answer from being a black one. */
const kAoFloor : f32 = 0.25;
/* The scene projection is infinite reversed-Z with this near plane: depth = zn / viewDepth. */
const kZNear : f32 = 0.05;

struct VOut { @builtin(position) pos : vec4f, @location(0) ndc : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.ndc = p;
  return o;
}

/* Camera-relative position of the depth texel at integer coordinate `px`. */
fn aoPos(px : vec2i, dims : vec2i) -> vec3f {
  let d = textureLoad(depthTex, clamp(px, vec2i(0, 0), dims - vec2i(1, 1)), 0);
  if (d <= 0.0) { return vec3f(0.0, 0.0, 0.0); }
  let uv = (vec2f(px) + vec2f(0.5, 0.5)) / vec2f(dims);
  let nd = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let ray = camRay(A, nd);
  return ray * ((kZNear / d) / max(dot(ray, A.camFwd.xyz), 1.0e-4));
}

/* Project a camera-relative point back to the depth texture's integer grid. */
fn aoProject(p : vec3f, dims : vec2i) -> vec2i {
  let z = dot(p, A.camFwd.xyz);
  if (z <= kZNear) { return vec2i(-1, -1); }
  let x = dot(p, A.camRight.xyz) / (z * A.params.x * A.params.y) + A.view.z;
  let y = dot(p, A.camUp.xyz) / (z * A.params.x) + A.view.x + A.view.w;
  let uv = vec2f(x * 0.5 + 0.5, 0.5 - y * 0.5);
  return vec2i(uv * vec2f(dims));
}

fn aoHash(p : vec2f) -> f32 { return fract(sin(dot(p, vec2f(127.1, 311.7))) * 43758.5453); }

@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let dims = vec2i(textureDimensions(depthTex, 0));
  let uv = vec2f(in.ndc.x * 0.5 + 0.5, 0.5 - in.ndc.y * 0.5);
  let px = vec2i(uv * vec2f(dims));
  let d0 = textureLoad(depthTex, clamp(px, vec2i(0, 0), dims - vec2i(1, 1)), 0);
  if (d0 <= 0.0) { return vec4f(1.0, 1.0, 1.0, 1.0); }   /* sky: nothing to occlude */
  let p0 = aoPos(px, dims);
  /* Screen-space extent of the sample disc: A.params.x is tan(halfFov), so the radius in pixels is
   * r / (viewDepth * tan) * (height/2). */
  let viewZ = max(dot(p0, A.camFwd.xyz), 1.0e-3);
  if (kAoRadiusM / (viewZ * A.params.x) * (0.5 * f32(dims.y)) < kAoMinPx) {
    return vec4f(1.0, 1.0, 1.0, 1.0);
  }
  /* Normal from the depth field, taking the SMALLER of the two one-sided differences on each axis so
   * a silhouette does not tilt the normal into the background. */
  let pxr = aoPos(px + vec2i(1, 0), dims);
  let pxl = aoPos(px - vec2i(1, 0), dims);
  let pyu = aoPos(px + vec2i(0, 1), dims);
  let pyd = aoPos(px - vec2i(0, 1), dims);
  let dxv = select(p0 - pxl, pxr - p0, length(pxr - p0) < length(p0 - pxl));
  let dyv = select(p0 - pyd, pyu - p0, length(pyu - p0) < length(p0 - pyd));
  /* The cross product's sense follows the screen parametrisation, not the surface, and it comes out
   * pointing INTO the surface — measured on the reference frame at 100 % of shaded pixels. A visible
   * fragment's outward normal points back at the eye, and p0 is the eye-to-fragment vector, so that
   * is a test the parametrisation cannot lie about. Without it the estimator below reads the
   * hemisphere on the wrong side and darkens convexities instead of cavities. */
  let nrmRaw = normalize(cross(dyv, dxv));
  let nrmA = select(nrmRaw, -nrmRaw, dot(nrmRaw, p0) > 0.0);

  /* Golden-angle spiral, rotated per pixel: 16 taps carry no visible ring, and the half-resolution
   * target plus a linear read in the composite takes the rest of the noise out. */
  let rot = aoHash(vec2f(px)) * 6.28318530718;
  var occ = 0.0;
  for (var i = 0; i < kAoSamples; i = i + 1) {
    let fi = (f32(i) + 0.5) / f32(kAoSamples);
    let ang = rot + f32(i) * 2.39996323;
    let rr = kAoRadiusM * sqrt(fi);
    /* A cosine-weighted hemisphere direction about the normal, built from two tangents. */
    let tanA = normalize(cross(nrmA, select(vec3f(0.0, 0.0, 1.0), vec3f(1.0, 0.0, 0.0),
                                            abs(nrmA.z) > 0.9)));
    let tanB = cross(nrmA, tanA);
    let sp = p0 + (tanA * cos(ang) + tanB * sin(ang)) * rr
                + nrmA * (kAoRadiusM * sqrt(max(1.0 - fi, 0.0)) * 0.5);
    let spx = aoProject(sp, dims);
    if (spx.x < 0 || spx.y < 0 || spx.x >= dims.x || spx.y >= dims.y) { continue; }
    let sd = textureLoad(depthTex, spx, 0);
    if (sd <= 0.0) { continue; }
    /* The occluder is the SCENE point at the tap's screen position, not the tap itself: the estimator
     * is "how much of this point's hemisphere does the visible surface fill" (Alchemy AO), and the
     * two tests it needs are exactly the two below — inside the radius, above the tangent plane. A
     * depth comparison against the tap belongs to the other estimator, the one that asks whether the
     * TAP is buried; here it rejects every genuine occluder, because a correctly oriented normal puts
     * the tap in front of the surface and a real occluder in front of the tap. */
    let sceneP = aoPos(spx, dims);
    let toward = sceneP - p0;
    let dist = length(toward);
    if (dist > kAoRadiusM * 2.0) { continue; }       /* range check: a far wall is not contact */
    /* normalize(0) is NaN and NaN passes the cosA reject below, because NaN compares false. */
    if (dist < 1.0e-3) { continue; }
    let cosA = dot(normalize(toward), nrmA);
    if (cosA <= 0.05) { continue; }
    occ = occ + cosA * (1.0 - smoothstep(kAoRadiusM, kAoRadiusM * 2.0, dist));
  }
  let ao = clamp(1.0 - kAoStrength * occ / f32(kAoSamples), kAoFloor, 1.0);
  return vec4f(ao, ao, ao, 1.0);
}
)";

void AoStage::Configure(const Gpu &gpu, wgpu::TextureView depthView, wgpu::Buffer atmoBuf, int width,
                        int height) {
  Device = gpu.Device;
  /* Skipping the DRAW, not the pass: the target keeps its clear value of 1 and the frame's pass count
   * is the same number either way, which is what makes FB_GEOM a paired measurement. */
  Enabled = !GeometryIsolation();
  W = width / 2 > 0 ? width / 2 : 1;
  H = height / 2 > 0 ? height / 2 : 1;

  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)W, (uint32_t)H, 1};
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  Out = Device.CreateTexture(&td);

  wgpu::ShaderSourceWGSL wsl{};
  std::string src = std::string(kAtmoCommon) + kAoWGSL;
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = wgpu::TextureFormat::RGBA8Unorm;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[2] = {};
  be[0].binding = 0; be[0].textureView = depthView;
  be[1].binding = 1; be[1].buffer = atmoBuf; be[1].size = kAtmoUniformBytes;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 2;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
}

void AoStage::Encode(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  if (!Pipe || !Enabled) return;
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(3);
}

} // namespace outshine::Render
