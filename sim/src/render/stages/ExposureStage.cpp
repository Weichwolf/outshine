#include "ExposureStage.h"
#include "SceneScale.h"
#include "SurfaceLight.h"

#include <string>

namespace outshine::Render {

static const char *kExposureCS = R"(
struct Meter {
  outBlack : f32,
  outWhite : f32,
  contrast : f32,
  adaptLog : f32,
  blackLog : f32,
  whiteLog : f32,
  horizE : f32,
  pad0 : f32,
};
struct ExpoCfg { mode : u32, keyEv : f32, compEv : f32, pad1 : f32 };

@group(0) @binding(0) var<storage, read> irr : Irr;
@group(0) @binding(1) var<storage, read_write> meter : Meter;
@group(0) @binding(2) var<uniform> cfg : ExpoCfg;

/* THE ADAPTATION LUMINANCE, the one quantity the anchors are ratios of. Pattanaik et al. 2000 §4.1.2
 * take it for still images from a KNOWN REFLECTANCE UNDER THE SCENE LIGHT — one fifth of a Macbeth
 * chart's paper-white patch — and meter the picture only for their driving sequence, foveally and
 * aimed by hand. kGroundBounce IS that known reflectance without the chart: the measured mean visible
 * reflectance of Central European land cover. Lambert then gives the radiance. */

/* Reference black, from Pattanaik et al. §4.2 verbatim: "reference white as five times the current
 * adaptation level and reference black as 1/32 the intensity of reference white", i.e. A/6.4 —
 * log2 6.4 = 2.678 stops under A. Checked against the meter it replaces: that meter's black anchor
 * for this scene sat at -6.723 and this rule puts it at -6.535, agreement to 0.19 EV. */
const kBlackBelowA : f32 = 2.678072;

/* THE SPAN IS NOT THE SAME PAPER'S 32:1. That figure is its DISPLAY's contrast (§4.3: reference
 * white 125 cd/m2, reference black 4 cd/m2 on a 1995 CRT), and transplanting a display's contrast
 * onto the scene side costs, measured on this scene's own HDR histogram, 50.5 % of the frame past
 * 250/255. What the scene is mapped INTO here is 8-bit sRGB, and that container's own range is
 * derived and not chosen: code 255 decodes to 1.0, code 1 to (1/255)/12.92 = 3.0356e-4, a ratio of
 * 3294 = 11.686 stops. The frame's own metered span was 10.43 EV, so this errs toward not clipping. */
const kSpanEv : f32 = 11.686;

/* WHERE A LANDS ON THE DISPLAY, and it is the same paper's own instantiation: adaptation luminance
 * 25 cd/m2 against a reference white of 125, i.e. one fifth of display white, display-LINEAR. The
 * superseded meter's key target 0.035 belonged to a DIFFERENT quantity — the log-average of the
 * darkest 5-45 % of the picture, which this scene measured 1.69 EV below A — and applying it to A
 * underexposes: solved against the same histogram it gives a lower-half mean of about 20/255 against
 * a band of 45. That is which of the two numbers was in the wrong system. */
const kAdaptDisplay : f32 = 0.2;

@compute @workgroup_size(1, 1, 1)
fn cs() {
  /* The floor is the residual illumination SurfaceLight.h already adds to every lit surface, so a
   * night scene's anchor rests on the same number its ground does instead of on log2(0). */
  let e = max(irr.sky.w, kNightAmbient);
  let adaptLog = log2(kGroundBounce * e * kInvPi * kSceneExposure);
  let blackLog = adaptLog - kBlackBelowA;
  let whiteLog = blackLog + kSpanEv;

  meter.horizE = e;
  meter.adaptLog = adaptLog;
  meter.blackLog = blackLog;
  meter.whiteLog = whiteLog;
  /* The exponent is not a look setting: it is what puts A at kAdaptDisplay inside a span whose
   * position A already occupies by construction. */
  meter.contrast = log(kAdaptDisplay) / log(kBlackBelowA / kSpanEv);

  /* A SLIDE and not a factor on the target: written as a factor, +2 EV came out as exponent 0.86
   * against 1.47 — brighter, with the contrast washed out of it, which is not what a stop does.
   * Manual declares the scene radiance that shall sit at the adaptation luminance. */
  let slide = select(cfg.compEv, adaptLog - cfg.keyEv, cfg.mode == 1u);
  meter.outBlack = blackLog - slide;
  meter.outWhite = whiteLog - slide;
}
)";

void ExposureStage::Configure(const Gpu &gpu, wgpu::Buffer meterBuf, wgpu::Buffer irrBuf) {
  Queue = gpu.Queue;

  wgpu::BufferDescriptor cd{};
  cd.size = 16;
  cd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  CfgBuf = gpu.Device.CreateBuffer(&cd);

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + kExposureCS;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = gpu.Device.CreateShaderModule(&smd);

  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = sm;
  cp.compute.entryPoint = "cs";
  Pipe = gpu.Device.CreateComputePipeline(&cp);

  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].buffer = irrBuf;   be[0].size = wgpu::kWholeSize;
  be[1].binding = 1; be[1].buffer = meterBuf; be[1].size = kMeterBytes;
  be[2].binding = 2; be[2].buffer = CfgBuf;   be[2].size = 16;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 3;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void ExposureStage::EncodeCompute(const FrameContext &, wgpu::ComputePassEncoder &pass) {
  if (!Pipe) return;

  struct Cfg {
    uint32_t Mode;
    float KeyEv, CompEv, Pad;
  } cfg{};
  cfg.Mode = Params.Mode == ExposureMode::Manual ? 1u : 0u;
  cfg.KeyEv = Params.KeyEv;
  cfg.CompEv = Params.CompEv;
  Queue.WriteBuffer(CfgBuf, 0, &cfg, sizeof(cfg));

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.DispatchWorkgroups(1, 1, 1);
}

} // namespace outshine::Render
