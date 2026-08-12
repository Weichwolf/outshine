#include "ExposureStage.h"
#include "Filmic.h"
#include "SceneScale.h"
#include "SurfaceLight.h"

#include <string>

namespace outshine::Render {

static const char *kExposureCS = R"(
struct Meter { expScale : f32, keyLog : f32, horizE : f32, pad0 : f32 };
struct ExpoCfg { mode : u32, keyEv : f32, compEv : f32, pad1 : f32 };

@group(0) @binding(0) var<storage, read> irr : Irr;
@group(0) @binding(1) var<storage, read_write> meter : Meter;
@group(0) @binding(2) var<uniform> cfg : ExpoCfg;

@compute @workgroup_size(1, 1, 1)
fn cs() {
  /* The floor is the residual illumination SurfaceLight.h already adds to every lit surface, so a
   * night scene's key rests on the same number its ground does instead of on log2(0). */
  let e = max(irr.sky.w, kNightAmbient);
  /* THE KEY, and it is a surface and not a histogram: the scene's own mean land cover — kGroundBounce
   * is the measured mean visible reflectance of Central European land cover — lit by THIS frame's
   * horizontal irradiance and seen face-on. Lambert gives the radiance, kFilmicMid says where the
   * curve wants it, and the quotient is the whole exposure. It cannot depend on where the camera
   * points, because turning the head does not change the physics of the scene.
   *
   * What stood here was an eleven-stop log ramp between two anchors. Its span was a DISPLAY range
   * (sRGB code 1 to code 255, 11.686 EV) transplanted onto the scene side, and against this scene it
   * put the white anchor at scene radiance 2^7.51 = 182 while the brightest pixel in the frame was
   * 1.5 — 6.9 stops of empty headroom, which is why L > 200 was measured at exactly 0.000 in six of
   * six reference frames. A display's range is not a scene's range. */
  let keyL = kGroundBounce * e * kInvPi * kSceneExposure;
  let anchor = select(keyL * exp2(-cfg.compEv), exp2(cfg.keyEv), cfg.mode == 1u);
  meter.expScale = kFilmicMid / max(anchor, 1.0e-9);
  meter.keyLog = log2(max(anchor, 1.0e-9));
  meter.horizE = e;
  meter.pad0 = 0.0;
}
)";

void ExposureStage::Configure(const Gpu &gpu, wgpu::Buffer meterBuf, wgpu::Buffer irrBuf) {
  Queue = gpu.Queue;

  wgpu::BufferDescriptor cd{};
  cd.size = 16;
  cd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  CfgBuf = gpu.Device.CreateBuffer(&cd);

  const std::string src =
      std::string(kSceneScaleWGSL) + kFilmicWGSL + kSurfaceLightWGSL + kExposureCS;
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
