#include "FBNvisStage.h"

#include <cmath>

#include "FBCamera.h"

namespace FlightBox::Render {

namespace {
/* THE TUBE'S REFERENCE RADIANCE, in the scene pass's own linear units: the transfer is
 * I = 1 - exp(-L / kNvisRefL), a saturating amplifier with exactly one number in it, and at L = this
 * number the tube stands at 1 - 1/e = 63 % of full output.
 * MEASURED, not chosen. `gpu_native --mission missions/vis-night.fbm --interval 40 --albedo photo`
 * (2026-06-21T23:00Z over 46.6 N / 6.9 E, 3000 m); over the 1240x170 px ground band of the frame the
 * displayed luminance was inverted through the Narkowicz ACES fit the tonemap applies, giving a scene
 * radiance of p10 = 3.0e-3, median = 1.42e-2, p90 = 2.15e-2. Putting the reference AT the median
 * spreads that decade over 0.19 .. 0.79 of the tube's range instead of clipping it. */
constexpr float kNvisRefL = 1.4e-2f;
} // namespace

static const char *kNvisWGSL = R"(
struct NU {
  bay   : vec4f,   // the whole bay, x0, y0, x1, y1 in frame pixels — the drawn quad
  vid   : vec4f,   // the picture inside it; between the two the bay is simply dark
  optic : vec4f,   // tan(azHalf), tan(elHalf), f = 1/tan(sceneFov/2), viewShift (NDC)
  frame : vec4f,   // 2/width, 2/height, aspect = width/height, half-saturation radiance
};
@group(0) @binding(0) var<uniform> nu : NU;
@group(0) @binding(1) var nsamp : sampler;
@group(0) @binding(2) var nhdr : texture_2d<f32>;
struct NVO { @builtin(position) pos : vec4f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> NVO {
  var q = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
                          vec2f(1.0, 0.0), vec2f(1.0, 1.0), vec2f(0.0, 1.0));
  let t = q[i];
  var o : NVO;
  o.pos = vec4f(mix(nu.bay.x, nu.bay.z, t.x) * nu.frame.x - 1.0,
                1.0 - mix(nu.bay.y, nu.bay.w, t.y) * nu.frame.y, 0.0, 1.0);
  return o;
}
@fragment fn fs(in : NVO) -> @location(0) vec4f {
  if (in.pos.x < nu.vid.x || in.pos.x > nu.vid.z || in.pos.y < nu.vid.y || in.pos.y > nu.vid.w) {
    return vec4f(0.0, 0.0, 0.0, 1.0);
  }
  /* The picture is a gnomonic crop of the SAME projection the windscreen uses, off-centre term and
   * all — otherwise the tube looks somewhere the aircraft is not pointing. */
  let sx = (in.pos.x - nu.vid.x) / (nu.vid.z - nu.vid.x) * 2.0 - 1.0;
  let sy = 1.0 - (in.pos.y - nu.vid.y) / (nu.vid.w - nu.vid.y) * 2.0;
  let ndc = vec2f(nu.optic.z / nu.frame.z * (sx * nu.optic.x),
                  nu.optic.z * (sy * nu.optic.y) + nu.optic.w);
  let uv = vec2f((ndc.x + 1.0) * 0.5, (1.0 - ndc.y) * 0.5);
  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { return vec4f(0.0, 0.0, 0.0, 1.0); }
  let rad = textureSampleLevel(nhdr, nsamp, uv, 0.0).rgb;
  let lum = dot(rad, vec3f(0.2126, 0.7152, 0.0722));
  let inten = 1.0 - exp(-max(lum, 0.0) / nu.frame.w);
  /* P43 phosphor: one channel of information, so one hue and only its brightness varies. */
  let disp = vec3f(0.20, 1.0, 0.42) * inten;
  return vec4f(pow(disp, vec3f(2.2)), 1.0);
}
)";

void FBNvisStage::Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::TextureView hdrView) {
  Queue = gpu.Queue;

  wgpu::BufferDescriptor bd{};
  bd.size = 64;   /* four vec4f — matches NU exactly, so minBindingSize cannot drift */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = gpu.Device.CreateBuffer(&bd);

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kNvisWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = gpu.Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  Pipe = gpu.Device.CreateRenderPipeline(&rp);

  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = bd.size;
  be[1].binding = 1; be[1].sampler = samp;
  be[2].binding = 2; be[2].textureView = hdrView;
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 3;
  bgd.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bgd);
}

void FBNvisStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!Have || !Pipe) return;
  Systems::FBMfdBayRect vid = Systems::FBNvisRect(Systems::FBMfdBodyOf(Bay));
  const float k = 3.14159265f / 180.f;
  float f = 1.0f / std::tan(kSceneVerticalFovDeg * 0.5f * k);
  float shift = 1.0f - (float)ctx.ViewH / (float)ctx.Height;
  float u[16] = {Bay.X0, Bay.Y0, Bay.X1, Bay.Y1,
                 vid.X0, vid.Y0, vid.X1, vid.Y1,
                 std::tan(Systems::FBNvisAzHalfDeg(vid) * k), std::tan(Systems::kNvisElHalfDeg * k), f, shift,
                 2.0f / (float)ctx.Width, 2.0f / (float)ctx.Height,
                 (float)ctx.Width / (float)ctx.Height, kNvisRefL};
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.Draw(6);
}

} // namespace FlightBox::Render
