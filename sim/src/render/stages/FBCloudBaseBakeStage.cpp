#include "FBCloudBaseBakeStage.h"
#include "FBCloudNoiseCommon.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace FlightBox {

static bool CloudCellsOn(void) { const char *e = getenv("FB_CLOUD_CELLS"); return !e || atoi(e) != 0; }

static const char *kCloudBaseCS = R"(
@group(0) @binding(0) var outTex : texture_storage_3d<rgba8unorm, write>;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 128u || id.y >= 128u || id.z >= 128u) { return; }
  let uv = (vec3f(id) + 0.5) / 128.0;
  /* Perlin-Worley DISPLACEMENT (Schneider 01 §3): inverted Worley forms the LOW END of a Perlin remap
   * -> keeps Perlin's connectedness (no planar facets) but gains Worley billow CONTRAST, so mid-coverage
   * forms solid cores with defined edges instead of a uniform thin sheet the detail shreds into slivers. */
  let pf = perlinFbm(uv, 3.0);
  let pw = 1.0 - worleyFbm(uv, 6.0);
  let raw = remap(pf, 0.0, 1.0, pw, 1.0);
  /* Contrast from a SMOOTH S-curve on the connected Perlin field (rounded billows, no angular Worley cell
   * ridges) — a light Worley touch adds billow without imprinting Voronoi edges. smoothstep gives the
   * dynamic range solid cores need while staying smooth (01 §3: keep Perlin's connectedness). */
  /* SOFT S-curve: a gentle contrast (not steep) so it doesn't band into corduroy/fold-lines. Opacity comes
   * from the extinction/density scale (optical depth), NOT from a sharp base transition -> decoupled. */
  let contrast = smoothstep(0.24, 0.64, pf);
  let base = clamp(mix(contrast, raw, 0.15), 0.0, 1.0);
  /* G = F1-ROUND 2D cell field (Candidate 12): (1-F1)^2 = round bumps at cell centres (NOT angular
   * F2-F1 Voronoi walls). Two octaves so cells vary in size (no uniform pyramids). Purely 2D -> columnar
   * (constant along the texture z) -> vertical towers. Drives horizontal discreteness + tower height. */
  let cellF = clamp(pow(worley2D(uv.xy, 9.0), 2.0) * 0.72 + pow(worley2D(uv.xy * 1.9 + 5.3, 5.0), 2.0) * 0.28, 0.0, 1.0);
  textureStore(outTex, vec3i(id), vec4f(base, cellF, 0.0, 0.0));
}
)";

static uint32_t MipCount(uint32_t n) { uint32_t m = 1; while (n > 1) { n >>= 1; m++; } return m; }

void FBCloudBaseBakeStage::Configure(const FBGpu &gpu, FBCloudMipDownStage &mipDown) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Instance = gpu.Instance;
  const uint32_t n = 128;

  wgpu::TextureDescriptor td{};
  td.dimension = wgpu::TextureDimension::e3D;
  td.size = {n, n, n};
  td.mipLevelCount = MipCount(n);   /* full mip chain: grazing rays sample coarse mips -> no over-Nyquist moiré */
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Tex = Device.CreateTexture(&td);

  /* Base-construction sweep hooks (ridge diagnosis): toggle the Worley displacement / octaves via env. */
  std::string baseCS = kCloudBaseCS;
  auto rep = [&](const std::string &from, const std::string &to) {
    auto p = baseCS.find(from); if (p != std::string::npos) baseCS.replace(p, from.size(), to);
  };
  if (getenv("FB_BASE_PERLIN_ONLY")) rep("remap(pf, 0.0, 1.0, pw, 1.0)", "pf");
  if (const char *pf = getenv("FB_BASE_PFREQ")) rep("perlinFbm(uv, 3.0)", "perlinFbm(uv, " + std::string(pf) + ")");
  if (const char *ww = getenv("FB_BASE_WORLEY_W"))
    rep("1.0 - worleyFbm(uv, 6.0)", "mix(0.5, 1.0 - worleyFbm(uv, 6.0), " + std::string(ww) + ")");
  if (const char *wf = getenv("FB_BASE_WFREQ")) rep("worleyFbm(uv, 6.0)", "worleyFbm(uv, " + std::string(wf) + ")");
  if (const char *sh = getenv("FB_BASE_STRETCH_HI")) rep("(0.82 - 0.55)", "(" + std::string(sh) + " - 0.55)");
  if (const char *lo = getenv("FB_SS_LO")) rep("smoothstep(0.24,", "smoothstep(" + std::string(lo) + ",");
  if (const char *hi = getenv("FB_SS_HI")) rep(" 0.64, pf)", " " + std::string(hi) + ", pf)");
  /* F1-cell field (B) is OFF by default: skip its worley2D generation entirely (the Stand-A density path
   * reads only .r) — no boot cost. FB_CLOUD_CELLS=1 keeps it (paired with CELLS_ON=1.0 in the march). */
  if (!CloudCellsOn())
    rep("clamp(pow(worley2D(uv.xy, 9.0), 2.0) * 0.72 + pow(worley2D(uv.xy * 1.9 + 5.3, 5.0), 2.0) * 0.28, 0.0, 1.0)", "0.0");

  auto mkmod = [&](const std::string &code) {
    wgpu::ShaderSourceWGSL w{};
    w.code = code.c_str();
    wgpu::ShaderModuleDescriptor smd{};
    smd.nextInChain = &w;
    return Device.CreateShaderModule(&smd);
  };

  /* mip 0: noise compute -> storage -> copy to sampled mip 0. Storage-usage 3D textures don't sample
   * trilinear on every backend (software Dawn falls back to nearest -> flat facets), so generate into
   * a storage volume, then copy into this sampled-only volume that filters correctly. */
  wgpu::TextureViewDescriptor vAll{}; vAll.dimension = wgpu::TextureViewDimension::e3D;
  auto mkStorage3d = [&](uint32_t sz) {
    wgpu::TextureDescriptor sd{};
    sd.dimension = wgpu::TextureDimension::e3D;
    sd.size = {sz, sz, sz};
    sd.format = wgpu::TextureFormat::RGBA8Unorm;
    sd.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::CopySrc;
    return Device.CreateTexture(&sd);
  };
  {
    wgpu::Texture tex = mkStorage3d(n);
    wgpu::ComputePipelineDescriptor cp{}; cp.compute.module = mkmod(std::string(kCloudNoiseCommon) + baseCS); cp.compute.entryPoint = "cs";
    wgpu::ComputePipeline pipe = Device.CreateComputePipeline(&cp);
    wgpu::BindGroupEntry be{}; be.binding = 0; be.textureView = tex.CreateView(&vAll);
    wgpu::BindGroupDescriptor bg{}; bg.layout = pipe.GetBindGroupLayout(0); bg.entryCount = 1; bg.entries = &be;
    wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = enc.BeginComputePass();
    pass.SetPipeline(pipe); pass.SetBindGroup(0, Device.CreateBindGroup(&bg)); pass.DispatchWorkgroups(n / 4, n / 4, n / 4); pass.End();
    wgpu::TexelCopyTextureInfo s{}, d{}; s.texture = tex; d.texture = Tex; wgpu::Extent3D ext{n, n, n};
    enc.CopyTextureToTexture(&s, &d, &ext);
    wgpu::CommandBuffer cmd = enc.Finish(); Queue.Submit(1, &cmd);
  }
  /* downsample each subsequent level from the sampled previous level */
  uint32_t lvl = 1, sz = n / 2;
  while (sz >= 1) {
    wgpu::Texture st = mkStorage3d(sz);
    wgpu::TextureViewDescriptor sv{}; sv.dimension = wgpu::TextureViewDimension::e3D; sv.baseMipLevel = lvl - 1; sv.mipLevelCount = 1;
    mipDown.Downsample(Tex.CreateView(&sv), st.CreateView(&vAll), sz);
    wgpu::TexelCopyTextureInfo s{}, d{}; s.texture = st; d.texture = Tex; d.mipLevel = lvl; wgpu::Extent3D ext{sz, sz, sz};
    wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
    enc.CopyTextureToTexture(&s, &d, &ext);
    wgpu::CommandBuffer cmd = enc.Finish(); Queue.Submit(1, &cmd);
    lvl++; sz >>= 1;
  }
}

/* Numeric shape/density histogram: evaluate the SAME base-shape math density() uses over a 3D grid in
 * the shell and read back the values, so base dynamic-range tuning is driven by numbers, not eyes.
 * Reports shape percentiles AND the post-coverage-remap density d = (shape-(1-cov))/cov percentiles —
 * the goal (per convergence guidance) is cloud CORES at d~1 with only edges in low d. */
void FBCloudBaseBakeStage::ShapeStats(float cover, float low, float high) {
  const uint32_t GX = 128, GY = 128, GZ = 32, N = GX * GY * GZ;
  static const char *kHistCS = R"(
struct P { cov : f32, low : f32, high : f32, pad : f32 };
@group(0) @binding(0) var baseTex : texture_3d<f32>;
@group(0) @binding(1) var samp : sampler;
@group(0) @binding(2) var<storage, read_write> outv : array<f32>;
@group(0) @binding(3) var<uniform> C : P;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 128u || id.y >= 128u || id.z >= 32u) { return; }
  let h = (f32(id.z) + 0.5) / 32.0;
  let posKm = vec3f(f32(id.x) * 0.31, f32(id.y) * 0.29, 2.0 + h * 2.6);   /* ~40x37 km patch, deck altitudes */
  let b0 = textureSampleLevel(baseTex, samp, posKm / 9.0, 0.0).r;
  let b1 = textureSampleLevel(baseTex, samp, posKm / 24.0, 0.0).r;
  let b = b1 * 0.6 + b0 * 0.4;
  let thresh = 1.0 - C.cov;
  var d = 0.0;
  if (b > thresh) {   /* mirror density(): low-freq tower height + dome envelope, no detail erosion */
    let cov = (b - thresh) / (1.0 - thresh);
    let topMax = mix(1.0 + 0.15 * C.high, 0.5, C.low);
    let topH = mix(0.35, topMax, pow(clamp((b1 - thresh) / (1.0 - thresh), 0.0, 1.0), 1.5));
    let hNorm = h / max(topH, 0.05);
    if (hNorm < 1.0) {
      let wispBase = smoothstep(0.0, 0.12, hNorm);
      let topFall = smoothstep(1.0, 0.72, hNorm);
      d = cov * wispBase * topFall;
    }
  }
  outv[id.x + id.y * 128u + id.z * 128u * 128u] = d;
}
)";
  wgpu::ShaderSourceWGSL w{};
  w.code = kHistCS;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &w;
  wgpu::ShaderModule mod = Device.CreateShaderModule(&smd);
  wgpu::ComputePipelineDescriptor pd{};
  pd.compute.module = mod;
  pd.compute.entryPoint = "cs";
  wgpu::ComputePipeline pipe = Device.CreateComputePipeline(&pd);

  wgpu::BufferDescriptor sbd{};
  sbd.size = (uint64_t)N * sizeof(float);
  sbd.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
  wgpu::Buffer store = Device.CreateBuffer(&sbd);

  float uni[4] = {cover, low, high, 0.0f};
  wgpu::BufferDescriptor ubd{};
  ubd.size = sizeof(uni);
  ubd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer ubuf = Device.CreateBuffer(&ubd);
  Queue.WriteBuffer(ubuf, 0, uni, sizeof(uni));

  /* fresh sampler matching FBCloudMarchStage's CloudSamp descriptor (repeat/linear) — sampler behaviour
   * is defined entirely by the descriptor, not object identity, so a diagnostic tool owning its own is fine. */
  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = wgpu::AddressMode::Repeat;
  sd.addressModeV = wgpu::AddressMode::Repeat;
  sd.addressModeW = wgpu::AddressMode::Repeat;
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  wgpu::Sampler samp = Device.CreateSampler(&sd);

  wgpu::TextureViewDescriptor vd{};
  vd.dimension = wgpu::TextureViewDimension::e3D;
  wgpu::BindGroupEntry be[4]{};
  be[0].binding = 0; be[0].textureView = Tex.CreateView(&vd);
  be[1].binding = 1; be[1].sampler = samp;
  be[2].binding = 2; be[2].buffer = store;
  be[3].binding = 3; be[3].buffer = ubuf;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = pipe.GetBindGroupLayout(0);
  bg.entryCount = 4;
  bg.entries = be;
  wgpu::BindGroup bind = Device.CreateBindGroup(&bg);

  wgpu::BufferDescriptor rbd{};
  rbd.size = (uint64_t)N * sizeof(float);
  rbd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
  wgpu::Buffer readb = Device.CreateBuffer(&rbd);

  wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
  wgpu::ComputePassEncoder pass = enc.BeginComputePass();
  pass.SetPipeline(pipe);
  pass.SetBindGroup(0, bind);
  pass.DispatchWorkgroups(GX / 4, GY / 4, GZ / 4);
  pass.End();
  enc.CopyBufferToBuffer(store, 0, readb, 0, (uint64_t)N * sizeof(float));
  wgpu::CommandBuffer cmd = enc.Finish();
  Queue.Submit(1, &cmd);

  bool ok = false;
  Instance.WaitAny(readb.MapAsync(wgpu::MapMode::Read, 0, (uint64_t)N * sizeof(float),
      wgpu::CallbackMode::WaitAnyOnly, [&ok](wgpu::MapAsyncStatus st, wgpu::StringView) { ok = (st == wgpu::MapAsyncStatus::Success); }),
      UINT64_MAX);
  if (!ok) { printf("[shapehist] readback failed\n"); return; }
  const float *v = static_cast<const float *>(readb.GetConstMappedRange(0, (uint64_t)N * sizeof(float)));
  std::vector<float> s(v, v + N);
  readb.Unmap();
  std::sort(s.begin(), s.end());
  auto pct = [&](double p) { return s[(size_t)(p * (N - 1))]; };
  size_t cloud = 0, solid = 0;
  for (float x : s) { if (x > 0.001f) cloud++; if (x > 0.9f) solid++; }
  /* percentiles over CLOUD voxels (d>0) — the whole-grid median is ~0 since most of the shell is clear */
  size_t c0 = N - cloud;
  auto cpct = [&](double p) { return s[c0 + (size_t)(p * (cloud > 0 ? cloud - 1 : 0))]; };
  printf("[shapehist] cov=%.2f low=%.2f high=%.2f  N=%u (post-dome density d, no detail erosion)\n", cover, low, high, N);
  printf("[shapehist] d over CLOUD voxels: p10=%.3f median=%.3f p90=%.3f max=%.3f\n",
         cpct(0.10), cpct(0.50), cpct(0.90), s.back());
  printf("[shapehist] cloud voxels (d>0): %.1f%%  |  SOLID cores (d>0.9): %.2f%%\n",
         100.0 * cloud / N, 100.0 * solid / N);
  (void)pct;
}

} // namespace FlightBox
