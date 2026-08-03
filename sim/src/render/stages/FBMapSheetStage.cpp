#include "FBMapSheetStage.h"

#include "FBLog.h"
#include "FBMips.h"

#include <cmath>

namespace FlightBox::Render {

namespace {
/* [SET] The chart's own paper. It is NOT the terrain's colour: a frame that shows this and nothing
 * else is a frame with no sheet, and it must be readable as such at a glance. */
constexpr float kBackR = 0.10f, kBackG = 0.11f, kBackB = 0.13f;
/* [SET] One update's fetch budget. The native source is a blocking curl with a 3 s per-URL retry
 * ceiling, so an unreachable server costs at most this many stalls per pump instead of the whole
 * visible set. */
constexpr int kFetchPerPump = 4;
/* [SET] Long enough that a page still pending at the browser's tile worker survives a few frames of
 * panning, short enough that the list stays a working set. */
constexpr unsigned kForgetFrames = 120;

const char *kSheetWGSL = R"(
struct SU { scale : vec4f, back : vec4f };
@group(0) @binding(0) var<uniform> su : SU;
@group(0) @binding(1) var sheetSamp : sampler;
@group(0) @binding(2) var sheetTex : texture_2d_array<f32>;
struct SVO { @builtin(position) pos : vec4f, @location(0) suv : vec2f,
             @location(1) @interpolate(flat) slayer : f32 };
@vertex fn vs(@location(0) p : vec2f, @location(1) uv : vec2f, @location(2) l : f32) -> SVO {
  var o : SVO;
  o.pos = vec4f(p.x * su.scale.x - 1.0, 1.0 - p.y * su.scale.y, 0.0, 1.0);
  o.suv = uv;
  o.slayer = l;
  return o;
}
/* The sample is taken UNCONDITIONALLY and then selected away: textureSample needs uniform control
 * flow, and the backdrop quad shares this pipeline with the tiles. */
@fragment fn fs(in : SVO) -> @location(0) vec4f {
  let t = textureSample(sheetTex, sheetSamp, in.suv, max(i32(in.slayer), 0));
  return select(t, vec4f(su.back.rgb, 1.0), in.slayer < 0.0);
}
)";
} // namespace

void FBMapSheetStage::Init(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Pyramid.resize((size_t)fb_pyramid_bytes(kTs));

  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)kTs, (uint32_t)kTs, (uint32_t)kLayers};
  td.mipLevelCount = (uint32_t)fb_mip_count(kTs);
  td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  Sheet = Device.CreateTexture(&td);

  wgpu::SamplerDescriptor sd{};
  sd.addressModeU = wgpu::AddressMode::ClampToEdge;
  sd.addressModeV = wgpu::AddressMode::ClampToEdge;
  sd.magFilter = wgpu::FilterMode::Linear;
  sd.minFilter = wgpu::FilterMode::Linear;
  sd.mipmapFilter = wgpu::MipmapFilterMode::Linear;
  sd.maxAnisotropy = 1;
  Samp = Device.CreateSampler(&sd);

  wgpu::BufferDescriptor bd{};
  bd.size = 32;   /* vec4 scale + vec4 backdrop */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);
  float uni[8] = {2.0f / (float)gpu.Width, 2.0f / (float)gpu.Height, 0.0f, 0.0f,
                  kBackR, kBackG, kBackB, 1.0f};
  Queue.WriteBuffer(Uni, 0, uni, sizeof uni);

  /* (visible tiles + the backdrop) * 6 verts * 5 floats, with the layer budget as the ceiling. */
  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  bd.size = (uint64_t)(kLayers + 1) * 6 * 5 * sizeof(float);
  Vtx = Device.CreateBuffer(&bd);
  Verts.reserve((size_t)(kLayers + 1) * 6 * 5);

  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = kSheetWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attrs[3] = {};
  attrs[0].format = wgpu::VertexFormat::Float32x2; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
  attrs[1].format = wgpu::VertexFormat::Float32x2; attrs[1].offset = 8;  attrs[1].shaderLocation = 1;
  attrs[2].format = wgpu::VertexFormat::Float32;   attrs[2].offset = 16; attrs[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 20;
  vbl.attributeCount = 3;
  vbl.attributes = attrs;

  wgpu::ColorTargetState ct{};
  ct.format = gpu.SurfaceFormat;   /* opaque: the sheet REPLACES the frame it lands on */

  wgpu::VertexState vs{};
  vs.module = sm;
  vs.entryPoint = "vs";
  vs.bufferCount = 1;
  vs.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.entryPoint = "fs";
  fs.targetCount = 1;
  fs.targets = &ct;
  wgpu::RenderPipelineDescriptor pd{};
  pd.vertex = vs;
  pd.fragment = &fs;
  pd.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  Pipe = Device.CreateRenderPipeline(&pd);

  wgpu::TextureViewDescriptor tvd{};
  tvd.dimension = wgpu::TextureViewDimension::e2DArray;
  tvd.mipLevelCount = (uint32_t)fb_mip_count(kTs);
  tvd.arrayLayerCount = (uint32_t)kLayers;
  wgpu::BindGroupEntry be[3] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = 32;
  be[1].binding = 1; be[1].sampler = Samp;
  be[2].binding = 2; be[2].textureView = Sheet.CreateView(&tvd);
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 3;
  bgd.entries = be;
  Bind = Device.CreateBindGroup(&bgd);
}

void FBMapSheetStage::SetView(const FBMapView &v, bool on) {
  View = v;
  On = on && v.Width > 0 && v.Height > 0;
  if (On) Z = v.SheetZoom(kTs);
}

int FBMapSheetStage::Find(int z, int x, int y) const {
  for (size_t i = 0; i < Tiles.size(); i++)
    if (Tiles[i].Z == z && Tiles[i].X == x && Tiles[i].Y == y) return (int)i;
  return -1;
}

int FBMapSheetStage::AllocLayer(void) {
  if (!FreeLayers.empty()) { int l = FreeLayers.back(); FreeLayers.pop_back(); return l; }
  if (LayerUsed < kLayers) return LayerUsed++;
  /* The oldest tile nobody looked at this frame gives up its layer. */
  int victim = -1;
  unsigned oldest = ~0u;
  for (size_t i = 0; i < Tiles.size(); i++)
    if (Tiles[i].Layer >= 0 && Tiles[i].Touch != Frame && Tiles[i].Touch < oldest) {
      oldest = Tiles[i].Touch;
      victim = (int)i;
    }
  if (victim < 0) return -1;
  int layer = Tiles[(size_t)victim].Layer;
  Tiles.erase(Tiles.begin() + victim);
  return layer;
}

void FBMapSheetStage::WriteLayer(int layer, const unsigned char *pyramid) {
  const unsigned char *p = pyramid;
  int w = kTs, level = 0;
  for (;;) {
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = Sheet;
    dst.mipLevel = (uint32_t)level;
    dst.origin = {0, 0, (uint32_t)layer};
    wgpu::TexelCopyBufferLayout lay{};
    lay.bytesPerRow = (uint32_t)w * 4;
    lay.rowsPerImage = (uint32_t)w;
    wgpu::Extent3D ext{(uint32_t)w, (uint32_t)w, 1};
    Queue.WriteTexture(&dst, p, (size_t)w * w * 4, &lay, &ext);
    if (w == 1) break;
    p += (size_t)w * w * 4;
    w >>= 1;
    level++;
  }
}

void FBMapSheetStage::Pump(void) {
  if (!On || !Device) return;
  Frame++;
  Wanted = Resident = Holes = 0;

  const double p = View.PxPerMerc();
  const double n = std::exp2((double)Z);
  const double tilePx = p / n;
  if (!(tilePx > 0.5)) return;

  /* The Mercator window the frame covers, in tile units at this zoom. */
  const double mx0 = FBMercX(View.CentreLonDeg), my0 = FBMercY(View.CentreLatDeg);
  const double left = mx0 - (double)View.CentreX() / p;
  const double top = my0 - (double)View.CentreY() / p;
  const double right = left + (double)View.Width / p;
  const double bottom = top + (double)View.Height / p;
  const long tx0 = (long)std::floor(left * n), tx1 = (long)std::floor(right * n);
  long ty0 = (long)std::floor(top * n), ty1 = (long)std::floor(bottom * n);
  const long span = (long)n;
  if (ty0 < 0) ty0 = 0;
  if (ty1 > span - 1) ty1 = span - 1;

  /* A pan leaves pending tiles and holes behind that never hold a layer and would otherwise grow
   * without bound across a session. Only entries nobody has looked at for a while go. */
  for (size_t i = Tiles.size(); i-- > 0;)
    if (Tiles[i].Layer < 0 && Frame - Tiles[i].Touch > kForgetFrames) Tiles.erase(Tiles.begin() + (long)i);

  int fetched = 0;
  for (long ty = ty0; ty <= ty1; ty++)
    for (long tx = tx0; tx <= tx1; tx++) {
      long wx = ((tx % span) + span) % span;   /* the antimeridian is a wrap, not an edge */
      Wanted++;
      int idx = Find(Z, (int)wx, (int)ty);
      if (idx < 0) {
        Tiles.push_back({Z, (int)wx, (int)ty, -1, 0, Frame});
        idx = (int)Tiles.size() - 1;
      }
      FBSheetTile &t = Tiles[(size_t)idx];
      t.Touch = Frame;
      if (t.Ready == 1) { Resident++; continue; }
      if (t.Ready == -1) { Holes++; continue; }
      if (!Fetch || fetched >= kFetchPerPump) continue;
      fetched++;
      int r = Fetch(t.Z, t.X, t.Y, kTs, Pyramid.data());
      if (r < 0) { t.Ready = -1; Holes++; continue; }
      if (r == 0) continue;   /* pending: the browser's worker has not answered yet */
      int layer = AllocLayer();
      if (layer < 0) continue;   /* every layer is in this frame's own working set; next pump */
      /* AllocLayer may have erased a tile out from under `idx`. */
      idx = Find(Z, (int)wx, (int)ty);
      if (idx < 0) { FreeLayers.push_back(layer); continue; }
      Tiles[(size_t)idx].Layer = layer;
      Tiles[(size_t)idx].Ready = 1;
      WriteLayer(layer, Pyramid.data());
      Resident++;
    }
}

const char *FBMapSheetNote(FBMapSheetState s) {
  switch (s) {
    case FBMapSheetState::Waiting:     return "NO OSM SHEET - WAITING ON FB-TILES";
    case FBMapSheetState::Unreachable: return "NO OSM SHEET - FB-TILES UNREACHABLE";
    case FBMapSheetState::Partial:     return "OSM SHEET INCOMPLETE";
    default:                           return "";
  }
}

FBMapSheetState FBMapSheetStage::State() const {
  if (!On) return FBMapSheetState::Off;
  if (Wanted <= 0) return FBMapSheetState::Waiting;
  if (Resident >= Wanted) return FBMapSheetState::Complete;
  if (Resident > 0) return FBMapSheetState::Partial;
  return Holes >= Wanted ? FBMapSheetState::Unreachable : FBMapSheetState::Waiting;
}

void FBMapSheetStage::BuildQuads(void) {
  Verts.clear();
  auto quad = [&](float x0, float y0, float x1, float y1, float layer) {
    const float c[6][4] = {{x0, y0, 0.0f, 0.0f}, {x1, y0, 1.0f, 0.0f}, {x0, y1, 0.0f, 1.0f},
                           {x1, y0, 1.0f, 0.0f}, {x1, y1, 1.0f, 1.0f}, {x0, y1, 0.0f, 1.0f}};
    for (int i = 0; i < 6; i++) {
      Verts.push_back(c[i][0]); Verts.push_back(c[i][1]);
      Verts.push_back(c[i][2]); Verts.push_back(c[i][3]);
      Verts.push_back(layer);
    }
  };
  quad(0.0f, 0.0f, (float)View.Width, (float)View.Height, -1.0f);

  const double p = View.PxPerMerc();
  const double n = std::exp2((double)Z);
  const double mx0 = FBMercX(View.CentreLonDeg), my0 = FBMercY(View.CentreLatDeg);
  for (const FBSheetTile &t : Tiles) {
    if (t.Ready != 1 || t.Layer < 0 || t.Z != Z || t.Touch != Frame) continue;
    double tmx = (double)t.X / n, tmy = (double)t.Y / n;
    double dx = tmx - mx0;
    if (dx > 0.5) dx -= 1.0;
    if (dx < -0.5) dx += 1.0;
    float x0 = (float)((double)View.CentreX() + dx * p);
    float y0 = (float)((double)View.CentreY() + (tmy - my0) * p);
    float side = (float)(p / n);
    quad(x0, y0, x0 + side, y0 + side, (float)t.Layer);
  }
}

void FBMapSheetStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  (void)ctx;
  if (!On || !Pipe) return;
  BuildQuads();
  const size_t cap = (size_t)(kLayers + 1) * 6 * 5;
  if (Verts.size() > cap) Verts.resize(cap - cap % 30);   /* whole quads only: a cut one is 2 stray tris */
  if (Verts.empty()) return;
  Queue.WriteBuffer(Vtx, 0, Verts.data(), Verts.size() * sizeof(float));
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx, 0, Verts.size() * sizeof(float));
  pass.Draw((uint32_t)(Verts.size() / 5));
  if (Logged++ % 120 == 0)
    FBLog::Debug("map", "sheet", {{"zoom", Z}, {"wanted", Wanted}, {"resident", Resident},
                                  {"holes", Holes}, {"quads", (int)(Verts.size() / 30)}});
}

} // namespace FlightBox::Render
