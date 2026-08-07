#include "TonemapStage.h"
#include "ExposureStage.h"
#include "GeometryIsolation.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace outshine::Render {

static const char *kTonemapWGSL = R"(
struct Meter {
  outBlack : f32,
  outWhite : f32,
  contrast : f32,
  keyLog : f32,
  blackLog : f32,
  whiteLog : f32,
  valid : f32,
  pad0 : f32,
};
@group(0) @binding(0) var samp : sampler;
@group(0) @binding(1) var hdr : texture_2d<f32>;
@group(0) @binding(2) var aoTex : texture_2d<f32>;
@group(0) @binding(3) var<storage, read> meter : Meter;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f };
@vertex fn vs(@builtin(vertex_index) i : u32) -> VOut {
  var c = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0)); // fullscreen tri
  var o : VOut;
  let p = c[i];
  o.pos = vec4f(p, 0.0, 1.0);
  o.uv = vec2f((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);
  return o;
}
/* A LOG-DOMAIN curve, and not the ACES fit that stood here, because the fit could not do this scene:
 * measured, the demo frame spans 11.8 EV (2^-7.89 .. 2^3.92) and its ground sits 8 stops under its
 * sky. Narkowicz ACES saturates at an input of 7.2 and Hable's shoulder asymptotes at 0.933 — solved
 * against the measured histogram, BOTH put more than 4 % of the frame past 250/255 at the gain the
 * foreground needs, whatever their white point. Compressing six stops of sky into the top of the
 * range needs a logarithmic response there, so the whole curve is one.
 *
 * The two anchors and the exponent are METERED (stages/ExposureStage.h); this shader has no constant
 * of its own left. Applied to LUMINANCE and not per channel: measured, the per-channel form put the
 * blue channel of 99.96 % of the foreground at exactly 0 — a low sun is 6:3:1, so at these levels
 * blue falls below the black anchor while luminance does not, and the field came out rust. It is
 * also what makes the meter and the curve read the same quantity. */
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let scene = textureSample(hdr, samp, in.uv);
  // alpha = the DIRECT fraction of this pixel's radiance; occlusion darkens only the rest. The AO
  // target is half resolution and read with the same linear sampler, which is its blur.
  let aoAll = textureLoad(aoTex, vec2i(in.pos.xy * 0.5), 0);
  let ao = aoAll.r;
  let lit = scene.rgb * mix(ao, 1.0, clamp(scene.a, 0.0, 1.0));
  let lum = max(dot(lit, vec3f(0.2126, 0.7152, 0.0722)), 1e-7);
  let ob = mix(meter.outBlack, kFrozenBlack, kFreeze);
  let span = max(mix(meter.outWhite, kFrozenWhite, kFreeze) - ob, 1.0);
  let tlin = (log2(lum) - ob) / span;
  // THE TOE. Below kToe the log-linear ramp is replaced by an exponential foot of the same value and
  // the same slope at the knee, so nothing above the knee moves by a bit and nothing below reaches
  // zero. kToe = 0 restores the hard clamp exactly.
  let ktoe = max(mix(kToe, kFrozenToe, kFreeze), 1.0e-6);
  let t = clamp(select(tlin, ktoe * exp(tlin / ktoe - 1.0), tlin < ktoe), 0.0, 1.0);
  let out = pow(t, mix(meter.contrast, kFrozenContrast, kFreeze));
  // Chroma rides along untouched through the shadows and mid-tones and is let go of toward white:
  // without this the curve compresses LUMINANCE while the ratio carries a channel straight past 1,
  // measured as 3.38 % of the frame at 250+ against a 1.93 % luminance clip. 0.6 is where the roll
  // starts; below it nothing in this scene's ground or sky is touched.
  let desat = smoothstep(0.6, 1.0, out);
  let rgb = mix(lit * (out / lum), vec3f(out), desat);
  let peak = max(max(rgb.r, rgb.g), rgb.b);
  let pull = select(1.0, (1.0 - out) / max(peak - out, 1e-5), peak > 1.0);
  return vec4f(clamp(vec3f(out) + (rgb - vec3f(out)) * pull, vec3f(0.0), vec3f(1.0)), 1.0);
}
)";

void TonemapStage::Configure(const Gpu &gpu, wgpu::Sampler samp, wgpu::TextureView sceneEven,
                             wgpu::TextureView sceneOdd, wgpu::TextureView aoView,
                             wgpu::Buffer meterBuf) {
  /* FB_GEOM freezes the metered curve at the values this scene METERED before the DAG existed
   * (walk --bench 500: blackLog2 -8.20303, whiteLog2 3.51562, contrast 1.38095), so a geometry change
   * cannot move the exposure and then be read back off the PNG as a geometry difference. */
  /* The freeze is about the ANCHORS; the toe is not one, so FB_GEOM keeps the shipped curve's foot. */
  double fb = -8.20303, fw = 3.51562, fc = 1.38095, ft = kToe;
  bool freeze = GeometryIsolation();
  /* FB_TONE_PROBE=black,white — the curve as a RULER: exponent 1, no toe, so the frame's own display
   * luminance IS (log2 L - black)/(white - black) and a PNG read back through the sRGB decode is the
   * scene's HDR histogram. The desaturation above 0.6 preserves luminance by construction, so it does
   * not disturb the reading. It is the only way to measure the population BELOW the black anchor,
   * where the shipped curve has no inverse. */
  if (const char *e = getenv("FB_TONE_PROBE")) {
    double b = 0.0, w = 0.0;
    if (std::sscanf(e, "%lf,%lf", &b, &w) == 2 && w > b) {
      fb = b; fw = w; fc = 1.0; ft = 0.0; freeze = true;
    }
  }
  const std::string src = std::string(freeze ? "const kFreeze : f32 = 1.0;\n"
                                             : "const kFreeze : f32 = 0.0;\n")
                        + "const kFrozenBlack : f32 = " + std::to_string(fb) + ";\n"
                          "const kFrozenWhite : f32 = " + std::to_string(fw) + ";\n"
                          "const kFrozenContrast : f32 = " + std::to_string(fc) + ";\n"
                          "const kFrozenToe : f32 = " + std::to_string(ft) + ";\n"
                          "const kToe : f32 = " + std::to_string(kToe) + ";\n"
                        + kTonemapWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = gpu.Device.CreateShaderModule(&smd);

  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;
  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;   /* no vertex buffers — the triangle comes from vertex_index */
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;       /* no depthStencil on the tonemap pass */
  TonemapPipe = gpu.Device.CreateRenderPipeline(&rp);

  for (int i = 0; i < 2; i++) {
    wgpu::BindGroupEntry be[4] = {};
    be[0].binding = 0; be[0].sampler = samp;
    be[1].binding = 1; be[1].textureView = i ? sceneOdd : sceneEven;
    be[2].binding = 2; be[2].textureView = aoView;
    be[3].binding = 3; be[3].buffer = meterBuf; be[3].size = ExposureStage::kMeterBytes;
    wgpu::BindGroupDescriptor bgd{};
    bgd.layout = TonemapPipe.GetBindGroupLayout(0);
    bgd.entryCount = 4;
    bgd.entries = be;
    TonemapBind[i] = gpu.Device.CreateBindGroup(&bgd);
  }
}

void TonemapStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  pass.SetPipeline(TonemapPipe);
  pass.SetBindGroup(0, TonemapBind[ctx.FrameNo & 1u]);
  pass.Draw(3);
}

} // namespace outshine::Render
