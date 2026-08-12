#include "SubjectDraw.h"

#include <string>

#include "SceneTargets.h"

namespace outshine::Render {

static const char *kSubjectWGSL = R"(
struct S { mvp : mat4x4f, anc : vec4f };
@group(0) @binding(0) var<uniform> s : S;

struct SOut { @builtin(position) pos : vec4f };

@vertex fn vs(@location(0) p : vec3f) -> SOut {
  var o : SOut;
  o.pos = s.mvp * vec4f(p + s.anc.xyz, 1.0);
  return o;
}

struct SFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
/* A FLAT VALUE AND NOT A SHADE. The coverage and depth rungs read the depth attachment; this colour
 * exists so the case directory holds a picture a person can look at, and a lit one would invite the
 * eye to judge a lighting model this unit does not have. */
@fragment fn fs(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(0.8, 0.8, 0.8, 1.0);
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

void SubjectDraw::Configure(const Gpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  const std::string src = std::string(kVelocityWGSL) + kSubjectWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attr{};
  attr.format = wgpu::VertexFormat::Float32x3;
  attr.offset = 0;
  attr.shaderLocation = 0;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 3 * sizeof(float);
  vbl.attributeCount = 1;
  vbl.attributes = &attr;

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthCompare = wgpu::CompareFunction::Greater;
  ds.depthWriteEnabled = true;

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = m;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(true)};
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  rp.primitive.cullMode = wgpu::CullMode::None;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be{};
  be.binding = 0;
  be.buffer = Uni;
  be.size = kUniFloats * sizeof(float);
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 1;
  bg.entries = &be;
  Bind = Device.CreateBindGroup(&bg);
}

void SubjectDraw::SetMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                          const double anchor[3]) {
  NVerts = nverts;
  NIdx = nidx;
  for (int axis = 0; axis < 3; ++axis) { Anchor[axis] = anchor[axis]; }
  if (nverts == 0 || nidx == 0 || !Device) return;

  wgpu::BufferDescriptor vd{};
  vd.size = (uint64_t)nverts * 3 * sizeof(float);
  vd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  Vtx = Device.CreateBuffer(&vd);
  Queue.WriteBuffer(Vtx, 0, verts, (size_t)nverts * 3 * sizeof(float));

  /* WriteBuffer copies in 4-byte units, so an odd index count is padded to keep the queue's own
   * alignment rule -- the draw still submits exactly `nidx`. */
  const uint64_t indexBytes = ((uint64_t)nidx * sizeof(uint32_t) + 3u) & ~uint64_t{3};
  wgpu::BufferDescriptor id{};
  id.size = indexBytes;
  id.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
  Idx = Device.CreateBuffer(&id);
  Queue.WriteBuffer(Idx, 0, idx, (size_t)nidx * sizeof(uint32_t));
}

void SubjectDraw::Encode(const FrameContext &ctx, ClusterCut &, wgpu::RenderPassEncoder &pass) {
  if (NIdx == 0 || !Vtx || !Idx) return;
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  for (int i = 0; i < 3; i++) u[16 + i] = (float)(Anchor[i] - ctx.Eye[i]);
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(NIdx);
}

} // namespace outshine::Render
