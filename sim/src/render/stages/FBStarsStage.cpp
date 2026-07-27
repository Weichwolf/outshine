#include "FBStarsStage.h"
#include <cmath>

namespace FlightBox::Render {

/* HYG star-field placement, Polaris-pinned. */
static double GmstDeg(double unixSec) {   /* Greenwich mean sidereal time, deg (IAU J2000 polynomial) */
  double jd = unixSec / 86400.0 + 2440587.5, dd = jd - 2451545.0;
  double g = std::fmod(280.46061837 + 360.98564736629 * dd, 360.0);
  return g < 0 ? g + 360.0 : g;
}
/* RA/dec + local sidereal time + latitude -> ENU; 0 if at or below the horizon. */
static int StarEnu(double lstDeg, double latDeg, double raDeg, double decDeg, double out[3]) {
  const double RAD = 3.14159265358979 / 180.0;
  double H = (lstDeg - raDeg) * RAD, dec = decDeg * RAD;
  double sl = std::sin(latDeg * RAD), cl = std::cos(latDeg * RAD);
  double sinAlt = sl * std::sin(dec) + cl * std::cos(dec) * std::cos(H);
  if (sinAlt <= 0.03) return 0;   /* ~1.7deg margin: refraction + terrain (stars.h) */
  double az = std::atan2(-std::cos(dec) * std::sin(H), std::sin(dec) * cl - std::cos(dec) * sl * std::cos(H));
  double ca = std::sqrt(std::max(0.0, 1.0 - sinAlt * sinAlt));
  out[0] = ca * std::sin(az);   /* east */
  out[1] = ca * std::cos(az);   /* north */
  out[2] = sinAlt;              /* up */
  return 1;
}
/* B-V colour index -> spectral tint. */
static void StarColour(float bv, float out[3]) {
  float t = bv < -0.4f ? -0.4f : bv > 1.8f ? 1.8f : bv;
  const float blue[3] = {0.61f, 0.70f, 1.0f}, white[3] = {1, 1, 1}, yellow[3] = {1.0f, 0.96f, 0.84f};
  const float orange[3] = {1.0f, 0.80f, 0.55f}, red[3] = {1.0f, 0.62f, 0.42f};
  auto lerp = [&](const float a[3], const float b[3], float f) {
    for (int i = 0; i < 3; i++) out[i] = a[i] + (b[i] - a[i]) * f;
  };
  if (t < 0.0f) lerp(blue, white, (t + 0.4f) / 0.4f);
  else if (t < 0.6f) lerp(white, yellow, t / 0.6f);
  else if (t < 1.2f) lerp(yellow, orange, (t - 0.6f) / 0.6f);
  else lerp(orange, red, (t - 1.2f) / 0.6f);
}

static const char *kStarWGSL = R"(
struct SU { mvp : mat4x4f, p : vec4f };   /* p = (dayFade, sizeScale, viewportW, viewportH) */
@group(0) @binding(0) var<uniform> su : SU;
struct VOut { @builtin(position) pos : vec4f, @location(0) uv : vec2f,
              @location(1) bright : f32, @location(2) col : vec3f };
@vertex fn vs(@builtin(vertex_index) vi : u32, @location(0) ipos : vec3f,
              @location(1) ibr : f32, @location(2) icol : vec3f) -> VOut {
  var q = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(-1.0, 1.0),
                          vec2f(-1.0, 1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0));
  let corner = q[vi];
  var clip = su.mvp * vec4f(ipos, 1.0);
  /* Stars are POINT sources: radius is hard-capped ~2px and barely tracks brightness — brightness goes
   * into HDR intensity + a tight glow halo (below), NOT the disk diameter, so bright stars don't bloat. */
  let px = clamp(1.2 + 0.4 * ibr, 1.2, 1.9) * su.p.y;
  clip.x = clip.x + corner.x * (2.0 * px / su.p.z) * clip.w;
  clip.y = clip.y + corner.y * (2.0 * px / su.p.w) * clip.w;
  var o : VOut;
  o.pos = clip; o.uv = corner; o.bright = ibr; o.col = icol;
  return o;
}
@fragment fn fs(in : VOut) -> @location(0) vec4f {
  let r = sqrt(dot(in.uv, in.uv));
  let core = smoothstep(0.55, 0.0, r);           /* tight ~1px core — the point itself */
  let halo = smoothstep(1.0, 0.15, r) * 0.3;     /* faint wide glow — lets Venus/Sirius shine, dim stars stay points */
  let a = (core + halo) * in.bright * (1.0 - su.p.x);
  return vec4f(in.col * a, a);
}
)";

void FBStarsStage::Init(const FBGpu &gpu) {
  Device = gpu.Device;
  Queue = gpu.Queue;

  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = kStarWGSL;
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attr[3] = {};
  attr[0].format = wgpu::VertexFormat::Float32x3; attr[0].offset = 0;  attr[0].shaderLocation = 0;
  attr[1].format = wgpu::VertexFormat::Float32;   attr[1].offset = 12; attr[1].shaderLocation = 1;
  attr[2].format = wgpu::VertexFormat::Float32x3; attr[2].offset = 16; attr[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = 7 * sizeof(float);
  vbl.stepMode = wgpu::VertexStepMode::Instance;   /* one entry per star, 6 verts per instance */
  vbl.attributeCount = 3;
  vbl.attributes = attr;

  wgpu::BlendState blend{};                        /* additive: stars accumulate, never darken */
  blend.color.srcFactor = wgpu::BlendFactor::One;  blend.color.dstFactor = wgpu::BlendFactor::One;
  blend.alpha.srcFactor = wgpu::BlendFactor::One;  blend.alpha.dstFactor = wgpu::BlendFactor::One;
  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;
  ct.blend = &blend;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthWriteEnabled = false;
  ds.depthCompare = wgpu::CompareFunction::Always;   /* at infinity; terrain paints over them */

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
  bd.size = 20 * sizeof(float);   /* mat4 + vec4 */
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be{};
  be.binding = 0; be.buffer = Uni; be.size = 20 * sizeof(float);
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 1;
  bg.entries = &be;
  Bind = Device.CreateBindGroup(&bg);
}

void FBStarsStage::SetCatalogue(const uint8_t *hyg, int nbytes, double originLat, double originLon) {
  int n = nbytes / 6;
  Cat.clear();
  Lat = originLat;
  Lon = originLon;
  if (n <= 0) { NStars = 0; return; }
  Cat.reserve((size_t)n * 4);
  for (int i = 0; i < n; i++) {
    const uint8_t *p = hyg + i * 6;
    uint16_t ra = (uint16_t)(p[0] | (p[1] << 8));
    int16_t dec = (int16_t)(p[2] | (p[3] << 8));
    Cat.push_back((float)ra / 65536.0f * 360.0f);
    Cat.push_back((float)dec / 32767.0f * 90.0f);
    Cat.push_back((float)p[4] / 255.0f * 8.0f - 1.5f);
    Cat.push_back((float)p[5] / 255.0f * 3.0f - 0.5f);
  }
  NStars = n;
  DirAt = -1e30;   /* force a rebuild on the next Update */
}

void FBStarsStage::Update(double nowSec) {
  if (NStars <= 0) return;
  if (DirAt > -1e29 && nowSec - DirAt < 20.0) return;

  double lst = std::fmod(GmstDeg(nowSec) + Lon, 360.0);
  if (lst < 0) lst += 360.0;
  /* Star ENU -> ECEF via the origin's own ENU axes. */
  const double RAD = 3.14159265358979 / 180.0;
  double P = Lat * RAD, L = Lon * RAD;
  double sP = std::sin(P), cP = std::cos(P), sL = std::sin(L), cL = std::cos(L);
  double E[3] = {-sL, cL, 0.0}, N[3] = {-sP * cL, -sP * sL, cP}, U[3] = {cP * cL, cP * sL, sP};
  const double R = 40000.0;

  Dir.clear();
  int vis = 0;
  for (int i = 0; i < NStars; i++) {
    double enu[3];
    if (!StarEnu(lst, Lat, Cat[i * 4], Cat[i * 4 + 1], enu)) continue;
    double ec[3];
    for (int a = 0; a < 3; a++) ec[a] = (E[a] * enu[0] + N[a] * enu[1] + U[a] * enu[2]) * R;
    float mag = Cat[i * 4 + 2], bv = Cat[i * 4 + 3];
    float bright = 1.45f - 0.42f * mag;   /* shaders.h: brighter (lower mag) -> more intense */
    bright = bright < 0.12f ? 0.12f : bright > 1.5f ? 1.5f : bright;
    float col[3]; StarColour(bv, col);
    Dir.push_back((float)ec[0]); Dir.push_back((float)ec[1]); Dir.push_back((float)ec[2]);
    Dir.push_back(bright); Dir.push_back(col[0]); Dir.push_back(col[1]); Dir.push_back(col[2]);
    vis++;
  }
  NStarVis = vis;
  DirAt = nowSec;
  if (vis <= 0) return;

  size_t bytes = (size_t)vis * 7 * sizeof(float);
  if (InstCap < vis) {
    wgpu::BufferDescriptor bd{};
    bd.size = bytes;
    bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Inst = Device.CreateBuffer(&bd);
    InstCap = vis;
  }
  Queue.WriteBuffer(Inst, 0, Dir.data(), bytes);
}

void FBStarsStage::Encode(const FBFrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  if (!(ctx.GroundPhoto && ctx.DayFade < 0.6f && NStarVis > 0)) return;
  float su[20];
  for (int i = 0; i < 16; i++) su[i] = ctx.Mvp20[i];   /* same camera-relative MVP as the terrain */
  su[16] = ctx.DayFade; su[17] = 1.0f; su[18] = (float)ctx.Width; su[19] = (float)ctx.Height;
  Queue.WriteBuffer(Uni, 0, su, sizeof su);
  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Inst);
  pass.Draw(6, (uint32_t)NStarVis);
}

} // namespace FlightBox::Render
