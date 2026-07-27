#include "FBTileLightsStage.h"

namespace FlightBox {

/* The vs subtracts (eye - anchor), so the field is camera-relative without a per-frame re-upload.
 * Depth-tested but depth-WRITE off: terrain occludes far lights, a light never occludes another. */
static const char *kLightWGSL = R"(
struct LU { mvp : mat4x4f, p : vec4f, eye : vec4f };   /* p = (dayFade, vpW, vpH, focal); eye.xyz = eye-anchor (m) */
@group(0) @binding(0) var<uniform> lu : LU;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f, @location(1) col : vec3f };
@vertex fn vs(@builtin(vertex_index) vi : u32, @location(0) ipos : vec3f,
              @location(1) irad : f32, @location(2) icol : vec3f) -> VOut {
  var q = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(-1.0, 1.0),
                          vec2f(-1.0, 1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0));
  let corner = q[vi];
  let rel = ipos - lu.eye.xyz;                 /* camera-relative ECEF (m) */
  var clip = lu.mvp * vec4f(rel, 1.0);
  let dist = max(length(rel), 1.0);
  let px = clamp(irad * (0.5 * lu.p.z * lu.p.w) / dist, 1.3, 4.0);   /* world radius -> screen px; ≥1.3 px
     floor defeats the sub-pixel collapse that makes a point light vanish at range (the star lesson) */
  clip.x = clip.x + corner.x * (2.0 * px / lu.p.y) * clip.w;
  clip.y = clip.y + corner.y * (2.0 * px / lu.p.z) * clip.w;
  var o : VOut;
  o.pos = clip; o.uv = corner; o.col = icol;
  return o;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let r = length(in.uv);
  let core = smoothstep(1.0, 0.0, r);          /* soft round point */
  let a = core * (1.0 - lu.p.x);               /* fade out toward day */
  return vec4f(in.col * a, a);
}
)";

void FBTileLightsStage::Init(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = kLightWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attr[3] = {};
  attr[0].format = wgpu::VertexFormat::Float32x3; attr[0].offset = 0;  attr[0].shaderLocation = 0;
  attr[1].format = wgpu::VertexFormat::Float32;   attr[1].offset = 12; attr[1].shaderLocation = 1;
  attr[2].format = wgpu::VertexFormat::Float32x3; attr[2].offset = 16; attr[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 7 * sizeof(float);
  vbl.stepMode = wgpu::VertexStepMode::Instance;
  vbl.attributeCount = 3;
  vbl.attributes = attr;

  wgpu::BlendState blend{};                        /* additive: lights accumulate */
  blend.color.srcFactor = wgpu::BlendFactor::One;  blend.color.dstFactor = wgpu::BlendFactor::One;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;  blend.alpha.dstFactor = wgpu::BlendFactor::One;
  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  ct.blend = &blend;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = false;
  ds.depthCompare = wgpu::CompareFunction::GreaterEqual;   /* reversed-Z: terrain in FRONT occludes; a light
     co-planar with its own ground tile (equal depth) still shows — Greater alone drops it on the tie */

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = m;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = 24 * sizeof(float);   /* mat4 + p + eye */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be{};
  be.binding = 0; be.buffer = Uni; be.size = 24 * sizeof(float);
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 1;
  bg.entries = &be;
  Bind = Device.CreateBindGroup(&bg);
}

void FBTileLightsStage::SetLights(const float *inst, int count) {
  NLights = count;
  if (count <= 0) return;
  size_t bytes = (size_t)count * 7 * sizeof(float);
  if (InstCap < count) {
    wgpu::BufferDescriptor bd{};
    bd.size = bytes;
    bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Inst = Device.CreateBuffer(&bd);
    InstCap = count;
  }
  Queue.WriteBuffer(Inst, 0, inst, bytes);
}

void FBTileLightsStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!(ctx.GroundPhoto && ctx.DayFade < 0.6f && NLights > 0)) return;
  float lu[24];
  for (int i = 0; i < 16; i++) lu[i] = ctx.Mvp20[i];   /* same camera-relative MVP as the terrain */
  lu[16] = ctx.DayFade; lu[17] = (float)ctx.Width; lu[18] = (float)ctx.Height; lu[19] = 1.7320508f;   /* focal = 1/tan(30°) */
  lu[20] = (float)(ctx.Eye[0] - Anchor[0]); lu[21] = (float)(ctx.Eye[1] - Anchor[1]);
  lu[22] = (float)(ctx.Eye[2] - Anchor[2]); lu[23] = 0.0f;
  Queue.WriteBuffer(Uni, 0, lu, sizeof lu);
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Inst);
  pass.Draw(6, (uint32_t)NLights);
}

} // namespace FlightBox
