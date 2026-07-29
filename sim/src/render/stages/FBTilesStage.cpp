#include "FBTilesStage.h"
#include "FBMips.h"
#include "FBAtmoCommon.h"
#include "FBAtmoSample.h"
#include "FBAtmoHaze.h"
#include "FBLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace FlightBox::Render {

/* The terrain draw. Per-draw data, albedo array, grazing mip bias, RenderBundle signature and the
 * invariant counters: doc/render/renderer.md, Abschnitt 6. */
static const char *kTerrainWGSL = R"(
/* haze:  x = sigma0 (1/m, Koschmieder of the reported visibility), y = camera altitude ASL (m),
 *        z = the WGS84 ground radius under the camera (Mm), w = spare.
 * dk0..2: one weather deck each as (baseM, topM, sun optical depth, cover) — FBAtmoHaze.h. */
struct U { mvp : mat4x4f, sun : vec4f, haze : vec4f, dk0 : vec4f, dk1 : vec4f, dk2 : vec4f };
@group(0) @binding(0) var<uniform> u : U;
// per-draw storage entry: a.xyz = camera-relative ECEF offset (origin_ecef - cam_ecef, float),
// a.w = albedo array LAYER; b.x = per-tile PHOTO brightness GAIN (1.0 for OSM / bright tiles). The draw
// selects entry i via firstInstance, so instance_index == draw index.
struct Tile { a : vec4f, b : vec4f };
@group(0) @binding(1) var<storage, read> tiles : array<Tile>;
@group(0) @binding(2) var samp : sampler;
@group(0) @binding(3) var albedo : texture_2d_array<f32>;
/* The LUT sampler, NOT the albedo one: the sky-view LUT wraps in azimuth (AddressMode::Repeat) and its
 * seam sits at u = 0, which is the SUN's own azimuth — sampling it clamped puts a filtered seam right
 * across the brightest part of the far field. */
@group(0) @binding(4) var lsamp : sampler;
@group(0) @binding(5) var svLUT : texture_2d<f32>;
@group(0) @binding(6) var<uniform> A : Atmo;
/* The lit-albedo weights: 0.4 ambient + 3.0 x N.L direct, unchanged from the clear-air terrain. Only
 * kOvercastAmb is new — see the deck block in fs(). */
const kTerrainAmb : f32 = 0.4;
const kTerrainDir : f32 = 3.0;
/* Extra DIFFUSE the ground gains for every unit of direct beam a deck takes away. Derived from the
 * illuminance ratio, not tuned: clear sun at 45 deg gives 0.4 + 3.0*cos45 = 2.52 here, of which 0.4 is
 * the diffuse share; measured overcast diffuse illuminance runs about 1.0-1.5x the clear-sky diffuse,
 * so the diffuse term gains ~35 % of 0.4 = 0.14 under a closed deck. The resulting ground under full
 * overcast is 0.55/2.52 = 22 % of the sunlit clear case, inside the 10-25 % that overcast/clear global
 * illuminance measures. */
const kOvercastAmb : f32 = 0.15;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f, @location(1) nrm : vec3f,
              @location(2) @interpolate(flat) layer : u32, @location(3) wpos : vec3f,
              @location(4) @interpolate(flat) gain : f32 };
@vertex fn vs(@builtin(instance_index) inst : u32,
              @location(0) p : vec3f, @location(1) uv : vec2f, @location(2) n : vec3f) -> VOut {
  var o : VOut;
  let t = tiles[inst];
  let rel = p + t.a.xyz;                    // camera-relative ECEF (metres); a.xyz = origin - cam
  o.pos = u.mvp * vec4f(rel, 1.0);
  o.uv = uv;
  o.nrm = n;
  o.layer = u32(t.a.w);
  o.wpos = rel;
  o.gain = t.b.x;
  return o;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let nrmN = normalize(in.nrm);
  // At grazing the uv FOOTPRINT anisotropy exceeds the 16:1 HW aniso cap; the sampler under-filters the
  // major axis -> VERTICAL STREAKS. Clamp the effective anisotropy to 16:1 with a mip bias derived from
  // the ACTUAL screen-space uv derivatives (dpdx/dpdy) — not the surface normal, which mis-reads on
  // camera-facing slopes. bias = log2(aniso/16), so only the >16:1 tail coarsens; near/overhead (aniso
  // ~1) gets bias 0 and stays fully sharp. Cap 4 so the far band can't blur to mush.
  // Grazing view -> the albedo footprint elongates along the depth axis past the 16:1 HW aniso cap ->
  // the major axis under-filters into VERTICAL STREAKS. Coarsen the mip by the view GRAZING (the view
  // ray vs the radial up — a robust screen-projection signal; the per-tile uv derivatives underestimate
  // it because far tiles carry a coarse mesh, and the surface normal mis-reads on camera-facing slopes).
  // grazeV = downward component of the view ray; bias climbs as it shallows, 0 above ~30° depression so
  // near/steep terrain stays fully sharp, ~2.8 at the horizon band. Cap 3.5 so the far band can't mush.
  let vdir = normalize(in.wpos);
  let upR = normalize(A.camPosMm.xyz);
  let grazeV = max(-dot(vdir, upR), 0.01);
  // SHARPNESS PRIORITY (user finding 2026-07-23 overrides the critic's streak finding): the old
  // cap 4 (16x blur) bought a streak-free far band with MUSH. Retuned — onset only below ~10°
  // depression (2^-2.5) so near+mid+most of the far field stay FULLY sharp, cap 1.2 (~2.3x footprint)
  // so the extreme-grazing horizon band gets a light coarsen, not a smear. Residual streaks in that
  // last band are ACCEPTED (sharp > streak-free, by explicit user preference).
  let gbias = clamp(1.0 * (-log2(grazeV) - 2.5), 0.0, 1.2);
  // rgba8unorm-srgb layer -> sampling decodes to LINEAR (no manual pow); uv is 0..1 across the tile.
  // in.gain lifts the dark low-zoom Esri photo composite toward the bright orthophoto level (per-tile,
  // linear; 1.0 for OSM/bright tiles) — SVS unaffected (its layers all carry gain 1.0).
  let base = textureSampleBias(albedo, samp, in.uv, i32(in.layer), gbias).rgb * in.gain;
  // Direct-sun diffuse, GATED by the daylight factor in EVS: with the sun below the horizon (night)
  // there is no direct sunlight, so diff -> 0. Without the gate, steep/aliased normals (more numerous on
  // coarse LOD tiles) catch the below-horizon sun via (0.4+3*diff) -> a bright brightness-step at LOD
  // seams. SVS is a constant daylit database view -> full diff. (Day EVS: skyExtra.x~1 -> unchanged.)
  let diffGate = select(1.0, A.skyExtra.x, A.skyExtra.y > 0.5);
  // The fragment's OWN altitude: which decks stand between it and the sun is a property of where it
  // is, and in the Alps a ridge really does poke through a 2 991 m base while the valley does not.
  let fragAltM = (length(A.camPosMm.xyz + in.wpos * 1.0e-6) - u.haze.z) * 1.0e6;
  // WEATHER LIGHT. A deck above this fragment attenuates the DIRECT beam (Beer over the slant path,
  // weighted by the fraction of sun rays that hit cloud at all) and converts what it removes into
  // DIFFUSE. Statistical per deck and per frame — deliberately NOT a shadow map: the quantity here is
  // "how much of the sun reaches the ground under this sky", which is an average over the whole deck,
  // and a per-pixel answer would need the march the cloud pass already pays for.
  let sunThru = deckSunThru(u.dk0, fragAltM) * deckSunThru(u.dk1, fragAltM) * deckSunThru(u.dk2, fragAltM);
  let diff = max(dot(nrmN, normalize(u.sun.xyz)), 0.0) * diffGate * sunThru;
  // EVS ground tracks the real light level (atmo.h: 0.08 night floor .. 1 day) so night is genuinely
  // dark under the star field; SVS (OSM) stays a constant daylit database view.
  let light = select(1.0, 0.08 + 0.92 * A.skyExtra.x, A.skyExtra.y > 0.5);
  let ambW = kTerrainAmb + kOvercastAmb * (1.0 - sunThru);
  var c = base * (ambW + kTerrainDir * diff) * light;   // scene RADIANCE in linear — the tonemap compresses
  // AERIAL PERSPECTIVE, from the weather rather than from a clear-air table: the same sigma0, the same
  // two scale heights and the same inscatter colour the cloud deck dissolves into (FBAtmoHaze.h). That
  // is the whole point of the shared header — deck and ground see ONE atmosphere, so a mountain and the
  // cloud above it fade at the same rate and the horizon has no edge. Per channel, so the ridge fifty
  // kilometres out loses its blue on the way here instead of going uniformly grey.
  let distM = length(in.wpos);
  let hz = hazeTransmittance3(u.haze.x, 0.5 * (u.haze.y + fragAltM), distM);
  c = c * hz + hazeInscatter(svLUT, lsamp, A, vdir) * (vec3f(1.0) - hz);
  return vec4f(c, 1.0);
}
)";

/* The storage-buffer and draw-loop bound; a multi-LOD cut to 240 km stays well under it. */
static const int kMaxDraws = 4096;

/* Mip levels are built off this thread; the renderer only uploads what fb_stream_pyramid hands it. */
static int MipCountFor(int ts) { return fb_mip_count(ts); }

static int fbPhotoZmax(void) { const char *e = getenv("FB_PHOTO_ZMAX"); return e ? atoi(e) : 11; }
static float fbTileYlin(const uint8_t *pyramid, int ts) {
  const uint8_t *top = pyramid + (fb_pyramid_bytes(ts) - 4);   /* 1x1 mip = the tile MEAN (sRGB bytes) */
  fb_srgb_lut_();
  return 0.2126f * fb_srgb_lin_[top[0]] + 0.7152f * fb_srgb_lin_[top[1]] + 0.0722f * fb_srgb_lin_[top[2]];
}

void FBTilesStage::Configure(const FBGpu &gpu, wgpu::Sampler samp, wgpu::Sampler lutSamp,
                             wgpu::TextureView skyLutView, wgpu::Buffer atmoBuf, int maxLayers) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  HdrFormat = gpu.HdrFormat;
  Samp = samp;
  LutSamp = lutSamp;
  SkyLutView = skyLutView;
  AtmoBuf = atmoBuf;
  MaxLayers = maxLayers;

  /* Streaming = a growable array FBWorld fills and recycles; static = one layer per tile. */
  if (Streaming) {
    LayerCap = 64;
    LayerUsed = 0;
    wgpu::TextureDescriptor td{};
    td.size = {(uint32_t)AlbedoTS, (uint32_t)AlbedoTS, (uint32_t)LayerCap};
    td.mipLevelCount = (uint32_t)MipCountFor(AlbedoTS);
    td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
    td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
    Albedo = Device.CreateTexture(&td);
  } else {
    const uint32_t TS = AlbedoData.empty() ? 256u : (uint32_t)AlbedoTS;
    const uint32_t layers = (uint32_t)(NTiles > 0 ? NTiles : 1);

    wgpu::TextureDescriptor td{};
    td.size = {TS, TS, layers};
    td.mipLevelCount = (uint32_t)MipCountFor((int)TS);
    td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
    td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    Albedo = Device.CreateTexture(&td);

    std::vector<uint8_t> pyr(fb_pyramid_bytes((int)TS));
    auto uploadLayer = [&](uint32_t layer, const uint8_t *data) {
      fb_build_pyramid(data, (int)TS, pyr.data());
      WriteAlbedoLayer((int)layer, pyr.data(), (int)TS);
    };

    std::vector<uint8_t> checker;
    auto makeChecker = [&]() {
      checker.resize((size_t)TS * TS * 4);
      for (uint32_t y = 0; y < TS; y++)
        for (uint32_t x = 0; x < TS; x++) {
          bool c = ((x >> 5) ^ (y >> 5)) & 1u;
          uint8_t *o = &checker[(y * TS + x) * 4];
          o[0] = c ? 200 : 40; o[1] = c ? 20 : 40; o[2] = c ? 200 : 40; o[3] = 255;
        }
    };

    const size_t layerBytes = (size_t)TS * TS * 4;
    for (uint32_t i = 0; i < layers; i++) {
      if (AlbedoData.size() >= (size_t)(i + 1) * layerBytes)
        uploadLayer(i, AlbedoData.data() + (size_t)i * layerBytes);
      else {
        if (checker.empty()) makeChecker();
        uploadLayer(i, checker.data());
      }
    }
  }

  /* osmmesh emits a triangle SOUP (6 verts/quad), so both paths draw non-indexed.
   * TODO: weld + index on upload — it would halve the traffic. */
  wgpu::BufferDescriptor bd{};
  if (!Streaming) {
    bd.size = (uint64_t)TerrainNVerts * 8 * sizeof(float);
    bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Vtx = Device.CreateBuffer(&bd);
    Queue.WriteBuffer(Vtx, 0, TerrainVerts.data(), (size_t)TerrainNVerts * 8 * sizeof(float));
  }

  bd.size = kUniFloats * sizeof(float);   /* mat4 + sun + haze + deck bases + deck transmittances */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  /* Storage, not uniform: it scales past the 64 KB uniform limit. Rewritten every frame. */
  int entries = Streaming ? kMaxDraws : (NTiles > 0 ? NTiles : 1);
  bd.size = (uint64_t)entries * 8 * sizeof(float);
  bd.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  TileBuf = Device.CreateBuffer(&bd);

  /* Atmo struct + sky-view sampling + THE shared air (constants, extinction law, inscatter colour) —
   * the same three splices the cloud pass makes, in the same order. */
  const std::string terrSrc = std::string(kAtmoCommon) + kAtmoSample + FBHazeConstsWGSL() + kHazeWGSL
                            + kTerrainWGSL;
  wgpu::ShaderSourceWGSL wgsl{};
  wgsl.code = terrSrc.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wgsl;
  wgpu::ShaderModule sm = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attrs[3] = {};
  attrs[0].format = wgpu::VertexFormat::Float32x3; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
  attrs[1].format = wgpu::VertexFormat::Float32x2; attrs[1].offset = 12; attrs[1].shaderLocation = 1;
  attrs[2].format = wgpu::VertexFormat::Float32x3; attrs[2].offset = 20; attrs[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 32;
  vbl.attributeCount = 3;
  vbl.attributes = attrs;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = true;
  ds.depthCompare = wgpu::CompareFunction::Greater;  /* [0,1] reversed-Z: nearer = larger */

  wgpu::ColorTargetState ct{};
  ct.format = HdrFormat;   /* scene renders into the offscreen HDR target, not the swapchain */

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = sm;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = sm;
  fs.targetCount = 1;
  fs.targets = &ct;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  rp.primitive.cullMode = wgpu::CullMode::None;  /* TODO: cull once the ECEF soup winding is pinned */
  Pipe = Device.CreateRenderPipeline(&rp);

  RebuildBind();
}

void FBTilesStage::SetStaticMesh(const float *verts, uint32_t nverts, int ntiles, const uint32_t *voff,
                                 const uint32_t *vcnt, const double *origins) {
  TerrainVerts.assign(verts, verts + (size_t)nverts * 8);
  TerrainNVerts = nverts;
  NTiles = ntiles;
  TileOff.assign(voff, voff + ntiles);
  TileCnt.assign(vcnt, vcnt + ntiles);
  TileOrigin.assign(origins, origins + (size_t)ntiles * 3);
}

void FBTilesStage::SetAlbedoArray(const uint8_t *rgba, int ts, int layers) {
  AlbedoTS = ts;
  AlbedoData.assign(rgba, rgba + (size_t)layers * ts * ts * 4);
}

/* Recreate at double the cap and copy the resident layers over. Rare. */
void FBTilesStage::EnsureAlbedoCap(int need) {
  if (need <= LayerCap) return;
  int cap = LayerCap ? LayerCap : 64;
  while (cap < need) cap *= 2;
  if (cap > MaxLayers) cap = MaxLayers;
  if (need > cap) return;   /* at the device array-layer ceiling — caller handles the -1 */

  const int mips = MipCountFor(AlbedoTS);
  wgpu::TextureDescriptor td{};
  td.size = {(uint32_t)AlbedoTS, (uint32_t)AlbedoTS, (uint32_t)cap};
  td.mipLevelCount = (uint32_t)mips;
  td.format = wgpu::TextureFormat::RGBA8UnormSrgb;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
  wgpu::Texture grown = Device.CreateTexture(&td);

  if (Albedo && LayerCap > 0) {   /* carry every resident layer AND its whole mip chain across */
    wgpu::CommandEncoder enc = Device.CreateCommandEncoder();
    for (int lv = 0; lv < mips; lv++) {
      uint32_t d = (uint32_t)(AlbedoTS >> lv); if (d == 0) d = 1;
      wgpu::TexelCopyTextureInfo src{}, dst{};
      src.texture = Albedo; src.mipLevel = (uint32_t)lv;
      dst.texture = grown;  dst.mipLevel = (uint32_t)lv;
      wgpu::Extent3D ext{d, d, (uint32_t)LayerCap};
      enc.CopyTextureToTexture(&src, &dst, &ext);
    }
    wgpu::CommandBuffer cmd = enc.Finish();
    Queue.Submit(1, &cmd);
  }
  Albedo = grown;
  LayerCap = cap;
  RebuildBind();
}

/* Also called whenever EnsureAlbedoCap swaps the array texture out: a bind group PINS a view. */
void FBTilesStage::RebuildBind(void) {
  wgpu::TextureViewDescriptor avd{};
  avd.dimension = wgpu::TextureViewDimension::e2DArray;
  wgpu::BindGroupEntry be[7] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = TileBuf; be[1].size = TileBuf.GetSize();
  be[2].binding = 2; be[2].sampler = Samp;
  be[3].binding = 3; be[3].textureView = Albedo.CreateView(&avd);
  be[4].binding = 4; be[4].sampler = LutSamp;            /* azimuth-wrapping, for the sky-view LUT */
  be[5].binding = 5; be[5].textureView = SkyLutView;     /* the haze's inscatter colour */
  be[6].binding = 6; be[6].buffer = AtmoBuf; be[6].size = 11 * 4 * sizeof(float);
  wgpu::BindGroupDescriptor bgd{};
  bgd.layout = Pipe.GetBindGroupLayout(0);
  bgd.entryCount = 7;
  bgd.entries = be;
  Bind = Device.CreateBindGroup(&bgd);
}

/* A recycled slot or a freshly grown one; -1 at the device ceiling. */
int FBTilesStage::AllocLayer(void) {
  if (!FreeLayers.empty()) { int l = FreeLayers.back(); FreeLayers.pop_back(); return l; }
  EnsureAlbedoCap(LayerUsed + 1);
  if (LayerUsed >= LayerCap) return -1;   /* 2048 ceiling */
  return LayerUsed++;
}

/* No mip building here — that already ran off-thread or synchronously. */
void FBTilesStage::WriteAlbedoLayer(int layer, const uint8_t *pyramid, int ts) {
  const uint8_t *p = pyramid;
  int w = ts, level = 0;
  for (;;) {
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = Albedo;
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

void FBTilesStage::SetLayerPhoto(int layer, float ylin, int z) {
  if (layer < 0) return;
  if (layer >= (int)LayerKind.size()) { LayerKind.resize((size_t)layer + 1, 0); LayerYlin.resize((size_t)layer + 1, 0.0f); }
  if (layer >= (int)Gains.size()) Gains.resize((size_t)layer + 1, 1.0f);
  LayerYlin[layer] = ylin;
  LayerKind[layer] = (int8_t)(z < fbPhotoZmax() ? 1 : 2);   /* 1 = far (gets gain), 2 = near (Ytarget ref) */
}

void FBTilesStage::ClearLayer(int layer) {
  if (layer < 0 || layer >= (int)LayerKind.size()) return;
  LayerKind[layer] = 0;
  if (layer < (int)Gains.size()) Gains[layer] = 1.0f;
}

/* EMA-smoothed, so the far field does not flicker as tiles stream and evict. */
void FBTilesStage::UpdatePhotoGains(void) {
  double sum = 0.0; int n = 0;
  for (size_t l = 0; l < LayerKind.size(); l++)
    if (LayerKind[l] == 2 && LayerYlin[l] > 1e-4f) { sum += LayerYlin[l]; n++; }
  if (n > 0) {
    double mean = sum / n;
    const char *ae = getenv("FB_PHOTO_EMA"); double a = ae ? atof(ae) : 0.08;   /* smoothing rate/frame */
    PhotoYTarget = PhotoYValid ? PhotoYTarget * (1.0 - a) + mean * a : mean;
    PhotoYValid = true;
  }
  const char *ge = getenv("FB_PHOTO_MAXGAIN"); float maxg = ge ? (float)atof(ge) : 2.5f;
  int nfar = 0; double gsum = 0;
  for (size_t l = 0; l < LayerKind.size(); l++) {
    float g = 1.0f;
    if (LayerKind[l] == 1 && PhotoYValid && LayerYlin[l] > 1e-4f) {
      g = (float)(PhotoYTarget / LayerYlin[l]);
      if (g < 1.0f) g = 1.0f; else if (g > maxg) g = maxg;
      nfar++; gsum += g;
    }
    if (l < Gains.size()) Gains[l] = g;
  }
  static long frameNoDbg = 0;   /* log cadence only — not a correctness-sensitive value */
  frameNoDbg++;
  if (getenv("FB_PHOTO_LOG") && (frameNoDbg % 60) == 0)
    FBLog::Debug("render", "photogain", {{"Ytarget", (double)PhotoYTarget}, {"near", n}, {"far", nfar},
                                         {"avgGain", nfar ? gsum / nfar : 1.0}});
}

int FBTilesStage::UploadTile(const float *verts, uint32_t nverts, const double origin[3],
                             const uint8_t *albedo, int ts, int z) {
  AlbedoTS = ts;

  int layer = AllocLayer();
  if (layer < 0) return -1;
  WriteAlbedoLayer(layer, albedo, ts);
  if (BaseMode == 1) SetLayerPhoto(layer, fbTileYlin(albedo, ts), z);   /* base is photo -> tracked for gain */
  else ClearLayer(layer);                                               /* OSM base -> no gain */

  int slot = -1;
  for (int i = 0; i < (int)DynTiles.size(); i++)
    if (!DynTiles[i].Used) { slot = i; break; }
  if (slot < 0) { slot = (int)DynTiles.size(); DynTiles.push_back(DynTile{}); }

  DynTile &d = DynTiles[slot];
  wgpu::BufferDescriptor bd{};
  bd.size = (uint64_t)nverts * 8 * sizeof(float);
  bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  d.Vtx = Device.CreateBuffer(&bd);
  Queue.WriteBuffer(d.Vtx, 0, verts, (size_t)nverts * 8 * sizeof(float));
  d.NVerts = nverts;
  for (int a = 0; a < 3; a++) d.Origin[a] = origin[a];
  d.Layer = layer;
  d.PhotoLayer = -1;   /* fetched lazily on the first EVS toggle (UploadTilePhoto) */
  d.PhotoUpTick = 0;
  d.Used = true;
  return slot;
}

int FBTilesStage::UploadTilePhoto(int slot, const uint8_t *photo, int ts, int z, unsigned frameNo) {
  if (slot < 0 || slot >= (int)DynTiles.size() || !DynTiles[slot].Used) return 0;
  DynTile &d = DynTiles[slot];
  if (d.PhotoLayer >= 0) return 1;   /* already attached */
  int layer = AllocLayer();
  if (layer < 0) return 0;            /* array full — caller stops retrying, tile stays OSM */
  WriteAlbedoLayer(layer, photo, ts);
  if (BaseMode == 0) SetLayerPhoto(layer, fbTileYlin(photo, ts), z);   /* overlay is photo when base is OSM */
  else ClearLayer(layer);
  d.PhotoLayer = layer;
  d.PhotoUpTick = frameNo;   /* 2-phase: draw the photo layer only once its upload is committed */
  return 1;
}

void FBTilesStage::ReleaseTile(int slot) {
  if (slot < 0 || slot >= (int)DynTiles.size() || !DynTiles[slot].Used) return;
  DynTile &d = DynTiles[slot];
  d.Vtx = nullptr;   /* drop the ref -> buffer freed */
  d.Used = false;
  ClearLayer(d.Layer);
  FreeLayers.push_back(d.Layer);
  if (d.PhotoLayer >= 0) { ClearLayer(d.PhotoLayer); FreeLayers.push_back(d.PhotoLayer); d.PhotoLayer = -1; }
}

void FBTilesStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  UpdatePhotoGains();   /* adaptive Ytarget from near photo tiles -> per-frame far-tile gains */

  static std::vector<float> off;
  int nDraw;
  if (Streaming) {
    nDraw = (int)DrawList.size();
    if (nDraw > kMaxDraws) nDraw = kMaxDraws;
    off.assign((size_t)nDraw * 8, 0.0f);
    for (int i = 0; i < nDraw; i++) {
      const DynTile &d = DynTiles[DrawList[i]];
      off[i * 8 + 0] = (float)(d.Origin[0] - ctx.Eye[0]);
      off[i * 8 + 1] = (float)(d.Origin[1] - ctx.Eye[1]);
      off[i * 8 + 2] = (float)(d.Origin[2] - ctx.Eye[2]);
      /* `layerMode` records which ground mode the chosen layer actually IS, so the mode-strictness
       * invariant is MEASURABLE rather than assumed. */
      int want = ctx.GroundPhoto ? 1 : 0;
      bool overlayReady = (d.PhotoLayer >= 0 && ctx.FrameNo > d.PhotoUpTick + 1);
      int layer, layerMode;
      if (want == BaseMode) { layer = d.Layer; layerMode = BaseMode; }
      else if (overlayReady) { layer = d.PhotoLayer; layerMode = want; }   /* overlay layer == the wanted mode */
      else { layer = d.Layer; layerMode = BaseMode; }                       /* fallback = the base = wrong mode */
      off[i * 8 + 3] = (float)layer;
      off[i * 8 + 4] = (layer >= 0 && layer < (int)Gains.size()) ? Gains[layer] : 1.0f;   /* photo brightness gain */
      if (layer < 0) { NotReadyDraws++; BlackDraws++; }
      else if (layerMode != want) WrongModeDraws++;   /* SVS showing EVS or vice-versa (mode bleed) */
    }
  } else {
    nDraw = NTiles;
    off.assign((size_t)nDraw * 8, 0.0f);
    for (int i = 0; i < nDraw; i++) {
      off[i * 8 + 0] = (float)(TileOrigin[i * 3 + 0] - ctx.Eye[0]);
      off[i * 8 + 1] = (float)(TileOrigin[i * 3 + 1] - ctx.Eye[1]);
      off[i * 8 + 2] = (float)(TileOrigin[i * 3 + 2] - ctx.Eye[2]);
      off[i * 8 + 3] = (float)i;   /* static: albedo layer == tile index */
      off[i * 8 + 4] = 1.0f;
    }
  }
  if (nDraw > 0) Queue.WriteBuffer(TileBuf, 0, off.data(), off.size() * sizeof(float));

  /* The camera-relative MVP + sun (0..19, as the frame context carries them) plus the ATMOSPHERE the
   * fragment shader needs as numbers: one Koschmieder sigma0, the geometry to turn a fragment offset
   * into an altitude, and the three decks' sun transmittance. Reused stack array — no per-frame heap. */
  float uni[kUniFloats];
  std::memcpy(uni, ctx.Mvp20, sizeof ctx.Mvp20);
  const double eyeLen = std::sqrt(ctx.Eye[0] * ctx.Eye[0] + ctx.Eye[1] * ctx.Eye[1] + ctx.Eye[2] * ctx.Eye[2]);
  uni[20] = FBHazeSigma0(Sky.VisibilityM);
  uni[21] = ctx.AltM;
  uni[22] = (float)((eyeLen - (double)ctx.AltM) / 1.0e6);   /* the REAL WGS84 radius under the camera */
  uni[23] = 0.0f;
  const float sunUp = (float)(ctx.SunDir[0] * ctx.Up[0] + ctx.SunDir[1] * ctx.Up[1] + ctx.SunDir[2] * ctx.Up[2]);
  float groundThru = 1.0f;
  for (int i = 0; i < 3; i++) {
    const FBCloudDeckParams &d = Sky.Deck[i];
    const float tau = FBDeckSunOpticalDepth(d, sunUp);
    uni[24 + i * 4 + 0] = d.BaseM;
    uni[24 + i * 4 + 1] = d.TopM;
    uni[24 + i * 4 + 2] = tau;
    uni[24 + i * 4 + 3] = d.Cover;
    groundThru *= FBDeckSunTransmittance(tau, d.Cover, 1.0f);   /* the value at sea level = the log's */
  }
  /* The air and the light this frame, as NUMBERS: what the picture does with them is measurable off a
   * PNG, but why it does it is not. Logged when the atmosphere actually changes, like cloud_decks. */
  if (std::fabs(uni[20] - LoggedSigma0) > 1e-9f || std::fabs(groundThru - LoggedSunThru) > 1e-4f) {
    LoggedSigma0 = uni[20];
    LoggedSunThru = groundThru;
    FBLog::Debug("render", "terrain_light",
                 {{"visM", (double)Sky.VisibilityM}, {"sigma0PerM", (double)uni[20]},
                  {"camAltM", (double)ctx.AltM}, {"sunUp", (double)sunUp},
                  {"lowTau", (double)uni[26]}, {"midTau", (double)uni[30]}, {"highTau", (double)uni[34]},
                  {"groundSunThru", (double)groundThru},
                  {"groundLitFrac", (double)(3.0f * groundThru / (0.4f + 0.15f * (1.0f - groundThru) + 3.0f * groundThru))}});
  }
  Queue.WriteBuffer(Uni, 0, uni, sizeof uni);

  /* Signature = the draw STRUCTURE only. TileBuf/uniform CONTENTS change every frame, but the bundle
   * references those buffers by HANDLE — so only a structural change forces a re-record. */
  if (Streaming && nDraw > 0) {
    uint64_t sig = 1469598103934665603ULL;
    auto mix = [&sig](uint64_t v) { sig ^= v; sig *= 1099511628211ULL; };
    mix((uint64_t)nDraw);
    mix((uint64_t)(uintptr_t)Bind.Get());
    for (int i = 0; i < nDraw; i++) {
      const DynTile &d = DynTiles[DrawList[i]];
      mix((uint64_t)(uintptr_t)d.Vtx.Get());
      mix((uint64_t)d.NVerts);
    }
    if (sig != BundleSig || !Bundle) {
      wgpu::TextureFormat cf = HdrFormat;
      wgpu::RenderBundleEncoderDescriptor rbd{};
      rbd.colorFormatCount = 1;
      rbd.colorFormats = &cf;
      rbd.depthStencilFormat = wgpu::TextureFormat::Depth32Float;
      wgpu::RenderBundleEncoder rbe = Device.CreateRenderBundleEncoder(&rbd);
      rbe.SetPipeline(Pipe);
      rbe.SetBindGroup(0, Bind);
      for (int i = 0; i < nDraw; i++) {
        const DynTile &d = DynTiles[DrawList[i]];
        rbe.SetVertexBuffer(0, d.Vtx);
        rbe.Draw(d.NVerts, 1, 0, (uint32_t)i);
      }
      Bundle = rbe.Finish();
      BundleSig = sig;
      TerrainBundleRecords++;
    }
  }

  if (Streaming) {   /* per-tile buffers baked into Bundle; firstInstance = draw index -> storage entry */
    if (Bundle && nDraw > 0) pass.ExecuteBundles(1, &Bundle);
  } else {
    pass.SetPipeline(Pipe);
    pass.SetBindGroup(0, Bind);
    pass.SetVertexBuffer(0, Vtx);
    for (int i = 0; i < NTiles; i++)   /* firstInstance = i -> instance_index picks tile i's offset */
      pass.Draw(TileCnt[i], 1, TileOff[i], (uint32_t)i);
  }
}

} // namespace FlightBox::Render
