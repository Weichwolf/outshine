#include "World.h"
#include "Renderer.h"
#include "TerrainLoader.h"
#include "Camera.h"
#include "Geodesy.h"
#include "Log.h"
#include "Store.h"
#include "Unit.h"
#include "UnitRegistry.h"
#include "Units.h"
#include "geo.h"
#include "VegetationTemplates.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <string>

namespace outshine::World {

static const int kRootZ = 8;

static double Clock(void) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
}

static const int kMaxZ = 14;
/* [SET] the zoom whose tile is the anchor of the ground shader's world-fixed noise frame: 39 km at
 * the reference latitude, so the surface is continuous over the whole visible near and mid field and
 * the float offset it costs resolves 2.3 mm. */
static const int kAnchorZ = 10;
static const int kGrace = 180;                 /* passes unasked before eviction (lru.h hysteresis) */
static const double kEarthCirc = 40075016.686;
/* LOD is PURELY distance-based: height variance is deliberately NOT in the decision, so a flat near
 * tile refines as far as a rugged one at the same distance (equal albedo resolution by distance).
 * kSseK = H / (2 tan(fov/2)) is the pixel focal length.
 * Herleitung + Konstantentabelle: doc/world/terrain.md, Abschnitt 2.2. */
static const double kSseK = 720.0 / (2.0 * 0.57735026919);   /* H / (2 tan(fov/2)) */
static const double kEdgeTau = 384.0;   /* target max on-screen tile edge (px); lower = finer = more tiles */
/* Quads per tile edge, and it is the ZERO POINT of the whole LOD chain: the cluster DAG's level 0 IS
 * this mesh, so no tolerance below its own decimation error can ever be honoured. kEdgeTau/kGrid is
 * that error in pixels -- 4 px against the DAG's declared 1 px (render/ClusterDag.h). 96 is where the
 * price stops: a 256-texel DEM tile would carry 255, and 128 measured 1029 MB of tile VRAM against
 * 615 at 96 and p95 18.4 ms against 13.1. doc/world/terrain.md 2.2. */
static const int kGrid = 96;
static const double kCosView = 0.5;            /* frustum weight: <60deg off-axis -> full priority */
static const int kNodeCeil = 6000;             /* safety backstop on the working set */

/* Packed (z,x,y) node key: z<16, x/y<2^30 (z<=29). */
static inline uint64_t Key(int z, long x, long y) {
  return ((uint64_t)(z & 0xF) << 60) | ((uint64_t)(x & 0x3FFFFFFF) << 30) | (uint64_t)(y & 0x3FFFFFFF);
}
static std::unordered_map<uint64_t, int> gIndex;

World::World()
  : R(nullptr), TS(512), ViewM(6000.0), Lat0(0), Lon0(0),
    Pass(0), Evicted(0), LastLog(0), Leaves(0), DrawnReady(0), Pending(0), TargetTot(0), TargetRdy(0), MeshVram(0),
    NightLights(false), Anchor{0, 0, 0}, LightsResident(0) {}

World::~World() { if (Opened) fb_stream_close(); }

/* Cosmetic LUT: the shader gets colour * (intensity/255) * brightness, additive. */
static const float kLightColor[8][3] = {
  {1.00f, 0.55f, 0.22f},   /* 0 residential  — sodium orange */
  {1.00f, 0.68f, 0.36f},   /* 1 primary      — amber */
  {0.75f, 0.85f, 1.00f},   /* 2 motorway     — cool white */
  {1.00f, 0.78f, 0.48f},   /* 3 building     — warm window */
  {0.70f, 0.85f, 1.00f},   /* 4 commercial   — cool */
  {0.90f, 0.95f, 1.00f},   /* 5 aerodrome    — white */
  {1.00f, 0.16f, 0.10f},   /* 6 tower        — obstruction red */
  {1.00f, 0.66f, 0.38f},   /* 7 city glow    — amber aggregate */
};
static const float kLightRadiusM[8] = {7.f, 9.f, 9.f, 6.f, 7.f, 12.f, 10.f, 40.f};
static const float kLightBright[8]  = {1.0f, 1.2f, 1.3f, 0.9f, 1.0f, 1.5f, 1.2f, 0.8f};
static const int kLightBudget = 65536;   /* max sprites resident (team-lead cap) */
static const double kLightLiftM = 6.0;   /* sit lights just above the DEM so terrain occludes cleanly */
static const float kLightGain = 3.0f;    /* additive HDR gain so cores pop (ACES compresses highlights) */

bool World::Open(Render::Renderer *renderer, const char *tilesBase, double lat, double lon,
                   double viewMeters, int albedoTS) {
  R = renderer;
  TS = albedoTS;
  ViewM = viewMeters;
  Lat0 = lat;
  Lon0 = lon;
  gIndex.clear();
  if (fb_stream_open(tilesBase, lat, lon, kRootZ) == 0) return false;
  Cls_.Open(lat, lon);
  Opened = true;
  /* Sprite positions are stored relative to this anchor, so the renderer holds float offsets. */
  osmmesh_geo g0{}; g0.lat = lat; g0.lon = lon; g0.alt = 0.0;
  osmmesh_ecef a0 = osmmesh_geo_to_ecef(g0);
  Anchor[0] = a0.x; Anchor[1] = a0.y; Anchor[2] = a0.z;
  if (R) R->SetLightAnchor(Anchor);
  return true;
}

double World::SpanM(int z) const {
  return kEarthCirc * std::cos(Lat0 * kPi / 180.0) / (double)(1L << z);
}

void World::Center(int z, long x, long y, double out[3]) const {
  osmmesh_geo g = osmmesh_tile_frac_to_geo((uint8_t)z, (uint32_t)x, (uint32_t)y, 0.5, 0.5);
  g.alt = 0.0;
  osmmesh_ecef e = osmmesh_geo_to_ecef(g);
  out[0] = e.x; out[1] = e.y; out[2] = e.z;
}

static double Dist(const double a[3], const double b[3]) {
  double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

int World::Ensure(int z, long x, long y) {
  uint64_t k = Key(z, x, y);
  auto it = gIndex.find(k);
  if (it != gIndex.end()) return it->second;
  Node nd{};
  nd.z = z; nd.x = x; nd.y = y; nd.slot = -1;
  int idx = (int)Nodes.size();
  Nodes.push_back(std::move(nd));
  gIndex[k] = idx;
  return idx;
}

/* A child that fails only PREVENTS the parent's split; it never voids coverage. No side effects. */
bool World::Viable(int z, long x, long y, const double eye[3]) const {
  long n = 1L << z;
  if (x < 0 || y < 0 || x >= n || y >= n) return false;
  double c[3];
  Center(z, x, y, c);
  double dist = Dist(c, eye);
  double span = SpanM(z);
  return dist - span * 0.71 <= ViewM;
}

void World::AddWork(int idx, int z, long x, long y, const double eye[3], const double fwd[3]) {
  double c[3];
  if (Nodes[idx].haveMesh) { c[0] = Nodes[idx].origin[0]; c[1] = Nodes[idx].origin[1]; c[2] = Nodes[idx].origin[2]; }
  else Center(z, x, y, c);
  double dist = Dist(c, eye);
  if (dist < 1.0) dist = 1.0;
  double dir[3] = {(c[0] - eye[0]) / dist, (c[1] - eye[1]) / dist, (c[2] - eye[2]) / dist};
  double d = dir[0] * fwd[0] + dir[1] * fwd[1] + dir[2] * fwd[2];
  double weight = d > kCosView ? 1.0 : 0.05;   /* in-frustum refines first */
  WorkList.push_back(Work{idx, weight / dist});
}

void World::Emit(int idx) {
  Leaves++;
  Node &n = Nodes[idx];
  if (Ready(n)) {   /* committed on the GPU (uploaded + one pass elapsed) — safe to draw */
    n.emitPass = Pass;   /* drawn this pass -> a mode switch may keep showing it (old mode) until the overlay lands */
    DrawnReady++;
    MeshVram += (long)n.nverts * 32;
    DrawSlots.push_back(n.slot);
    /* The drawn LOD cut, unconditionally: it hosts the night lights AND it is the only list that says
     * which mesh the near-field ground field may rasterise. Gating it on NightLights made the ground
     * field silently empty in daylight. */
    DrawnLeaves.push_back(idx);
  } else {
    Pending++;
  }
}

/* lightState 1 = decoded (the buffer may be empty for a dark tile), -1 = pending. */
void World::BuildLights(int idx) {
  Node &n = Nodes[idx];
  if (!n.haveMesh) { n.lightState = -1; return; }   /* need origin height first; retry a later pass */
  if ((int)LightBytes.size() < 65540) LightBytes.resize(65540);
  int nb = fb_stream_lights(n.z, (uint32_t)n.x, (uint32_t)n.y, LightBytes.data(), (int)LightBytes.size());
  if (nb < 4) { n.lightState = -1; return; }         /* pending (0) or too big — retry next pass */
  const uint8_t *p = LightBytes.data();
  int count = (int)p[0] | ((int)p[1] << 8);
  n.lightInst.clear();
  /* One height per tile is plenty for point lights seen from altitude (a z14 tile is < 2 km). */
  osmmesh_ecef oe{n.origin[0], n.origin[1], n.origin[2]};
  double groundAsl = osmmesh_ecef_to_geo(oe).alt;
  int have = nb - 4 < count * 6 ? (nb - 4) / 6 : count;
  n.lightInst.reserve((size_t)have * 7);
  for (int i = 0; i < have; i++) {
    const uint8_t *r = p + 4 + i * 6;
    double fx = ((int)r[0] | ((int)r[1] << 8)) / 65535.0;
    double fy = ((int)r[2] | ((int)r[3] << 8)) / 65535.0;
    int cls = r[4] & 7;
    float inten = (float)r[5] / 255.0f;
    osmmesh_geo g = osmmesh_tile_frac_to_geo((uint8_t)n.z, (uint32_t)n.x, (uint32_t)n.y, fx, fy);
    g.alt = groundAsl + kLightLiftM;
    osmmesh_ecef e = osmmesh_geo_to_ecef(g);
    float w = inten * kLightBright[cls] * kLightGain;
    n.lightInst.push_back((float)(e.x - Anchor[0]));
    n.lightInst.push_back((float)(e.y - Anchor[1]));
    n.lightInst.push_back((float)(e.z - Anchor[2]));
    n.lightInst.push_back(kLightRadiusM[cls]);
    n.lightInst.push_back(kLightColor[cls][0] * w);
    n.lightInst.push_back(kLightColor[cls][1] * w);
    n.lightInst.push_back(kLightColor[cls][2] * w);
  }
  n.lightState = 1;
}

/* A pure function of camera and tile geometry, needing NO tile data — which is what lets the
 * boot/teleport request go straight to the final leaves, with no LOD ladder and no rebuild churn. */
bool World::WantSplit(int z, long x, long y, const double eye[3]) const {
  if (z >= kMaxZ || (int)Nodes.size() >= kNodeCeil) return false;
  double c[3]; Center(z, x, y, c);
  double dist = Dist(c, eye); if (dist < 1.0) dist = 1.0;
  return SpanM(z) * kSseK / dist > kEdgeTau;
}

int World::Find(int z, long x, long y) const {
  auto it = gIndex.find(Key(z, x, y));
  return it == gIndex.end() ? -1 : it->second;
}

/* Can this subtree cover its area with tiles READY this pass? */
bool World::CanCover(int z, long x, long y, const double eye[3]) const {
  if (WantSplit(z, x, y, eye)) {
    bool anyV = false, allC = true;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      if (!CanCover(z + 1, cx, cy, eye)) allC = false;
    }
    if (anyV && allC) return true;
  }
  int idx = Find(z, x, y);
  return idx >= 0 && Ready(Nodes[idx]);
}

/* Intermediate nodes are only touched (kept alive as refine-holds); only target leaves get AddWork'd. */
void World::RequestSubtree(int z, long x, long y, const double eye[3], const double fwd[3]) {
  int idx = Ensure(z, x, y);
  Nodes[idx].touch = Pass;
  if (WantSplit(z, x, y, eye)) {
    bool anyV = false;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      RequestSubtree(z + 1, cx, cy, eye, fwd);
    }
    if (anyV) return;   /* intermediate — the children carry the request */
  }
  if (!Uploaded(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);   /* TARGET leaf */
}

/* The draw traversal; 1 = this node's area is fully covered by emitted tiles. No intermediate LOD
 * level is ever built — only the geometry-target leaves. Ablauf: doc/world/terrain.md §2.3. */
int World::Descend(int z, long x, long y, const double eye[3], const double fwd[3]) {
  int idx = Ensure(z, x, y);
  Nodes[idx].touch = Pass;
  if (WantSplit(z, x, y, eye)) {
    bool anyV = false, allC = true;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      if (!CanCover(z + 1, cx, cy, eye)) allC = false;
    }
    if (anyV) {
      if (allC) {   /* the refined level is fully ready -> draw the children */
        for (int q = 0; q < 4; q++) {
          long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
          if (Viable(z + 1, cx, cy, eye)) Descend(z + 1, cx, cy, eye, fwd);
        }
        return 1;
      }
      if (Ready(Nodes[idx])) {   /* resident coarse tile holds the whole area while targets stream */
        for (int q = 0; q < 4; q++) {
          long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
          if (Viable(z + 1, cx, cy, eye)) RequestSubtree(z + 1, cx, cy, eye, fwd);
        }
        Emit(idx);
        return 1;
      }
      /* self not resident: emit ready descendants, the rest is a hole the loading screen covers */
      for (int q = 0; q < 4; q++) {
        long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
        if (Viable(z + 1, cx, cy, eye)) Descend(z + 1, cx, cy, eye, fwd);
      }
      return 0;
    }
  }
  /* A base-resident but overlay-pending leaf still EMITS (never a hole) but reports not mode-covered,
   * so an ancestor keeps holding. */
  if (!Uploaded(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);
  Emit(idx);
  return Ready(Nodes[idx]) ? 1 : 0;
}

/* Side-effect-free walk of the target cut: the LoadProgress that gates the loading screen. */
void World::CountTargets(int z, long x, long y, const double eye[3], int &total, int &ready) const {
  if (WantSplit(z, x, y, eye)) {
    bool anyV = false;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      CountTargets(z + 1, cx, cy, eye, total, ready);
    }
    if (anyV) return;
  }
  total++;
  int idx = Find(z, x, y);
  if (idx >= 0 && Ready(Nodes[idx])) ready++;
}

/* ---- THE EFFECTS ------------------------------------------------------------------------------
 * Every number below is a RENDERING quantity and is marked [SET]: the simulation publishes an
 * afterburner BIT, a cartridge's bloom TIME and a motor's burn TIME, and says nothing whatever about
 * how long a plume looks or how wide a chaff cloud gets. What is NOT set here is every quantity the
 * simulation does publish — the burn window comes from the store catalogue, the flare's intensity
 * history from core/Countermeasure.h's own curve, the nozzle from the mesh.
 * Sizes are metres and brightnesses are radiance: doc/render/units-visual.md §9. */
static const float kFlamePlumeLenPerRadius = 11.0f;    /* [SET] visible AB plume vs nozzle exit radius */
static const float kFlameWidthPerRadius = 0.95f;      /* [SET] the efflux necks slightly at the petals */
static const float kFlameGain = 1.0f;                 /* [SET] scale on the shader's own flame radiance ramp */
static const float kMotorHalfLenM = 2.5f;             /* [SET] a boosting rocket's visible flame */
static const float kMotorRadiusM = 0.55f;             /* [SET] */
static const float kMotorGain = 1.7f;                 /* [SET] a solid motor outshines a turbine's plume */
static const size_t kMaxCrumbs = 96;                  /* [SET] bounded memory per trail */
static const float kSmokeRadiance = 1.2f;             /* [SET] sunlit white smoke, the terrain's own level */
static const float kFlareRadiusM = 4.5f;              /* [SET] the LUMINOUS ball: the grain is centimetres, what an eye sees is the glow around it */
static const float kFlareGain = 1.0f;                 /* [SET] scale on the shader's core+halo radiance */
static const float kChaffR0M = 0.6f, kChaffRMaxM = 18.0f;   /* [SET] a dipole cloud dispersing */
static const float kChaffAlpha = 0.45f;               /* [SET] peak opacity of the bloomed bundle */
static const size_t kMaxSpriteDraws = 512;            /* mirrors SpritesStage's own instance cap */

/* THE DETONATION. Its SIZE is not [SET]: a burst is drawn on the scale of the thing it went off
 * against, and that scale is published (VisualSignature, the damage layout read as geometry). What
 * is set is the fraction and the clock — a warhead going off on an airframe makes a ball about half
 * as wide as the airframe is long, and a ball that has stopped emitting after ~1.6 s. */
static const float kBlastRadiusPerDimM = 0.55f;       /* [SET] ball radius vs the target's largest dimension */
static const float kBlastMinRadiusM = 4.0f;           /* [SET] a target that published no geometry at all */
static const double kBlastLifeS = 1.6;                /* [SET] flash -> billow -> soot */
static const float kBlastGrow0 = 0.40f;               /* [SET] r(p) = R * (kBlastGrow0 + (1-kBlastGrow0)*sqrt(p)) */
static const size_t kMaxBlasts = 24;                  /* [SET] bounded: the oldest is dropped, never the newest */
/* THE COLUMN the ball leaves. Four puffs is what it takes to read as a rising column rather than as a
 * string of balls at the sizes below; the rise rate is buoyant plume order, not wind. */
static const int kBlastPuffs = 4;                     /* [SET] */
static const double kBlastPuffDelayS = 0.5;           /* [SET] */
static const double kBlastSmokeLifeS = 11.0;          /* [SET] */
static const double kSmokeRiseMs = 6.0;               /* [SET] buoyant rise of the column */
/* DISPERSION IS THE AIR'S, NOT THE TARGET'S: how wide a column gets is set by how long it has had to
 * mix, so it is an absolute radius rather than a multiple of what burned. Measured against the first
 * attempt, which scaled it off the wreck: a 4 m target's column was 11 m wide and 156 m tall — a
 * thread, invisible past 2 km, where the whole point is a mark that stays readable. */
static const float kColumnRMaxM = 45.0f;              /* [SET] a standing column at the end of its life */
static const float kBlastSmokeRMaxM = 35.0f;          /* [SET] the ball's own column, shorter lived */
static const float kBlastSmokeRadiance = 0.10f;       /* [SET] burnt smoke: dark, and it stays dark at night */
static const float kBlastSmokeAlpha0 = 0.75f;         /* [SET] */

/* WHAT IS LEFT. A unit whose register says it cannot finish the sortie burns; on the ground the fire
 * and its column persist for the rest of the run, which is the whole point — a destroyed target has
 * to LOOK destroyed the next time the camera comes past. */
static const float kWreckFirePerDimM = 0.45f;         /* [SET] flame height vs the wreck's largest dimension */
static const float kWreckFireWidthFrac = 0.55f;       /* [SET] tongue width vs its height */
static const float kWreckFireGain = 0.85f;            /* [SET] scale on the shader's flame ramp */
static const double kWreckFlickerHz = 1.7;            /* [SET] the noise's scroll rate along the tongue */
static const double kWreckAglM = 8.0;                 /* [SET] below this a burning unit is DOWN, not flying */
static const double kWreckColumnStepS = 1.1;          /* [SET] one puff of the standing column per this */
static const double kWreckColumnLifeS = 26.0;         /* [SET] */
/* The trail a HOLED airframe lays. Same machinery as a motor's, four times finer and twice as long
 * lived, because it is laid at 200 m/s instead of at 700 and has to read from behind. */
static const TrailStyle kMotorTrail = {250.0, 20.0, 1.0f, 16.0f, 0.80f, 1.2f};
static const TrailStyle kDamageTrail = {60.0, 40.0, 2.5f, 34.0f, 0.55f, 0.10f};

/* THE LAMPS. Radiance times the shader's own core/halo amplitude; the sub-pixel energy floor does the
 * rest, so one number covers a light at 100 m and the same light at 8 km. Measured against a mondless
 * night sky in the browser: doc/render/units-visual.md §11. */
static const float kNavLightRadM = 0.30f;             /* [SET] the GLOW, not the lens: a 6 cm lamp is a 30 cm ball of light at night */
/* [DERIVED] from the sub-pixel energy floor, which is what makes ONE number serve every range. Below
 * full resolution a sprite contributes col * (kLightCore*kLightCoreMean + kLightHalo*kLightHaloMean)
 * * (R*0.5*H/tan(fov/2) / 0.7072 / d)^2 = col * 1.806 * (R*881.6/d)^2. Solving that for a clearly
 * readable 0.30 radiance at d = 2 km with R = 0.30 m gives 9.5. Measured at that setting: see below. */
static const float kNavGain = 9.5f;
static const float kNavRed[3] = {1.00f, 0.045f, 0.030f};    /* [SET] aviation red, port */
static const float kNavGreen[3] = {0.050f, 1.00f, 0.220f};  /* [SET] aviation green, starboard */
static const float kNavWhite[3] = {1.00f, 0.98f, 0.92f};    /* [SET] the tail light */
static const float kStrobeGain = 4.0f;                /* [SET] times kNavGain: a strobe outshines a position light */
static const double kStrobePeriodS = 1.0;             /* [SET] ~60 flashes/min, the civil and military norm */
static const double kStrobeDutyS = 0.07;              /* [SET] */
static const float kHeadlampGain = 3.0f;              /* [SET] times kNavGain: a driving lamp, seen from the air */
static const double kMoverMs = 1.0;                   /* [SET] below this a ground unit is parked, and parked is dark */
static const float kSunDarkDeg = -3.0f;               /* [SET] the same threshold the tile lights are gated on */

World::UnitFx &World::Fx(int id) {
  for (UnitFx &f : Fx_)
    if (f.Id == id) { f.Touch = Pass; return f; }
  UnitFx f;
  f.Id = id;
  f.Touch = Pass;
  f.FirstSeenS = SimTimeS_;
  Fx_.push_back(f);
  return Fx_.back();
}

bool World::Dark() const { return SunElDeg < kSunDarkDeg; }

/* The largest dimension this unit publishes of itself; a unit whose module declares no damage layout
 * publishes nothing at all, and then the floor is what a warhead does on its own. */
static float PublishedSizeM(const Units::VisualSignature &v) {
  float m = v.FrontalM;
  if (v.LateralM > m) m = v.LateralM;
  if (v.PlanM > m) m = v.PlanM;
  return m;
}

/* WHAT THIS UNIT IS DOING THAT AN EYE WOULD SEE, and every input is something the unit itself
 * published. Nothing here can write back: an SpriteDraw carries no simulation type, exactly like the
 * UnitDraw beside it. */
void World::AddUnitEffects(const Units::Unit &u, const Units::UnitSignature &sig, const double ecef[3],
                             const double fwd[3], const double right[3], const double up[3], bool isEye) {
  if (SpriteDraws_.size() >= kMaxSpriteDraws) return;

  /* 1 — THE NOZZLE. The bit is `Afterburner`, published off the ENGINE (units/SimUnit), and it is
   * the whole visible difference: an F-16 at military power shows no flame at all, in augmentation it
   * shows several metres of one. Nothing finer is published, so nothing finer is drawn. Never for the
   * eye unit: its own nozzle is six metres behind the camera. */
  float nzOff[3], nzRad = 0.0f;
  if (!isEye && sig.Afterburner && sig.Visual.TypeName[0] && R &&
      R->UnitNozzle(sig.Visual.TypeName, nzOff, nzRad) && nzRad > 0.0f) {
    /* Model space is glTF (+X right, +Y up, +Z aft), so the exhaust axis is -fwd and the offset rides
     * the very basis the airframe itself is drawn with — CameraBasisEcef's, borrowed, not rebuilt. */
    const float halfLen = 0.5f * kFlamePlumeLenPerRadius * nzRad;
    Render::SpriteDraw s;
    for (int i = 0; i < 3; i++) {
      const double ex = right[i] * nzOff[0] + up[i] * nzOff[1] - fwd[i] * nzOff[2];
      s.Ecef[i] = ecef[i] + ex - fwd[i] * (double)halfLen;
      s.Axis[i] = -(float)fwd[i];
    }
    s.HalfLenM = halfLen;
    s.RadiusM = kFlameWidthPerRadius * nzRad;
    s.Color[0] = s.Color[1] = s.Color[2] = kFlameGain;
    s.Alpha = 0.0f;
    s.Param = 1.0f;   /* the shock train a choked convergent-divergent nozzle shows */
    s.Kind = (uint32_t)Render::SpriteKind::Flame;
    SpriteDraws_.push_back(s);
  }

  if (!HaveSimTime_) return;   /* no clock, no age — and every effect below is an age */

  /* 2 — THE ROUND. A weapon unit's motor burns for exactly as long as the CATALOGUE says its type
   * burns (core/Store.h Perf.BoostS + SustainS), counted from the frame the round first appeared —
   * which is the frame it was created in, because a released store becomes a unit at launch. */
  if (u.GetKind() == Units::UnitKind::Weapon) {
    double motorS = 0.0;
    if (const StoreSpec *spec = FindStore(sig.Visual.TypeName))
      motorS = spec->Perf.BoostS + spec->Perf.SustainS;
    const UnitFx &fx = Fx(u.GetId());
    const bool burning = motorS > 0.0 && SimTimeS_ - fx.FirstSeenS < motorS;
    AddSmokeTrail(u, ecef, burning, kMotorTrail);
    if (burning && SpriteDraws_.size() < kMaxSpriteDraws) {
      /* No round publishes a LENGTH (a missile module declares no damage layout), so the flame hangs
       * at the unit's own origin rather than at an invented tail station — below its own radius at any
       * range a round is seen from. */
      Render::SpriteDraw s;
      for (int i = 0; i < 3; i++) {
        s.Ecef[i] = ecef[i] - fwd[i] * (double)kMotorHalfLenM;
        s.Axis[i] = -(float)fwd[i];
      }
      s.HalfLenM = kMotorHalfLenM;
      s.RadiusM = kMotorRadiusM;
      s.Color[0] = kMotorGain; s.Color[1] = kMotorGain * 1.15f; s.Color[2] = kMotorGain * 1.4f;   /* a rocket burns whiter than a turbine */
      s.Param = 0.5f;
      s.Kind = (uint32_t)Render::SpriteKind::Flame;
      SpriteDraws_.push_back(s);
    }
  }

  /* 3 — THE EXPENDABLES. Position and bloom time are published per cartridge; the AGE CURVES are the
   * simulation's own free functions, not a second opinion — a flare's radiometric history is the same
   * combustion whether a seeker or an eye is looking at it, and a chaff bundle is not a cloud before
   * it blooms for the eye either. */
  for (int i = 0; i < kMaxFlareClouds && SpriteDraws_.size() < kMaxSpriteDraws; i++) {
    const FlareCloud &f = sig.Flare[i];
    if (!f.Active) continue;
    const double n = FlareIrNorm(SimTimeS_ - f.BloomS);
    if (n <= 0.0) continue;
    Render::SpriteDraw s;
    GeoToEcef(f.LatDeg, f.LonDeg, f.AltM, s.Ecef);
    s.HalfLenM = s.RadiusM = kFlareRadiusM;
    s.Color[0] = s.Color[1] = s.Color[2] = kFlareGain * (float)n;
    s.Kind = (uint32_t)Render::SpriteKind::Flare;
    SpriteDraws_.push_back(s);
  }
  for (int i = 0; i < kMaxChaffClouds && SpriteDraws_.size() < kMaxSpriteDraws; i++) {
    const ChaffCloud &c = sig.Chaff[i];
    if (!c.Active) continue;
    const double age = SimTimeS_ - c.BloomS;
    const double n = ChaffRcsNorm(age);
    if (n <= 0.0) continue;
    const float grow = (float)std::sqrt(age / kChaffLifeS);
    Render::SpriteDraw s;
    GeoToEcef(c.LatDeg, c.LonDeg, c.AltM, s.Ecef);
    s.HalfLenM = s.RadiusM = kChaffR0M + (kChaffRMaxM - kChaffR0M) * grow;
    s.Alpha = kChaffAlpha * (float)n;
    for (int k = 0; k < 3; k++) s.Color[k] = kSmokeRadiance * s.Alpha;   /* premultiplied */
    s.Param = 0.6f;
    s.Kind = (uint32_t)Render::SpriteKind::Smoke;
    SpriteDraws_.push_back(s);
  }

  /* 4 — THE HIT. The ONLY trigger is a RISE in the published burst count: that number is monotone, it
   * has exactly one writer (core/DamageModel), and it goes up once per detonation and once per gun
   * bundle that went into the airframe. A timer here would be an invented battle; a level test would
   * draw the same explosion for the rest of the run. The ball is sized off the target's own published
   * silhouette, because a warhead going off against a MiG makes a MiG-sized ball. */
  {
    UnitFx &fx = Fx(u.GetId());
    if (sig.Damage.Hits > fx.Hits) {
      const float dim = PublishedSizeM(sig.Visual);
      SpawnBlast(ecef, dim > 0.0f ? kBlastRadiusPerDimM * dim : kBlastMinRadiusM, u.GetId());
    }
    fx.Hits = sig.Damage.Hits;
  }

  /* 5 — WHAT IS LEFT of it, and the same rule holds: the register decides, not a clock. */
  if (!sig.Damage.CombatEffective) {
    const Units::UnitPose p = u.GetPose();
    AddWreckFire(u, sig, p, ecef, up);
  }

  /* 6 — THE LAMPS, gated on the sun the client already gates the ground lights on. Drawn for the eye
   * unit too: in a chase view they are ON the aircraft the camera is watching. */
  if (Dark()) {
    if (u.GetKind() == Units::UnitKind::Aircraft) {
      AddNavLights(u, sig, ecef, fwd, right, up);
    } else if (u.GetKind() == Units::UnitKind::Ground) {
      /* A vehicle that is MOVING is a vehicle with its lamps on; a position that is parked is dark,
       * which is both the correct picture and the one that does not paint a hidden battery for a
       * watcher. Speed is published, so nothing here decides what a unit is. */
      const Units::UnitPose p = u.GetPose();
      if (p.SpeedMs > kMoverMs) {
        double lamp[3];
        for (int side = -1; side <= 1; side += 2) {
          for (int k = 0; k < 3; k++) lamp[k] = ecef[k] + fwd[k] * 2.0 + right[k] * (0.9 * side);
          const float gain = kNavGain * kHeadlampGain;
          AddLight(lamp, kNavLightRadM, gain * kNavWhite[0], gain * kNavWhite[1], gain * kNavWhite[2]);
        }
      }
    }
  }
}

/* THE TRAIL, and it is a memory of PUBLISHED POSES: a crumb is laid where the round was seen while its
 * motor was burning, and the segment between two crumbs is drawn until it has dispersed. The renderer
 * integrates nothing — without the crumbs a pose alone cannot say where the round has been. */
void World::AddSmokeTrail(const Units::Unit &u, const double ecef[3], bool laying,
                            const TrailStyle &st) {
  UnitFx &fx = Fx(u.GetId());
  if (laying) {
    bool lay = fx.Trail.empty();
    if (!lay) {
      const FxCrumb &last = fx.Trail.back();
      double d2 = 0.0;
      for (int i = 0; i < 3; i++) { const double dx = ecef[i] - last.Ecef[i]; d2 += dx * dx; }
      lay = d2 > st.StepM * st.StepM;
    }
    if (lay) {
      FxCrumb c;
      for (int i = 0; i < 3; i++) c.Ecef[i] = ecef[i];
      c.T = SimTimeS_;
      fx.Trail.push_back(c);
      if (fx.Trail.size() > kMaxCrumbs) fx.Trail.erase(fx.Trail.begin());
    }
  }
  while (!fx.Trail.empty() && SimTimeS_ - fx.Trail.front().T > st.LifeS) fx.Trail.erase(fx.Trail.begin());
  if (fx.Trail.size() < 2) return;

  /* The head of the trail follows the round itself while the motor burns, so the plume stays attached
   * instead of ending one crumb behind. */
  for (size_t i = 0; i + 1 < fx.Trail.size() + (laying ? 1 : 0); i++) {
    if (SpriteDraws_.size() >= kMaxSpriteDraws) return;
    const FxCrumb &a = fx.Trail[i];
    const double *b = (i + 1 < fx.Trail.size()) ? fx.Trail[i + 1].Ecef : ecef;
    double seg[3];
    double len2 = 0.0;
    for (int k = 0; k < 3; k++) { seg[k] = b[k] - a.Ecef[k]; len2 += seg[k] * seg[k]; }
    const double len = std::sqrt(len2);
    if (len < 1.0) continue;
    const double age = SimTimeS_ - a.T;
    const double life = age / st.LifeS;
    if (life >= 1.0) continue;
    Render::SpriteDraw s;
    for (int k = 0; k < 3; k++) {
      s.Ecef[k] = a.Ecef[k] + 0.5 * seg[k];
      s.Axis[k] = (float)(seg[k] / len);
    }
    s.RadiusM = st.R0M + (st.RMaxM - st.R0M) * (float)std::sqrt(life);
    /* Half a radius of overlap at each end: neighbouring segments then BUTT rather than bead, which is
     * what the first trail frame showed at every join (measured: a 5 px notch at 2.5 km). */
    s.HalfLenM = (float)(0.5 * len) + 0.5f * s.RadiusM;
    s.Alpha = st.Alpha0 * (float)((1.0 - life) * (1.0 - life));
    for (int k = 0; k < 3; k++) s.Color[k] = st.Radiance * s.Alpha;   /* premultiplied */
    s.Param = 0.35f;
    s.Kind = (uint32_t)Render::SpriteKind::Smoke;
    SpriteDraws_.push_back(s);
  }
}

void World::SpawnBlast(const double ecef[3], float radiusM, int unitId) {
  if (Blasts_.size() >= kMaxBlasts) Blasts_.erase(Blasts_.begin());   /* the OLDEST goes, never this one */
  FxBlast b;
  for (int i = 0; i < 3; i++) b.Ecef[i] = ecef[i];
  b.T0 = SimTimeS_;
  b.RadiusM = radiusM;
  /* Deterministic per unit and per event: the same run twice draws the same turbulence. */
  b.Seed = (float)((unitId * 37 + (int)(SimTimeS_ * 8.0)) % 251);
  Blasts_.push_back(b);
}

/* THE BALL AND ITS COLUMN. A blast belongs to the PLACE it happened at: the round that carried it is
 * retired in the same tick and the target may fly on out of the frame, so neither of them can host it.
 * The column is four puffs on a delay rather than one growing sprite, because a rising column is a
 * SEQUENCE of releases and a single sprite that grows reads as a balloon. */
void World::AddBlasts() {
  for (size_t i = 0; i < Blasts_.size();) {
    const FxBlast &b = Blasts_[i];
    const double age = SimTimeS_ - b.T0;
    if (age < 0.0 || age > kBlastSmokeLifeS + kBlastPuffs * kBlastPuffDelayS) {
      Blasts_.erase(Blasts_.begin() + (long)i);
      continue;
    }
    i++;
    if (SpriteDraws_.size() >= kMaxSpriteDraws) continue;
    double up[3];
    double len = std::sqrt(b.Ecef[0] * b.Ecef[0] + b.Ecef[1] * b.Ecef[1] + b.Ecef[2] * b.Ecef[2]);
    if (len < 1.0) len = 1.0;
    for (int k = 0; k < 3; k++) up[k] = b.Ecef[k] / len;

    if (age < kBlastLifeS) {
      const float p = (float)(age / kBlastLifeS);
      Render::SpriteDraw s;
      for (int k = 0; k < 3; k++) { s.Ecef[k] = b.Ecef[k]; s.Axis[k] = (float)up[k]; }
      s.HalfLenM = s.RadiusM = b.RadiusM * (kBlastGrow0 + (1.0f - kBlastGrow0) * std::sqrt(p));
      s.Color[0] = s.Color[1] = s.Color[2] = 1.0f;   /* the arc lives in the shader's own constants */
      s.Alpha = 1.0f;
      s.Phase = p;
      s.Seed = b.Seed;
      s.Kind = (uint32_t)Render::SpriteKind::Fireball;
      SpriteDraws_.push_back(s);
    }
    for (int j = 0; j < kBlastPuffs && SpriteDraws_.size() < kMaxSpriteDraws; j++) {
      const double pa = age - j * kBlastPuffDelayS;
      if (pa <= 0.0 || pa >= kBlastSmokeLifeS) continue;
      const double life = pa / kBlastSmokeLifeS;
      Render::SpriteDraw s;
      for (int k = 0; k < 3; k++) s.Ecef[k] = b.Ecef[k] + up[k] * (kSmokeRiseMs * pa);
      const float r0 = 0.5f * b.RadiusM;
      s.HalfLenM = s.RadiusM = r0 + (kBlastSmokeRMaxM - r0) * (float)std::sqrt(life);
      s.Alpha = kBlastSmokeAlpha0 * (float)((1.0 - life) * (1.0 - life));
      for (int k = 0; k < 3; k++) s.Color[k] = kBlastSmokeRadiance * s.Alpha;   /* premultiplied */
      s.Param = 0.5f;
      s.Kind = (uint32_t)Render::SpriteKind::Smoke;
      SpriteDraws_.push_back(s);
    }
  }
}

/* WHAT IS LEFT OF A UNIT THAT CANNOT FINISH ITS SORTIE. Two pictures from one published bit: still
 * flying, it lays a black trail behind itself (the SAME crumb memory a motor uses, different numbers);
 * down, it stands and burns for the rest of the run, and the burning is anchored to the ground under
 * it rather than to the model origin so a wreck does not float. */
void World::AddWreckFire(const Units::Unit &u, const Units::UnitSignature &sig,
                           const Units::UnitPose &p, const double ecef[3], const double up[3]) {
  UnitFx &fx = Fx(u.GetId());
  if (fx.BurnSinceS < 0.0) fx.BurnSinceS = SimTimeS_;
  const double burnS = SimTimeS_ - fx.BurnSinceS;
  const bool down = p.ElevM - p.GroundAslM < kWreckAglM || u.GetKind() == Units::UnitKind::Ground;
  if (!down) {
    AddSmokeTrail(u, ecef, true, kDamageTrail);
    return;
  }
  const float dim = PublishedSizeM(sig.Visual);
  const float height = dim > 0.0f ? kWreckFirePerDimM * dim : kBlastMinRadiusM;
  const float seed = (float)(u.GetId() * 17 % 251);
  if (SpriteDraws_.size() < kMaxSpriteDraws) {
    Render::SpriteDraw s;
    for (int k = 0; k < 3; k++) {
      s.Ecef[k] = ecef[k] + up[k] * (0.5 * (double)height);
      s.Axis[k] = (float)up[k];
    }
    s.HalfLenM = 0.5f * height;
    s.RadiusM = kWreckFireWidthFrac * s.HalfLenM;
    s.Color[0] = s.Color[1] = s.Color[2] = kWreckFireGain;
    s.Phase = (float)(SimTimeS_ * kWreckFlickerHz);
    s.Seed = seed;
    s.Kind = (uint32_t)Render::SpriteKind::Fire;
    SpriteDraws_.push_back(s);
  }
  /* The standing column: puffs released on a fixed cadence since the unit was first seen burning, so
   * it is as tall as the fire is old and does not restart when the camera looks away. */
  const int n = (int)(kWreckColumnLifeS / kWreckColumnStepS);
  for (int j = 0; j < n && SpriteDraws_.size() < kMaxSpriteDraws; j++) {
    const double pa = std::fmod(burnS, kWreckColumnStepS) + j * kWreckColumnStepS;
    if (pa >= kWreckColumnLifeS || pa > burnS) continue;
    const double life = pa / kWreckColumnLifeS;
    Render::SpriteDraw s;
    for (int k = 0; k < 3; k++) s.Ecef[k] = ecef[k] + up[k] * (kSmokeRiseMs * pa);
    const float r0 = 0.6f * height;
    s.HalfLenM = s.RadiusM = r0 + (kColumnRMaxM - r0) * (float)std::sqrt(life);
    s.Alpha = kBlastSmokeAlpha0 * (float)((1.0 - life) * (1.0 - life));
    for (int k = 0; k < 3; k++) s.Color[k] = kBlastSmokeRadiance * s.Alpha;   /* premultiplied */
    s.Param = 0.5f;
    s.Kind = (uint32_t)Render::SpriteKind::Smoke;
    SpriteDraws_.push_back(s);
  }
}

void World::AddLight(const double ecef[3], float radiusM, float r, float g, float b) {
  if (SpriteDraws_.size() >= kMaxSpriteDraws) return;
  Render::SpriteDraw s;
  for (int k = 0; k < 3; k++) s.Ecef[k] = ecef[k];
  s.HalfLenM = s.RadiusM = radiusM;
  s.Color[0] = r; s.Color[1] = g; s.Color[2] = b;
  s.Kind = (uint32_t)Render::SpriteKind::Light;
  SpriteDraws_.push_back(s);
}

/* THE LAMPS AN AIRFRAME CARRIES, and their POSITIONS are published rather than tabulated: the wingtips
 * sit at half the frontal dimension, the tail at nearly half the lateral one, and both come out of the
 * damage layout read as geometry (units/Unit.h VisualSignature) — the same numbers the eye uses to
 * decide whether it can tell what kind of aeroplane it is looking at. A unit that publishes no
 * silhouette carries no lamps, which is the honest picture for a released store. */
void World::AddNavLights(const Units::Unit &u, const Units::UnitSignature &sig, const double ecef[3],
                           const double fwd[3], const double right[3], const double up[3]) {
  const float span = sig.Visual.FrontalM, len = sig.Visual.LateralM;
  if (span <= 0.0f || len <= 0.0f) return;
  /* A hair OUTBOARD of the published half-span, and that is geometry rather than fudge: the published
   * dimension is where the airframe ENDS, the lens sits on that end, and its glow is centred half a
   * ball further out. Placed exactly at the half-span the lamp is inside the wing's own depth and the
   * airframe occludes it — measured: only the tail light survived, at 132/255, the two wingtips at 1. */
  const double outboard = 0.5 * (double)span + 2.0 * (double)kNavLightRadM;
  double port[3], stbd[3], tail[3];
  for (int k = 0; k < 3; k++) {
    port[k] = ecef[k] - right[k] * outboard;
    stbd[k] = ecef[k] + right[k] * outboard;
    tail[k] = ecef[k] - fwd[k] * (0.45 * (double)len) + up[k] * (0.06 * (double)len);
  }
  AddLight(port, kNavLightRadM, kNavGain * kNavRed[0], kNavGain * kNavRed[1], kNavGain * kNavRed[2]);
  AddLight(stbd, kNavLightRadM, kNavGain * kNavGreen[0], kNavGain * kNavGreen[1], kNavGain * kNavGreen[2]);
  AddLight(tail, kNavLightRadM, kNavGain * kNavWhite[0], kNavGain * kNavWhite[1], kNavGain * kNavWhite[2]);
  /* The anti-collision strobe is the one lamp that is a CLOCK, because in the aeroplane it is one: a
   * flasher unit fires on its own period whatever the aircraft is doing. Phase is offset per unit id
   * so a four-ship does not blink in unison. */
  const double phase = std::fmod(SimTimeS_ + 0.137 * (double)(u.GetId() % 8), kStrobePeriodS);
  if (phase < kStrobeDutyS) {
    const float gain = kNavGain * kStrobeGain;
    AddLight(port, kNavLightRadM, gain * kNavWhite[0], gain * kNavWhite[1], gain * kNavWhite[2]);
    AddLight(stbd, kNavLightRadM, gain * kNavWhite[0], gain * kNavWhite[1], gain * kNavWhite[2]);
  }
}

/* THE ONE PLACE the picture reads the cast, and it reads nothing but the PUBLISHED pose — the barrier
 * units/SimUnit::PublishPose sets, never a foreign FDM. Nothing is written back: the record is a
 * value copy, and UnitDraw carries no simulation type at all, so there is no handle to write through.
 * doc/render/units-visual.md, Spec. */
void World::PublishUnits() {
  UnitDraws_.clear();
  SpriteDraws_.clear();
  if (!R) return;
  if (Units_) {
    for (const Units::Unit *u : Units_->Units()) {
      if (!u) continue;
      const Units::UnitSignature sig = u->GetSignature();
      const Units::UnitPose p = u->GetPose();
      double ecef[3];
      GeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, ecef);
      /* The airframe's own axes in ECEF, from the same function the camera uses — so a jet drawn at a
       * pose and a camera placed at that pose cannot disagree. glTF is +X right, +Y up, -Z forward. */
      double fwd[3], right[3], up[3];
      CameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
      const bool isEye = u->GetId() == EyeUnitId_;
      /* THE EFFECTS COME FIRST and they are drawn for the EYE UNIT TOO: its own flares fall behind it
       * and are in the picture the moment it turns, while its airframe (below) is the one thing the
       * camera may not draw, because the eye sits inside it. */
      AddUnitEffects(*u, sig, ecef, fwd, right, up, isEye);
      if (isEye) continue;
      if (!sig.Visual.TypeName[0]) continue;   /* nothing published to look at: no model, no draw */
      Render::UnitDraw d;
      for (int i = 0; i < 3; i++) {
        d.Ecef[i] = ecef[i];
        d.Rot[0 * 3 + i] = (float)right[i];
        d.Rot[1 * 3 + i] = (float)up[i];
        d.Rot[2 * 3 + i] = -(float)fwd[i];
      }
      const Units::UnitArticulation &a = p.Art;
      d.Art[(int)Render::ArtChannel::AileronL] = a.AileronLRad;
      d.Art[(int)Render::ArtChannel::AileronR] = a.AileronRRad;
      d.Art[(int)Render::ArtChannel::ElevonL] = a.ElevonLRad;
      d.Art[(int)Render::ArtChannel::ElevonR] = a.ElevonRRad;
      d.Art[(int)Render::ArtChannel::Rudder] = a.RudderRad;
      d.Art[(int)Render::ArtChannel::Lef] = a.LefDeg;
      d.Art[(int)Render::ArtChannel::Speedbrake] = a.SpeedbrakeDeg;
      d.Art[(int)Render::ArtChannel::Gear] = a.GearNorm;
      d.Art[(int)Render::ArtChannel::Hook] = a.HookNorm;
      d.Art[(int)Render::ArtChannel::Canopy] = a.CanopyNorm;
      std::snprintf(d.Type, sizeof d.Type, "%s", sig.Visual.TypeName);
      UnitDraws_.push_back(d);
    }
  }
  /* AFTER the cast, because a blast outlives the round that carried it and the target that took it —
   * it is the one effect in here with no unit to hang off. */
  if (HaveSimTime_) AddBlasts();
  R->SetUnitDraws(UnitDraws_.data(), (int)UnitDraws_.size());
  R->SetSpriteDraws(SpriteDraws_.data(), (int)SpriteDraws_.size());

  /* A unit that stopped being published takes its trail with it after the grace pass — the memory is
   * only ever a memory OF a published pose, so it may not outlive the publication by more than the
   * frame it takes to notice. */
  for (size_t i = 0; i < Fx_.size();) {
    if (Fx_[i].Touch + 2 >= Pass) { i++; continue; }
    Fx_[i] = std::move(Fx_.back());
    Fx_.pop_back();
  }
}


/* The frame the procedural ground surface is measured in: the tile's z10 ancestor, so every tile
 * within ~39 km shares one origin and the surface is continuous across their seams. A float holding
 * a 39 km offset resolves 2.3 mm, which is under the finest detail octave (0.12 m). */
void World::SurfaceAnchor(int z, long x, long y, double out[3]) const {
  const int za = z < kAnchorZ ? z : kAnchorZ;
  const int sh = z - za;
  Center(za, x >> sh, y >> sh, out);
}

void World::Update(double camLat, double camLon, const double eyeEcef[3], const double fwdEcef[3],
                     double nowMs) {
  const double tUpdate = Clock();
  Pass++;
  DrawSlots.clear();
  DrawnLeaves.clear();
  WorkList.clear();
  Leaves = DrawnReady = Pending = 0;
  MeshVram = 0;
  while (camLon > 180.0) camLon -= 360.0;   /* normalize lon before tile queries (dateline) */
  while (camLon < -180.0) camLon += 360.0;
  fb_stream_campos(camLat, camLon);   /* worker pump prioritises nearest tiles */

  /* Root ring: zROOT tiles whose centre is within the view radius. */
  uint32_t rx = 0, ry = 0;
  osmmesh_geo_to_tile(camLon, camLat, (uint8_t)kRootZ, &rx, &ry);
  double span = SpanM(kRootZ);
  long Rt = (long)std::ceil(ViewM / span) + 1;
  long n = 1L << kRootZ;
  TargetTot = TargetRdy = 0;
  for (long ty = (long)ry - Rt; ty <= (long)ry + Rt; ty++)
    for (long tx = (long)rx - Rt; tx <= (long)rx + Rt; tx++) {
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) continue;
      if (Viable(kRootZ, tx, ty, eyeEcef)) {
        Descend(kRootZ, tx, ty, eyeEcef, fwdEcef);
        CountTargets(kRootZ, tx, ty, eyeEcef, TargetTot, TargetRdy);
      }
    }

  /* Budgeted, worst-first (nearest + in-frustum). The cap is in ITEMS and that is now enough, because
   * an item costs a memcpy: fetch, mesh and DAG all happen off this thread (world/TerrainLoader.h).
   * A wall-clock slice was measured HERE and rejected — it moved the cost rather than removing it,
   * p95 17.9 -> 25.8 ms with the same maximum, because the work is not divisible below one tile. */
  std::sort(WorkList.begin(), WorkList.end(), [](const Work &a, const Work &b) { return a.prio > b.prio; });
  int build = 2, upload = 6;
  MeshMs_ = AlbedoMs_ = UploadMs_ = BuildingMs_ = 0.0;
  for (const Work &w : WorkList) {
    if (build == 0 && upload == 0) break;
    Node &nd = Nodes[w.idx];
    const double tMesh = Clock();
    if (!nd.haveMesh && build > 0) {
      float *v = nullptr;
      Render::DagCluster *cl = nullptr;
      int nv = 0, ncl = 0;
      double o[3];
      float err = 0.f;
      /* The DAG arrived built (world/TerrainLoader.h): what this frame pays is the copy into the
       * node, not the simplifier. */
      if (fb_stream_build(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, kGrid, &v, &nv, &cl, &ncl, o, &err)) {
        nd.verts.assign(v, v + (size_t)nv * 8);
        nd.clusters.assign(cl, cl + ncl);
        if (getenv("FB_DAGLOG")) {
          long per[16] = {0};
          int lv = 0;
          for (const Render::DagCluster &c : nd.clusters) { per[c.Level] += c.Count / 3; if (c.Level > lv) lv = c.Level; }
          std::string t;
          for (int i = 0; i <= lv; i++) t += "L" + std::to_string(i) + "=" + std::to_string(per[i]) + " ";
          Log::Debug("world", "dag", {{"z", nd.z}, {"dagVerts", nv},
              {"clusters", ncl}, {"levels", t}});
        }
        free(v);
        free(cl);
        nd.nverts = nv;
        nd.err = err;
        nd.origin[0] = o[0]; nd.origin[1] = o[1]; nd.origin[2] = o[2];
        nd.haveMesh = 1;
        build--;
      }
    }
    MeshMs_ += Clock() - tMesh;
    const double tUp = Clock();
    if (nd.haveMesh && nd.slot < 0 && upload > 0 && R && R->DeviceUsable()) {
      double anchor[3];
      SurfaceAnchor(nd.z, nd.x, nd.y, anchor);
      nd.slot = R->UploadTile(nd.verts.data(), (uint32_t)nd.nverts, nd.clusters.data(),
                              (int)nd.clusters.size(), nd.origin, anchor);
      if (nd.slot >= 0) {
        std::vector<float>().swap(nd.verts);
        std::vector<Render::DagCluster>().swap(nd.clusters);
        nd.readyPass = Pass;   /* 2-phase: drawable next pass, once the upload is submitted */
        upload--;
        Built++;   /* build completion (thrash: builds/min ~0 in a converged loiter, climbs if evict-rebuild) */
      }
    }
    UploadMs_ += Clock() - tUp;
  }

  /* No re-emit: Descend already built DrawSlots from tiles ready THIS pass, and newly uploaded ones
   * enter next pass — one frame of latency, invisible. */
  if (R) R->SetDrawList(DrawSlots.data(), (int)DrawSlots.size());

  PublishUnits();

  /* THE GROUND CLASS. It is a property of the PLACE, so it is built here once and read by the
   * fragment and by every CPU consumer from the same bytes (world/ClassField.h). */
  Cls_.Update(camLat, camLon);
  if (R) {
    R->SetClassFrame(Cls_.EastEcef(), Cls_.NorthEcef(), Cls_.Cam());
    if (Cls_.Dirty()) { R->WriteClassBuffer(Cls_.Buffer(), Cls_.BufferBytes()); Cls_.ClearDirty(); }
  }

  /* WHERE THE STAND STANDS. The ground fragment IS the stand (render/Sward.h) and reads it off the
   * world graticule, so all the renderer needs is the place and the local basis. */
  {
    double E[3], Nn[3], U[3];
    EnuAxesEcef(camLat, camLon, E, Nn, U);
    R->SetSwardBasis(camLat, camLon, E, Nn, U);
  }

  /* OSM buildings: the 3x3 z14 block around the camera, one tile per pass, built once and kept. The
   * vector tile is the SAME source the albedo raster is baked from, so a footprint and its grey patch
   * cannot disagree about where a house is.
   *
   * THE DAG IS BUILT OVER THE NEW TILE ALONE, in the tile pool, and appended when it lands. Over the
   * whole block it is superlinear — 265 ms at 51 456 verts, 480 ms at 58 368, measured — and even one
   * dense tile is 33.0 ms of a 50.9 ms frame natively (walkbench 150 m/s). Nothing is lost by
   * splitting it: the DAG's crack-free guarantee is about SHARED EDGES, and two buildings in two
   * tiles share none. */
  const double tBld = Clock();
  if (R && BuildingDagId == 0) {
    Vectors.Build(camLat, camLon, 1);
    /* [SET] 1.5 m half width for a watercourse: the narrow end of what OSM maps as a line — a ditch
     * or a stream. The right number is per kind and lives in vegetation.json's widthM; this stands
     * until WaterField reads it. */
    if (Veg_) Water.Ingest(Vectors, *Veg_);
    if (!Water.Surfaces().empty() || !Water.Courses().empty()) {
      Water.Tessellate(Vectors, WaterVerts);
      if (R && !WaterVerts.empty())
        R->SetWaterMesh(WaterVerts.data(), (uint32_t)(WaterVerts.size() / 6), Water.Anchor());
      Log::Debug("world", "water", {{"surfaces", (int)Water.Surfaces().size()},
                                    {"courses", (int)Water.Courses().size()},
                                    {"tris", (int)(WaterVerts.size() / 18)},
                                    {"noGround", (int)Water.NoGroundCount()},
                                    {"outliers", (int)Water.OutlierCount()}});
    }
    if (Buildings.Build(Vectors) > 0 && Buildings.AddedCount() > 0) {
      BuildingVerts = (uint32_t)(Buildings.Verts().size() / 8);
      BuildingDecodeMs_ = Clock() - tBld;
      const float *newVerts = Buildings.Verts().data() + Buildings.AddedFirst();
      BuildingSoup.assign(newVerts, newVerts + Buildings.AddedCount());
      BuildingDagId = ++BuildingDagSeq;
    }
  }
  if (R && BuildingDagId != 0) {
    float *dv = nullptr;
    Render::DagCluster *dc = nullptr;
    int ndv = 0, ndc = 0;
    /* Float 6 is uv.x, and a negative one tags the roof cap: an ATTRIBUTE seam, so the cap and its
     * wall keep two vertices but collapse as one point — otherwise the ring would read as a mesh
     * boundary and nothing would move. */
    if (fb_stream_dag(BuildingDagId, BuildingSoup.data(), (int)(BuildingSoup.size() / 8), 6,
                      &dv, &ndv, &dc, &ndc)) {
      const uint32_t base = (uint32_t)(BuildingDagVerts.size() / 8);
      BuildingDagVerts.insert(BuildingDagVerts.end(), dv, dv + (size_t)ndv * 8);
      for (int i = 0; i < ndc; i++) {
        dc[i].First += base;
        BuildingClusters.push_back(dc[i]);
      }
      free(dv);
      free(dc);
      BuildingDagId = 0;
      std::vector<float>().swap(BuildingSoup);
      if (getenv("FB_DAGLOG")) {
        long per[16] = {0};
        int lv = 0;
        for (const Render::DagCluster &c : BuildingClusters) { per[c.Level] += c.Count / 3; if (c.Level > lv) lv = c.Level; }
        float emin[16], emax[16];
        for (int i = 0; i < 16; i++) { emin[i] = 1e30f; emax[i] = 0.0f; }
        for (const Render::DagCluster &c : BuildingClusters) {
          if (c.SelfErr < emin[c.Level]) emin[c.Level] = c.SelfErr;
          if (c.SelfErr > emax[c.Level]) emax[c.Level] = c.SelfErr;
        }
        std::string t;
        for (int i = 0; i <= lv; i++)
          t += "L" + std::to_string(i) + "=" + std::to_string(per[i]) + "tris/err" +
               std::to_string(emin[i]) + ".." + std::to_string(emax[i]) + "m ";
        Log::Debug("world", "dag_buildings", {{"soupVerts", (int)BuildingVerts},
            {"dagVerts", (int)BuildingDagVerts.size() / 8}, {"clusters", (int)BuildingClusters.size()},
            {"levels", t}});
      }
      R->SetBuildingMesh(BuildingDagVerts.data(), (uint32_t)(BuildingDagVerts.size() / 8),
                         BuildingClusters.data(), (int)BuildingClusters.size(), Buildings.Anchor());
    }
  }
  BuildingMs_ = Clock() - tBld;

  /* LOWEST priority, after mesh/albedo/overlay. Nearest-first: DrawnLeaves is in descent order. */
  if (NightLights && R) {
    int fetch = 3;   /* per-pass decode budget */
    for (int idx : DrawnLeaves) {
      if (fetch == 0) break;
      if (Nodes[idx].lightState == 0) { BuildLights(idx); fetch--; }
    }
    LightBuf.clear();
    for (int idx : DrawnLeaves) {
      const Node &nd = Nodes[idx];
      if (nd.lightState != 1 || nd.lightInst.empty()) continue;
      if ((int)(LightBuf.size() / 7) + (int)(nd.lightInst.size() / 7) > kLightBudget) break;
      LightBuf.insert(LightBuf.end(), nd.lightInst.begin(), nd.lightInst.end());
    }
    LightsResident = (int)(LightBuf.size() / 7);
    R->SetLights(LightBuf.data(), LightsResident);
  } else if (R && LightsResident > 0) {
    LightsResident = 0;
    R->SetLights(nullptr, 0);   /* left night / disabled -> drop the field */
  }

  /* Grace-period eviction; swap-pop, so gIndex must stay in sync. */
  for (size_t i = 0; i < Nodes.size();) {
    Node &nd = Nodes[i];
    if (nd.touch == Pass) { nd.stale = 0; i++; continue; }
    if (++nd.stale <= kGrace) { i++; continue; }
    if (nd.slot >= 0 && R) R->ReleaseTile(nd.slot);
    gIndex.erase(Key(nd.z, nd.x, nd.y));
    size_t last = Nodes.size() - 1;
    if (i != last) {
      Nodes[i] = std::move(Nodes[last]);
      gIndex[Key(Nodes[i].z, Nodes[i].x, Nodes[i].y)] = (int)i;
    }
    Nodes.pop_back();
    Evicted++;
  }

  if (nowMs - LastLog >= 1000.0) {
    double dtMin = (nowMs - LastLog) / 60000.0;   /* interval in minutes for the per-minute rates */
    LastLog = nowMs;
    long albVram = (long)DrawnReady * TS * TS * 4;
    double vramMB = (double)(MeshVram + albVram) / 1.0e6;
    double buildsMin = dtMin > 0 ? (Built - PrevBuilt) / dtMin : 0.0;      /* STREAMING-THRASH probe: in a
                                    converged stationary loiter both rates -> ~0; steady climb = evict-rebuild churn */
    double evictMin = dtMin > 0 ? (Evicted - PrevEvicted) / dtMin : 0.0;
    PrevBuilt = Built; PrevEvicted = Evicted;
    Log::Debug("world", "fbworld", {{"leaves", Leaves}, {"drawn", DrawnReady}, {"pending", Pending},
                                      {"evicted", (int)Evicted}, {"vramMB", vramMB}, {"nodes", (int)Nodes.size()},
                                      {"lights", LightsResident}, {"buildsPerMin", buildsMin},
                                      {"evictPerMin", evictMin}, {"built", (int)Built}});
  }
  UpdateMs_ = Clock() - tUpdate;
}

} // namespace outshine::World
