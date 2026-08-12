#include "SubjectDraw.h"

#include <string>

#include "SceneTargets.h"
#include "SurfaceState.h"

namespace outshine::Render {

namespace {

/* THE SAME DERIVATION TABLE EVERY OTHER DRAW USES. A glTF subject is opaque, and its winding is
 * TRUSTED because the format defines one: the front face is counter-clockwise and
 * `Gltf::Subject::Indices` has already restated a mirroring node's order, so a face that turns away
 * is a back face and not an accident of how the file was authored. This is the opposite case from an
 * OSM ring, which arrives wound either way and is `Winding::Unknown` for it. */
constexpr SurfaceState kSubjectState = StateOf(Material{});
static_assert(kSubjectState.CullsBack(), "an opaque subject culls its back faces");

/* WHICH SCREEN-SPACE ORIENTATION glTF's FRONT FACE ARRIVES IN. MEASURED, not derived: the chain is
 * two flips and not one -- clip-space +Y is up while the framebuffer's runs down, and the eye basis
 * puts the view along -Z so the projection's own w is -z_eye -- and they cancel, leaving glTF's
 * counter-clockwise front face counter-clockwise on the target. The derivation that counted one flip
 * was written here first and culled every pixel of `render/coverage/quad`. */
constexpr wgpu::FrontFace kGltfFrontFace = wgpu::FrontFace::CCW;

} // namespace

static const char *kSubjectWGSL = R"(
struct S { mvp : mat4x4f, anc : vec4f, emitted : vec4f };
@group(0) @binding(0) var<uniform> s : S;

struct SOut { @builtin(position) pos : vec4f };

@vertex fn vs(@location(0) p : vec3f) -> SOut {
  var o : SOut;
  o.pos = s.mvp * vec4f(p + s.anc.xyz, 1.0);
  return o;
}

struct SFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
/* rho*L, DECLARED and not shaded. alpha is the direct fraction a display transfer weights its
 * occlusion by, and for a facet under a uniform environment every bit of it is direct. */
@fragment fn fs(in : SOut) -> SFrag {
  var o : SFrag;
  o.col = vec4f(s.emitted.rgb, 1.0);
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
  rp.primitive.frontFace = kGltfFrontFace;
  rp.primitive.cullMode =
      CullsBackFaces(kSubjectState, Winding::Trusted) ? wgpu::CullMode::Back : wgpu::CullMode::None;
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
  for (int i = 0; i < 3; i++) u[20 + i] = Surface.AlbedoLinear[i] * Surface.EnvironmentRadiance;
  Queue.WriteBuffer(Uni, 0, u, sizeof u);
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);
  pass.DrawIndexed(NIdx);
}

} // namespace outshine::Render
