#include "FBCloudDetailBakeStage.h"
#include "FBCloudNoiseCommon.h"
#include <string>

namespace FlightBox::Render {

static const char *kCloudDetailCS = R"(
@group(0) @binding(0) var outTex : texture_storage_3d<rgba8unorm, write>;
@compute @workgroup_size(4, 4, 4)
fn cs(@builtin(global_invocation_id) id : vec3u) {
  if (id.x >= 32u || id.y >= 32u || id.z >= 32u) { return; }
  let uv = (vec3f(id) + 0.5) / 32.0;
  /* 32^3 Nyquist is ~16 cells; worleyFbm internally reaches 4*f, so keep base freq <= 4 or the stored
   * detail is aliased HF garbage (radial streaks when it erodes the cloud). Fine scale comes from the
   * /0.28 km world-space sampling, not from over-cranking the generator frequency. */
  textureStore(outTex, vec3i(id), vec4f(worleyFbm(uv, 2.0), worleyFbm(uv, 3.0), worleyFbm(uv, 4.0), 1.0));
}
)";

static uint32_t MipCount(uint32_t n) { uint32_t m = 1; while (n > 1) { n >>= 1; m++; } return m; }

void FBCloudDetailBakeStage::Configure(const FBGpu &gpu, FBCloudMipDownStage &mipDown) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  const uint32_t n = 32;

  wgpu::TextureDescriptor td{};
  td.dimension = wgpu::TextureDimension::e3D;
  td.size = {n, n, n};
  td.mipLevelCount = MipCount(n);
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Tex = Device.CreateTexture(&td);

  auto mkmod = [&](const std::string &code) {
    wgpu::ShaderSourceWGSL w{};
    w.code = code.c_str();
    wgpu::ShaderModuleDescriptor smd{};
    smd.nextInChain = &w;
    return Device.CreateShaderModule(&smd);
  };

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
    wgpu::ComputePipelineDescriptor cp{}; cp.compute.module = mkmod(std::string(kCloudNoiseCommon) + kCloudDetailCS); cp.compute.entryPoint = "cs";
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

} // namespace FlightBox::Render
