#include "IrradianceStage.h"
#include "AtmoCommon.h"
#include <string>

namespace outshine::Render {

static const char *kIrradianceCS = R"(
struct Irr { sun : vec4f, sky : vec4f, sunDeck : vec4f };
@group(0) @binding(0) var<storage, read_write> irr : Irr;
@group(0) @binding(1) var svLUT : texture_2d<f32>;
@group(0) @binding(2) var tLUT : texture_2d<f32>;
@group(0) @binding(3) var lsamp : sampler;
@group(0) @binding(4) var<uniform> A : Atmo;

const kAzSteps : i32 = 16;
const kElSteps : i32 = 8;

/* The sky-view LUT is addressed by (azimuth about the sun, a horizon-dense elevation), so the
 * hemisphere integral needs no world vectors at all — only the two angles. */
fn skyRadiance(altR : f32, azR : f32) -> vec3f {
  let coord = sign(altR) * sqrt(abs(altR) / (PI * 0.5));
  let uv = vec2f(azR / (2.0 * PI), (coord + 1.0) * 0.5);
  return textureSampleLevel(svLUT, lsamp, uv, 0.0).rgb;
}

const kTauSteps : i32 = 16;

/* Optical depth of the segment p0 -> p1, in the units getScatteringValues works in (1/Mm against a
 * position in Mm), so the result is dimensionless. */
fn columnTau(p0 : vec3f, p1 : vec3f) -> vec3f {
  let d = p1 - p0;
  let seg = length(d) / f32(kTauSteps);
  var tau = vec3f(0.0);
  for (var i = 0; i < kTauSteps; i = i + 1) {
    tau = tau + getScatteringValues(p0 + d * ((f32(i) + 0.5) / f32(kTauSteps))).extinction * seg;
  }
  return tau;
}

@compute @workgroup_size(1, 1, 1)
fn cs() {
  /* E = int L cos(theta) dw over the upper hemisphere. Substituting u = sin(alt) makes the cosine
   * weight and the solid-angle Jacobian one factor of u, so a uniform grid in (u, azimuth) is an
   * unbiased estimator with weight u and no sin/cos in the loop. */
  var e = vec3f(0.0);
  for (var ia = 0; ia < kAzSteps; ia = ia + 1) {
    let az = 2.0 * PI * (f32(ia) + 0.5) / f32(kAzSteps);
    for (var ie = 0; ie < kElSteps; ie = ie + 1) {
      let u = (f32(ie) + 0.5) / f32(kElSteps);
      e = e + skyRadiance(asin(clamp(u, 0.0, 1.0)), az) * u;
    }
  }
  let skyE = e * (2.0 * PI / f32(kAzSteps * kElSteps));
  /* Direct normal: the transmittance from the camera's altitude to the sun, times the TOA
   * irradiance the whole model normalises to 1. Below the horizon the LUT is already zero. */
  let camP = atmoPos(A);
  let tr = textureSampleLevel(tLUT, lsamp, tLUTuv(camP, A.sunDir.xyz), 0.0).rgb;
  irr.sun = vec4f(tr, 0.0);
  /* WHAT THE DECK IS LIT BY, which is not what the ground is lit by. The deck's downward re-emission
   * used the beam measured HERE — reddened by the 6 km of slant path through dense air that the deck
   * stands above — and that reddening reached the ground a second time. So: take the slant leg back
   * off and put the VERTICAL leg on, because what comes down off a deck is a hemisphere flux and not
   * an 11-degree beam.
   *
   * MARCHED, not tapped: the transmittance LUT has 64 rows over 100 km, so its first two texel
   * CENTRES are 781 m and 2344 m and a 1 200 m deck base falls inside one of them. Tapping it gave a
   * blue lift of 4.7 % where the model's own extinction gives 19.6 % — the LUT cannot resolve this
   * question. The march uses getScatteringValues, i.e. the SAME model the LUT is baked from, and it
   * runs once per frame in a one-thread dispatch. */
  let upA = normalize(camP);
  let camAltM = (length(camP) - groundRadiusMM) * 1.0e6;
  let deckR = groundRadiusMM + max(A.view.y, camAltM) * 1.0e-6;
  let tSlant = rayIntersectSphere(camP, A.sunDir.xyz, deckR);
  var below = tr;
  if (A.view.y > camAltM + 1.0 && tSlant > 0.0) {
    let tauSlant = columnTau(camP, camP + A.sunDir.xyz * tSlant);
    let tauVert = columnTau(camP, upA * deckR);
    below = tr * exp(tauSlant - tauVert);
  }
  irr.sunDeck = vec4f(below, A.view.y);
  /* THE EXPOSURE ANCHOR'S ONE INPUT, published here and not recomputed downstream: E on a horizontal
   * surface, as luminance. It is a property of sun elevation and air, so it is the same number
   * whichever way the camera looks — which is the whole reason ExposureStage reads it. */
  let lw = vec3f(0.2126, 0.7152, 0.0722);
  let sunUp = max(dot(A.sunDir.xyz, A.up.xyz), 0.0);
  irr.sky = vec4f(skyE, dot(tr, lw) * sunUp + dot(skyE, lw));
}
)";

void IrradianceStage::Configure(const Gpu &gpu, wgpu::Buffer irrBuf, wgpu::TextureView skyLutView,
                                wgpu::TextureView transLutView, wgpu::Sampler lutSamp,
                                wgpu::Buffer atmoBuf) {
  wgpu::ShaderSourceWGSL wgsl{};
  std::string src = std::string(kAtmoCommon) + kIrradianceCS;
  wgsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule m = gpu.Device.CreateShaderModule(&smd);

  wgpu::ComputePipelineDescriptor cp{};
  cp.compute.module = m;
  cp.compute.entryPoint = "cs";
  Pipe = gpu.Device.CreateComputePipeline(&cp);

  wgpu::BindGroupEntry be[5] = {};
  be[0].binding = 0; be[0].buffer = irrBuf; be[0].size = kBufferBytes;
  be[1].binding = 1; be[1].textureView = skyLutView;
  be[2].binding = 2; be[2].textureView = transLutView;
  be[3].binding = 3; be[3].sampler = lutSamp;
  be[4].binding = 4; be[4].buffer = atmoBuf; be[4].size = kAtmoUniformBytes;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 5;
  bg.entries = be;
  Bind = gpu.Device.CreateBindGroup(&bg);
}

void IrradianceStage::EncodeCompute(const FrameContext &, wgpu::ComputePassEncoder &pass) {
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.DispatchWorkgroups(1, 1, 1);
}

} // namespace outshine::Render
