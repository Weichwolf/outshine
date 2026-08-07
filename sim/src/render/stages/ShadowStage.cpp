#include "ShadowStage.h"
#include "Frustum.h"
#include "GeometryIsolation.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace outshine::Render {

static const char *kShadowWGSL = R"(
struct S { mvp : mat4x4f, anc : vec4f };
@group(0) @binding(0) var<uniform> s : S;
@vertex fn vs(@location(0) p : vec3f) -> @builtin(position) vec4f {
  return s.mvp * vec4f(p + s.anc.xyz, 1.0);
}
)";

static void Cross3(const double a[3], const double b[3], double o[3]) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}
static void Norm3(double v[3]) {
  const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l > 0.0) { v[0] /= l; v[1] /= l; v[2] /= l; }
}
static double Dot3(const double a[3], const double b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void ShadowStage::Init(const Gpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)(kShadowTexels * kShadowCascades), (uint32_t)kShadowTexels, 1};
  td.format = wgpu::TextureFormat::Depth32Float;
  td.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  Atlas = Device.CreateTexture(&td);

  wgpu::SamplerDescriptor sd{};
  sd.compare = wgpu::CompareFunction::Less;   /* plain [0,1] depth: nearer to the light = smaller */
  sd.magFilter = wgpu::FilterMode::Linear;    /* hardware PCF inside each of the nine taps */
  sd.minFilter = wgpu::FilterMode::Linear;
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  Cmp = Device.CreateSampler(&sd);

  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = kShadowWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  /* The caster mesh is BuildingsStage's, so the stride is its stride; only position is read. */
  wgpu::VertexAttribute attr{};
  attr.format = wgpu::VertexFormat::Float32x3;
  attr.offset = 0;
  attr.shaderLocation = 0;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 8 * sizeof(float);
  vbl.attributeCount = 1;
  vbl.attributes = &attr;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Less;
  /* A prism drawn with both faces is its own worst caster: slope-scaled depth bias is what keeps the
   * lit wall from shadowing itself where the light grazes it. */
  ds.depthBias = 2;
  ds.depthBiasSlopeScale = 2.5f;

  wgpu::BindGroupLayoutEntry ble{};
  ble.binding = 0;
  ble.visibility = wgpu::ShaderStage::Vertex;
  ble.buffer.type = wgpu::BufferBindingType::Uniform;
  /* The whole point: a cascade block and a per-TILE block are the same shape, so which one a draw
   * reads is an offset and not a second bind group. Without this the terrain half would need one
   * bind group per tile per cascade, built every frame. */
  ble.buffer.hasDynamicOffset = true;
  ble.buffer.minBindingSize = 20 * sizeof(float);
  wgpu::BindGroupLayoutDescriptor bld{};
  bld.entryCount = 1;
  bld.entries = &ble;
  Bgl = Device.CreateBindGroupLayout(&bld);
  wgpu::PipelineLayoutDescriptor pld{};
  pld.bindGroupLayoutCount = 1;
  pld.bindGroupLayouts = &Bgl;

  wgpu::RenderPipelineDescriptor rp{};
  rp.layout = Device.CreatePipelineLayout(&pld);
  rp.vertex.module = m;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  rp.fragment = nullptr;   /* depth only */
  rp.depthStencil = &ds;
  rp.primitive.cullMode = wgpu::CullMode::None;   /* OSM ring winding is not reliable, as in the scene */
  Pipe = Device.CreateRenderPipeline(&rp);

  /* Blocks 0..3 are the cascades (buildings); after them, one block per cascade per tile. */
  wgpu::BufferDescriptor bd{};
  bd.size = (uint64_t)kCascadeStride * kShadowCascades * (1 + kMaxShadowTiles);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be{};
  be.binding = 0;
  be.buffer = Uni;
  be.offset = 0;
  be.size = 20 * sizeof(float);
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Bgl;
  bg.entryCount = 1;
  bg.entries = &be;
  Bind = Device.CreateBindGroup(&bg);
}

void ShadowStage::SetCasters(wgpu::Buffer vtx, uint32_t nverts, const DagCluster *clusters,
                             int nclusters, const double anchor[3]) {
  Vtx = vtx;
  NVerts = nverts;
  Clusters = clusters;
  NClusters = nclusters;
  for (int i = 0; i < 3; i++) Anchor[i] = anchor[i];
}

/* ONE CASCADE'S OWN CUT. A shadow map is a VIEW, so it takes the same ladder the camera does — but
 * its projection is orthographic, so there is no distance in the metric at all: a cluster's error in
 * SHADOW TEXELS is err / texelM, one number for the whole cascade. The box is the cull. */
void ShadowStage::Cut(int cascade, const double rel0[3], const double C[3], const double X[3],
                      const double Y[3], const double L[3], double R, double zn, double zf) {
  std::vector<DrawRange> &out = Ranges[cascade];
  out.clear();
  if (!Clusters || NClusters <= 0) return;
  const double texelM = 2.0 * R / (double)kShadowTexels;
  const float err = (float)(kShadowTauTexels * texelM);
  for (int i = 0; i < NClusters; i++) {
    const DagCluster &c = Clusters[i];
    if (!(c.SelfErr <= err && c.ParentErr > err)) continue;
    {
      const double d[3] = {rel0[0] + c.SelfCenter[0] - C[0], rel0[1] + c.SelfCenter[1] - C[1],
                           rel0[2] + c.SelfCenter[2] - C[2]};
      const double r = (double)c.SelfRadius;
      if (std::fabs(Dot3(d, X)) > R + r || std::fabs(Dot3(d, Y)) > R + r) continue;
      const double z = Dot3(d, L);
      if (z < zn - r || z > zf + r) continue;
    }
    if (!out.empty() && out.back().First + out.back().Count == c.First) out.back().Count += c.Count;
    else out.push_back(DrawRange{c.First, c.Count});
  }
}

void ShadowStage::Update(const FrameContext &ctx) {
  std::memset(Csm, 0, sizeof Csm);
  if (NVerts == 0 || !Vtx) return;

  /* Light-space basis: L is where the light TRAVELS, X/Y span the map. Pairing L with the world up
   * fails when the sun is near the zenith, so the fallback axis is the camera forward. */
  double L[3] = {-ctx.SunDir[0], -ctx.SunDir[1], -ctx.SunDir[2]};
  Norm3(L);
  double ref[3] = {ctx.Up[0], ctx.Up[1], ctx.Up[2]};
  if (std::fabs(Dot3(L, ref)) > 0.98) { ref[0] = ctx.Fwd[0]; ref[1] = ctx.Fwd[1]; ref[2] = ctx.Fwd[2]; }
  double X[3], Y[3];
  Cross3(ref, L, X); Norm3(X);
  Cross3(L, X, Y); Norm3(Y);

  /* Horizontal forward: the box is pushed ahead of the camera so the near cascade is not half spent
   * on what is behind the eye, but the shadow of something just behind the eye still lands in it. */
  double fh[3] = {ctx.Fwd[0] - ctx.Up[0] * Dot3(ctx.Fwd, ctx.Up),
                  ctx.Fwd[1] - ctx.Up[1] * Dot3(ctx.Fwd, ctx.Up),
                  ctx.Fwd[2] - ctx.Up[2] * Dot3(ctx.Fwd, ctx.Up)};
  Norm3(fh);

  const double rel0[3] = {Anchor[0] - ctx.Eye[0], Anchor[1] - ctx.Eye[1], Anchor[2] - ctx.Eye[2]};
  const double ratio = std::pow((double)kShadowFarM / (double)kShadowNearM,
                                1.0 / (double)(kShadowCascades - 1));
  for (int c = 0; c < kShadowCascades; c++) {
    const double R = (double)kShadowNearM * std::pow(ratio, (double)c);
    double C[3] = {fh[0] * R * 0.5, fh[1] * R * 0.5, fh[2] * R * 0.5};
    /* Snap the centre to whole shadow texels in light space, or the map crawls under a moving eye. */
    const double texel = 2.0 * R / (double)kShadowTexels;
    const double cx = std::floor(Dot3(C, X) / texel) * texel;
    const double cy = std::floor(Dot3(C, Y) / texel) * texel;
    const double cz = Dot3(C, L);
    for (int a = 0; a < 3; a++) C[a] = X[a] * cx + Y[a] * cy + L[a] * cz;

    /* Depth span: everything from 400 m up-sun of the box (a caster outside it still shadows into
     * it) to the far side of the box itself. */
    const double zn = -(R + 400.0), zf = R + 100.0, dz = zf - zn;
    const double ox = Dot3(C, X), oy = Dot3(C, Y), oz = Dot3(C, L);
    Cut(c, rel0, C, X, Y, L, R, zn, zf);
    float *m = Csm + c * 16;
    for (int col = 0; col < 3; col++) {
      m[col * 4 + 0] = (float)(X[col] / R);
      m[col * 4 + 1] = (float)(Y[col] / R);
      m[col * 4 + 2] = (float)(L[col] / dz);
      m[col * 4 + 3] = 0.0f;
    }
    m[12] = (float)(-ox / R);
    m[13] = (float)(-oy / R);
    m[14] = (float)((-oz - zn) / dz);
    m[15] = 1.0f;
  }

  float *far = Csm + kShadowCascades * 16;
  for (int c = 0; c < kShadowCascades; c++)
    far[c] = (float)((double)kShadowNearM * std::pow(ratio, (double)c));
  float *par = far + 4;
  par[0] = 1.0f / (float)kShadowCascades;
  par[1] = 1.5e-3f;   /* [SET] ortho depth bias; the normal offset in ShadowSample.h does the rest */
  par[2] = 2.0f * (float)kShadowNearM / (float)kShadowTexels;   /* cascade-0 metres per texel */
  /* Disarming the RECEIVERS leaves the pass and its four draws running, so the per-frame pass count
   * is the same number with and without — which is what makes FB_GEOM a paired measurement on two
   * otherwise identical frames instead of two different renderers. */
  par[3] = GeometryIsolation() ? 0.0f : 1.0f;

  /* One aligned block per cascade for the buildings, then one per cascade PER TILE for the terrain.
   * Same shape, so the draw loop picks a block with a dynamic offset and never rebinds. */
  const size_t fpb = kCascadeStride / sizeof(float);
  std::vector<float> blocks(fpb * kShadowCascades, 0.0f);
  for (int c = 0; c < kShadowCascades; c++) {
    float *b = blocks.data() + c * fpb;
    std::memcpy(b, Csm + c * 16, 16 * sizeof(float));
    b[16] = (float)(Anchor[0] - ctx.Eye[0]);
    b[17] = (float)(Anchor[1] - ctx.Eye[1]);
    b[18] = (float)(Anchor[2] - ctx.Eye[2]);
    b[19] = 0.0f;
  }

  /* THE TERRAIN CUT. A cascade is a box of half-width R around its own centre, so a tile enters it
   * when its bounding sphere reaches that box — the cheapest cull there is, and it is the same test
   * for every cascade. No cluster ladder here: a tile's mesh is already decimated to 33x33 supports
   * and the shadow's own tau would not remove a second level. */
  TerrainOverflow = 0;
  for (int c = 0; c < kShadowCascades; c++) TerrainCut[c].clear();
  if (!Terrain.empty()) {
    const double ratio2 = std::pow((double)kShadowFarM / (double)kShadowNearM,
                                   1.0 / (double)(kShadowCascades - 1));
    for (int c = 0; c < kShadowCascades; c++) {
      const double R = (double)kShadowNearM * std::pow(ratio2, (double)c);
      const double C[3] = {fh[0] * R * 0.5, fh[1] * R * 0.5, fh[2] * R * 0.5};
      for (uint32_t i = 0; i < (uint32_t)Terrain.size(); i++) {
        const TerrainCaster &t = Terrain[i];
        /* Tile centre relative to the camera; BoundCtr is relative to the tile's own origin. */
        const double rel[3] = {t.Origin[0] - ctx.Eye[0] + t.BoundCtr[0],
                               t.Origin[1] - ctx.Eye[1] + t.BoundCtr[1],
                               t.Origin[2] - ctx.Eye[2] + t.BoundCtr[2]};
        const double d[3] = {rel[0] - C[0], rel[1] - C[1], rel[2] - C[2]};
        /* Lateral against the box, and up-sun without limit: a ridge 400 m toward the sun still
         * shadows into this cascade, which is exactly what zn reserves depth for. */
        if (std::fabs(Dot3(d, X)) > R + t.BoundRad) continue;
        if (std::fabs(Dot3(d, Y)) > R + t.BoundRad) continue;
        if (Dot3(d, L) > R + 100.0 + t.BoundRad) continue;
        if (TerrainCut[c].size() >= kMaxShadowTiles) { TerrainOverflow++; continue; }
        TerrainCut[c].push_back(i);
      }
    }
    blocks.resize(fpb * kShadowCascades * (1 + kMaxShadowTiles), 0.0f);
    for (int c = 0; c < kShadowCascades; c++) {
      for (size_t k = 0; k < TerrainCut[c].size(); k++) {
        const TerrainCaster &t = Terrain[TerrainCut[c][k]];
        float *b = blocks.data() + (kShadowCascades + (size_t)c * kMaxShadowTiles + k) * fpb;
        std::memcpy(b, Csm + c * 16, 16 * sizeof(float));
        b[16] = (float)(t.Origin[0] - ctx.Eye[0]);
        b[17] = (float)(t.Origin[1] - ctx.Eye[1]);
        b[18] = (float)(t.Origin[2] - ctx.Eye[2]);
        b[19] = 0.0f;
      }
    }
  }
  Queue.WriteBuffer(Uni, 0, blocks.data(), blocks.size() * sizeof(float));
}

void ShadowStage::Encode(const FrameContext &, wgpu::RenderPassEncoder &pass) {
  DrawnVerts = 0;
  DrawCalls = 0;
  if ((NVerts == 0 || !Vtx) && Terrain.empty()) return;
  pass.SetPipeline(Pipe);
  for (int c = 0; c < kShadowCascades; c++) {
    if (Ranges[c].empty() && TerrainCut[c].empty()) continue;
    pass.SetViewport((float)(c * kShadowTexels), 0.0f, (float)kShadowTexels, (float)kShadowTexels,
                     0.0f, 1.0f);
    if (!Ranges[c].empty() && Vtx) {
      const uint32_t off = (uint32_t)c * kCascadeStride;
      pass.SetBindGroup(0, Bind, 1, &off);
      pass.SetVertexBuffer(0, Vtx);
      for (const DrawRange &r : Ranges[c]) {
        pass.Draw(r.Count, 1, r.First);
        DrawnVerts += (long)r.Count;
        DrawCalls++;
      }
    }
    /* One tile, one buffer, one offset — the vertex buffer changes per draw here, which the building
     * half never needs. That is the whole extra cost of terrain casting. */
    for (size_t k = 0; k < TerrainCut[c].size(); k++) {
      const TerrainCaster &t = Terrain[TerrainCut[c][k]];
      const uint32_t off = (uint32_t)((kShadowCascades + (size_t)c * kMaxShadowTiles + k) * kCascadeStride);
      pass.SetBindGroup(0, Bind, 1, &off);
      pass.SetVertexBuffer(0, t.Vtx);
      pass.Draw(t.NVerts);
      DrawnVerts += (long)t.NVerts;
      DrawCalls++;
    }
  }
}

} // namespace outshine::Render
