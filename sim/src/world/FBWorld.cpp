#include "FBWorld.h"
#include "FBRenderer.h"
#include "FBTerrainLoader.h"
#include "FBMips.h"         /* Render::fb_pyramid_bytes — the albedo scratch now holds a whole mip pyramid */
#include "FBCamera.h"
#include "FBGeodesy.h"
#include "FBLog.h"
#include "FBStore.h"
#include "FBUnit.h"
#include "FBUnitRegistry.h"
#include "FBUnits.h"
#include "geo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace FlightBox::World {

static const int kRootZ = 8;
static const int kMaxZ = 14;
static const int kGrace = 180;                 /* passes unasked before eviction (lru.h hysteresis) */
static const double kEarthCirc = 40075016.686;
/* LOD is PURELY distance-based: height variance is deliberately NOT in the decision, so a flat near
 * tile refines as far as a rugged one at the same distance (equal albedo resolution by distance).
 * kSseK = H / (2 tan(fov/2)) is the pixel focal length.
 * Herleitung + Konstantentabelle: doc/world/terrain.md, Abschnitt 2.2. */
static const double kSseK = 720.0 / (2.0 * 0.57735026919);   /* H / (2 tan(fov/2)) */
static const double kEdgeTau = 384.0;   /* target max on-screen tile edge (px); lower = finer = more tiles */
static const double kCosView = 0.5;            /* frustum weight: <60deg off-axis -> full priority */
static const int kNodeCeil = 6000;             /* safety backstop on the working set */

/* Packed (z,x,y) node key: z<16, x/y<2^30 (z<=29). */
static inline uint64_t Key(int z, long x, long y) {
  return ((uint64_t)(z & 0xF) << 60) | ((uint64_t)(x & 0x3FFFFFFF) << 30) | (uint64_t)(y & 0x3FFFFFFF);
}
static std::unordered_map<uint64_t, int> gIndex;

FBWorld::FBWorld()
  : R(nullptr), Photo(false), DefaultPhoto(false), Grid(32), TS(512), ViewM(6000.0), Lat0(0), Lon0(0),
    Pass(0), Evicted(0), LastLog(0), Leaves(0), DrawnReady(0), Pending(0), TargetTot(0), TargetRdy(0), MeshVram(0),
    NightLights(false), Anchor{0, 0, 0}, LightsResident(0) {}

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

bool FBWorld::Open(Render::FBRenderer *renderer, const char *tilesBase, double lat, double lon, int grid,
                   double viewMeters, int albedoTS) {
  R = renderer;
  Grid = grid;
  TS = albedoTS;
  ViewM = viewMeters;
  Lat0 = lat;
  Lon0 = lon;
  Scratch.resize((size_t)Render::fb_pyramid_bytes(TS));   /* holds a whole finished mip pyramid, not one level */
  gIndex.clear();
  if (fb_stream_open(tilesBase, lat, lon, kRootZ) == 0) return false;
  fb_stream_set_base(DefaultPhoto ? 1 : 0);        /* worker priority: base tiles before the overlay */
  /* Sprite positions are stored relative to this anchor, so the renderer holds float offsets. */
  osmmesh_geo g0{}; g0.lat = lat; g0.lon = lon; g0.alt = 0.0;
  osmmesh_ecef a0 = osmmesh_geo_to_ecef(g0);
  Anchor[0] = a0.x; Anchor[1] = a0.y; Anchor[2] = a0.z;
  if (R) R->SetLightAnchor(Anchor);
  return true;
}

double FBWorld::SpanM(int z) const {
  return kEarthCirc * std::cos(Lat0 * kPi / 180.0) / (double)(1L << z);
}

void FBWorld::Center(int z, long x, long y, double out[3]) const {
  osmmesh_geo g = osmmesh_tile_frac_to_geo((uint8_t)z, (uint32_t)x, (uint32_t)y, 0.5, 0.5);
  g.alt = 0.0;
  osmmesh_ecef e = osmmesh_geo_to_ecef(g);
  out[0] = e.x; out[1] = e.y; out[2] = e.z;
}

static double Dist(const double a[3], const double b[3]) {
  double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

int FBWorld::Ensure(int z, long x, long y) {
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
bool FBWorld::Viable(int z, long x, long y, const double eye[3]) const {
  long n = 1L << z;
  if (x < 0 || y < 0 || x >= n || y >= n) return false;
  double c[3];
  Center(z, x, y, c);
  double dist = Dist(c, eye);
  double span = SpanM(z);
  return dist - span * 0.71 <= ViewM;
}

void FBWorld::AddWork(int idx, int z, long x, long y, const double eye[3], const double fwd[3]) {
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

void FBWorld::Emit(int idx) {
  Leaves++;
  Node &n = Nodes[idx];
  if (Ready(n)) {   /* committed on the GPU (uploaded + one pass elapsed) — safe to draw */
    n.emitPass = Pass;   /* drawn this pass -> a mode switch may keep showing it (old mode) until the overlay lands */
    DrawnReady++;
    MeshVram += (long)n.nverts * 32;
    DrawSlots.push_back(n.slot);
    if (NightLights) DrawnLeaves.push_back(idx);   /* light hosts = the drawn LOD cut (near z14, far glow) */
  } else {
    Pending++;
  }
}

/* lightState 1 = decoded (the buffer may be empty for a dark tile), -1 = pending. */
void FBWorld::BuildLights(int idx) {
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
bool FBWorld::WantSplit(int z, long x, long y, const double eye[3]) const {
  if (z >= kMaxZ || (int)Nodes.size() >= kNodeCeil) return false;
  double c[3]; Center(z, x, y, c);
  double dist = Dist(c, eye); if (dist < 1.0) dist = 1.0;
  return SpanM(z) * kSseK / dist > kEdgeTau;
}

int FBWorld::Find(int z, long x, long y) const {
  auto it = gIndex.find(Key(z, x, y));
  return it == gIndex.end() ? -1 : it->second;
}

/* Can this subtree cover its area with tiles READY this pass? */
bool FBWorld::CanCover(int z, long x, long y, const double eye[3]) const {
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
  return idx >= 0 && CoversInMode(Nodes[idx]);   /* mode-aware: right mode ready, or already-drawn (keep old at TAB) */
}

/* Intermediate nodes are only touched (kept alive as refine-holds); only target leaves get AddWork'd. */
void FBWorld::RequestSubtree(int z, long x, long y, const double eye[3], const double fwd[3]) {
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
int FBWorld::Descend(int z, long x, long y, const double eye[3], const double fwd[3]) {
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
  return CoversInMode(Nodes[idx]) ? 1 : 0;
}

/* Side-effect-free walk of the target cut: the LoadProgress that gates the loading screen. */
void FBWorld::CountTargets(int z, long x, long y, const double eye[3], int &total, int &ready) const {
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
 * history from core/FBCountermeasure.h's own curve, the nozzle from the mesh.
 * Sizes are metres and brightnesses are radiance: doc/render/units-visual.md §9. */
static const float kFlamePlumeLenPerRadius = 11.0f;    /* [SET] visible AB plume vs nozzle exit radius */
static const float kFlameWidthPerRadius = 0.95f;      /* [SET] the efflux necks slightly at the petals */
static const float kFlameGain = 1.0f;                 /* [SET] scale on the shader's own flame radiance ramp */
static const float kMotorHalfLenM = 2.5f;             /* [SET] a boosting rocket's visible flame */
static const float kMotorRadiusM = 0.55f;             /* [SET] */
static const float kMotorGain = 1.7f;                 /* [SET] a solid motor outshines a turbine's plume */
static const double kSmokeLifeS = 20.0;               /* [SET] a trail segment from lay to gone */
static const double kTrailStepM = 250.0;              /* [SET] crumb spacing: kMaxCrumbs of these span 24 km */
static const size_t kMaxCrumbs = 96;                  /* [SET] bounded memory per burning round */
static const float kSmokeR0M = 1.0f, kSmokeRMaxM = 16.0f;   /* [SET] dispersion of a trail segment */
static const float kSmokeAlpha0 = 0.80f;              /* [SET] freshly laid opacity */
static const float kSmokeRadiance = 1.2f;             /* [SET] sunlit white smoke, the terrain's own level */
static const float kFlareRadiusM = 4.5f;              /* [SET] the LUMINOUS ball: the grain is centimetres, what an eye sees is the glow around it */
static const float kFlareGain = 1.0f;                 /* [SET] scale on the shader's core+halo radiance */
static const float kChaffR0M = 0.6f, kChaffRMaxM = 18.0f;   /* [SET] a dipole cloud dispersing */
static const float kChaffAlpha = 0.45f;               /* [SET] peak opacity of the bloomed bundle */
static const size_t kMaxSpriteDraws = 512;            /* mirrors FBSpritesStage's own instance cap */

FBWorld::UnitFx &FBWorld::Fx(int id) {
  for (UnitFx &f : Fx_)
    if (f.Id == id) { f.Touch = Pass; return f; }
  UnitFx f;
  f.Id = id;
  f.Touch = Pass;
  f.FirstSeenS = SimTimeS_;
  Fx_.push_back(f);
  return Fx_.back();
}

/* WHAT THIS UNIT IS DOING THAT AN EYE WOULD SEE, and every input is something the unit itself
 * published. Nothing here can write back: an FBSpriteDraw carries no simulation type, exactly like the
 * FBUnitDraw beside it. */
void FBWorld::AddUnitEffects(const Units::FBUnit &u, const Units::FBUnitSignature &sig, const double ecef[3],
                             const double fwd[3], const double right[3], const double up[3], bool isEye) {
  if (SpriteDraws_.size() >= kMaxSpriteDraws) return;

  /* 1 — THE NOZZLE. The bit is `Afterburner`, published off the ENGINE (units/FBSimUnit), and it is
   * the whole visible difference: an F-16 at military power shows no flame at all, in augmentation it
   * shows several metres of one. Nothing finer is published, so nothing finer is drawn. Never for the
   * eye unit: its own nozzle is six metres behind the camera. */
  float nzOff[3], nzRad = 0.0f;
  if (!isEye && sig.Afterburner && sig.Visual.TypeName[0] && R &&
      R->UnitNozzle(sig.Visual.TypeName, nzOff, nzRad) && nzRad > 0.0f) {
    /* Model space is glTF (+X right, +Y up, +Z aft), so the exhaust axis is -fwd and the offset rides
     * the very basis the airframe itself is drawn with — FBCameraBasisEcef's, borrowed, not rebuilt. */
    const float halfLen = 0.5f * kFlamePlumeLenPerRadius * nzRad;
    Render::FBSpriteDraw s;
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
    s.Kind = (uint32_t)Render::FBSpriteKind::Flame;
    SpriteDraws_.push_back(s);
  }

  if (!HaveSimTime_) return;   /* no clock, no age — and every effect below is an age */

  /* 2 — THE ROUND. A weapon unit's motor burns for exactly as long as the CATALOGUE says its type
   * burns (core/FBStore.h Perf.BoostS + SustainS), counted from the frame the round first appeared —
   * which is the frame it was created in, because a released store becomes a unit at launch. */
  if (u.GetKind() == Units::FBUnitKind::Weapon) {
    double motorS = 0.0;
    if (const FBStoreSpec *spec = FBFindStore(sig.Visual.TypeName))
      motorS = spec->Perf.BoostS + spec->Perf.SustainS;
    AddSmokeTrail(u, ecef, motorS);
    const UnitFx &fx = Fx(u.GetId());
    if (motorS > 0.0 && SimTimeS_ - fx.FirstSeenS < motorS && SpriteDraws_.size() < kMaxSpriteDraws) {
      /* No round publishes a LENGTH (a missile module declares no damage layout), so the flame hangs
       * at the unit's own origin rather than at an invented tail station — below its own radius at any
       * range a round is seen from. */
      Render::FBSpriteDraw s;
      for (int i = 0; i < 3; i++) {
        s.Ecef[i] = ecef[i] - fwd[i] * (double)kMotorHalfLenM;
        s.Axis[i] = -(float)fwd[i];
      }
      s.HalfLenM = kMotorHalfLenM;
      s.RadiusM = kMotorRadiusM;
      s.Color[0] = kMotorGain; s.Color[1] = kMotorGain * 1.15f; s.Color[2] = kMotorGain * 1.4f;   /* a rocket burns whiter than a turbine */
      s.Param = 0.5f;
      s.Kind = (uint32_t)Render::FBSpriteKind::Flame;
      SpriteDraws_.push_back(s);
    }
  }

  /* 3 — THE EXPENDABLES. Position and bloom time are published per cartridge; the AGE CURVES are the
   * simulation's own free functions, not a second opinion — a flare's radiometric history is the same
   * combustion whether a seeker or an eye is looking at it, and a chaff bundle is not a cloud before
   * it blooms for the eye either. */
  for (int i = 0; i < kMaxFlareClouds && SpriteDraws_.size() < kMaxSpriteDraws; i++) {
    const FBFlareCloud &f = sig.Flare[i];
    if (!f.Active) continue;
    const double n = FBFlareIrNorm(SimTimeS_ - f.BloomS);
    if (n <= 0.0) continue;
    Render::FBSpriteDraw s;
    FBGeoToEcef(f.LatDeg, f.LonDeg, f.AltM, s.Ecef);
    s.HalfLenM = s.RadiusM = kFlareRadiusM;
    s.Color[0] = s.Color[1] = s.Color[2] = kFlareGain * (float)n;
    s.Kind = (uint32_t)Render::FBSpriteKind::Flare;
    SpriteDraws_.push_back(s);
  }
  for (int i = 0; i < kMaxChaffClouds && SpriteDraws_.size() < kMaxSpriteDraws; i++) {
    const FBChaffCloud &c = sig.Chaff[i];
    if (!c.Active) continue;
    const double age = SimTimeS_ - c.BloomS;
    const double n = FBChaffRcsNorm(age);
    if (n <= 0.0) continue;
    const float grow = (float)std::sqrt(age / kChaffLifeS);
    Render::FBSpriteDraw s;
    FBGeoToEcef(c.LatDeg, c.LonDeg, c.AltM, s.Ecef);
    s.HalfLenM = s.RadiusM = kChaffR0M + (kChaffRMaxM - kChaffR0M) * grow;
    s.Alpha = kChaffAlpha * (float)n;
    for (int k = 0; k < 3; k++) s.Color[k] = kSmokeRadiance * s.Alpha;   /* premultiplied */
    s.Param = 0.6f;
    s.Kind = (uint32_t)Render::FBSpriteKind::Smoke;
    SpriteDraws_.push_back(s);
  }
}

/* THE TRAIL, and it is a memory of PUBLISHED POSES: a crumb is laid where the round was seen while its
 * motor was burning, and the segment between two crumbs is drawn until it has dispersed. The renderer
 * integrates nothing — without the crumbs a pose alone cannot say where the round has been. */
void FBWorld::AddSmokeTrail(const Units::FBUnit &u, const double ecef[3], double motorS) {
  UnitFx &fx = Fx(u.GetId());
  const bool burning = motorS > 0.0 && SimTimeS_ - fx.FirstSeenS < motorS;
  if (burning) {
    bool lay = fx.Trail.empty();
    if (!lay) {
      const FxCrumb &last = fx.Trail.back();
      double d2 = 0.0;
      for (int i = 0; i < 3; i++) { const double dx = ecef[i] - last.Ecef[i]; d2 += dx * dx; }
      lay = d2 > kTrailStepM * kTrailStepM;
    }
    if (lay) {
      FxCrumb c;
      for (int i = 0; i < 3; i++) c.Ecef[i] = ecef[i];
      c.T = SimTimeS_;
      fx.Trail.push_back(c);
      if (fx.Trail.size() > kMaxCrumbs) fx.Trail.erase(fx.Trail.begin());
    }
  }
  while (!fx.Trail.empty() && SimTimeS_ - fx.Trail.front().T > kSmokeLifeS) fx.Trail.erase(fx.Trail.begin());
  if (fx.Trail.size() < 2) return;

  /* The head of the trail follows the round itself while the motor burns, so the plume stays attached
   * instead of ending one crumb behind. */
  for (size_t i = 0; i + 1 < fx.Trail.size() + (burning ? 1 : 0); i++) {
    if (SpriteDraws_.size() >= kMaxSpriteDraws) return;
    const FxCrumb &a = fx.Trail[i];
    const double *b = (i + 1 < fx.Trail.size()) ? fx.Trail[i + 1].Ecef : ecef;
    double seg[3];
    double len2 = 0.0;
    for (int k = 0; k < 3; k++) { seg[k] = b[k] - a.Ecef[k]; len2 += seg[k] * seg[k]; }
    const double len = std::sqrt(len2);
    if (len < 1.0) continue;
    const double age = SimTimeS_ - a.T;
    const double life = age / kSmokeLifeS;
    if (life >= 1.0) continue;
    Render::FBSpriteDraw s;
    for (int k = 0; k < 3; k++) {
      s.Ecef[k] = a.Ecef[k] + 0.5 * seg[k];
      s.Axis[k] = (float)(seg[k] / len);
    }
    s.RadiusM = kSmokeR0M + (kSmokeRMaxM - kSmokeR0M) * (float)std::sqrt(life);
    /* Half a radius of overlap at each end: neighbouring segments then BUTT rather than bead, which is
     * what the first trail frame showed at every join (measured: a 5 px notch at 2.5 km). */
    s.HalfLenM = (float)(0.5 * len) + 0.5f * s.RadiusM;
    s.Alpha = kSmokeAlpha0 * (float)((1.0 - life) * (1.0 - life));
    for (int k = 0; k < 3; k++) s.Color[k] = kSmokeRadiance * s.Alpha;   /* premultiplied */
    s.Param = 0.35f;
    s.Kind = (uint32_t)Render::FBSpriteKind::Smoke;
    SpriteDraws_.push_back(s);
  }
}

/* THE ONE PLACE the picture reads the cast, and it reads nothing but the PUBLISHED pose — the barrier
 * units/FBSimUnit::PublishPose sets, never a foreign FDM. Nothing is written back: the record is a
 * value copy, and FBUnitDraw carries no simulation type at all, so there is no handle to write through.
 * doc/render/units-visual.md, Spec. */
void FBWorld::PublishUnits() {
  UnitDraws_.clear();
  SpriteDraws_.clear();
  if (!R) return;
  if (Units_) {
    for (const Units::FBUnit *u : Units_->Units()) {
      if (!u) continue;
      const Units::FBUnitSignature sig = u->GetSignature();
      const Units::FBUnitPose p = u->GetPose();
      double ecef[3];
      FBGeoToEcef(p.LatDeg, p.LonDeg, p.ElevM, ecef);
      /* The airframe's own axes in ECEF, from the same function the camera uses — so a jet drawn at a
       * pose and a camera placed at that pose cannot disagree. glTF is +X right, +Y up, -Z forward. */
      double fwd[3], right[3], up[3];
      FBCameraBasisEcef(p.YawDeg, p.PitchDeg, p.RollDeg, p.LatDeg, p.LonDeg, fwd, right, up);
      const bool isEye = u->GetId() == EyeUnitId_;
      /* THE EFFECTS COME FIRST and they are drawn for the EYE UNIT TOO: its own flares fall behind it
       * and are in the picture the moment it turns, while its airframe (below) is the one thing the
       * camera may not draw, because the eye sits inside it. */
      AddUnitEffects(*u, sig, ecef, fwd, right, up, isEye);
      if (isEye) continue;
      if (!sig.Visual.TypeName[0]) continue;   /* nothing published to look at: no model, no draw */
      Render::FBUnitDraw d;
      for (int i = 0; i < 3; i++) {
        d.Ecef[i] = ecef[i];
        d.Rot[0 * 3 + i] = (float)right[i];
        d.Rot[1 * 3 + i] = (float)up[i];
        d.Rot[2 * 3 + i] = -(float)fwd[i];
      }
      const Units::FBUnitArticulation &a = p.Art;
      d.Art[(int)Render::FBArtChannel::AileronL] = a.AileronLRad;
      d.Art[(int)Render::FBArtChannel::AileronR] = a.AileronRRad;
      d.Art[(int)Render::FBArtChannel::ElevonL] = a.ElevonLRad;
      d.Art[(int)Render::FBArtChannel::ElevonR] = a.ElevonRRad;
      d.Art[(int)Render::FBArtChannel::Rudder] = a.RudderRad;
      d.Art[(int)Render::FBArtChannel::Lef] = a.LefDeg;
      d.Art[(int)Render::FBArtChannel::Speedbrake] = a.SpeedbrakeDeg;
      d.Art[(int)Render::FBArtChannel::Gear] = a.GearNorm;
      d.Art[(int)Render::FBArtChannel::Hook] = a.HookNorm;
      d.Art[(int)Render::FBArtChannel::Canopy] = a.CanopyNorm;
      std::snprintf(d.Type, sizeof d.Type, "%s", sig.Visual.TypeName);
      UnitDraws_.push_back(d);
    }
  }
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

void FBWorld::Update(double camLat, double camLon, const double eyeEcef[3], const double fwdEcef[3],
                     double nowMs) {
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

  /* Budgeted, worst-first (nearest + in-frustum), to bound per-frame cost. */
  std::sort(WorkList.begin(), WorkList.end(), [](const Work &a, const Work &b) { return a.prio > b.prio; });
  int build = 2, albedo = 2, upload = 6;
  for (const Work &w : WorkList) {
    if (build == 0 && albedo == 0 && upload == 0) break;
    Node &nd = Nodes[w.idx];
    if (!nd.haveMesh && build > 0) {
      float *v = nullptr;
      int nv = 0;
      double o[3];
      float err = 0.f;
      if (fb_stream_build(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, Grid, &v, &nv, o, &err)) {
        nd.verts = v; nd.nverts = nv; nd.err = err;
        nd.origin[0] = o[0]; nd.origin[1] = o[1]; nd.origin[2] = o[2];
        nd.haveMesh = 1;
        build--;
      }
    }
    if (!nd.haveAlbedo && albedo > 0) {
      /* A photo base with no Esri coverage (204) falls back to OSM, so every tile can draw a base. */
      int baseMode = DefaultPhoto ? 1 : 0;
      int r = fb_stream_pyramid(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, baseMode, TS, Scratch.data());
      if (r < 0 && baseMode == 1)   /* photo hole -> OSM base */
        r = fb_stream_pyramid(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, 0, TS, Scratch.data());
      if (r > 0) {
        nd.albedo.assign(Scratch.begin(), Scratch.begin() + r);
        nd.haveAlbedo = 1;
        albedo--;
      }
    }
    if (nd.haveMesh && nd.haveAlbedo && nd.slot < 0 && upload > 0 && R && R->DeviceUsable()) {
      nd.slot = R->UploadTile(nd.verts, (uint32_t)nd.nverts, nd.origin, nd.albedo.data(), TS, nd.z);
      if (nd.slot >= 0) {
        free(nd.verts);
        nd.verts = nullptr;
        nd.albedo.clear();
        nd.readyPass = Pass;   /* 2-phase: drawable next pass, once the upload is submitted */
        upload--;
        Built++;   /* build completion (thrash: builds/min ~0 in a converged loiter, climbs if evict-rebuild) */
      }
    }
  }

  /* Symmetric: whichever mode is not the eager base is fetched lazily for on-screen tiles. A 204
   * marks the node -1 — give up, the tile keeps drawing its base. */
  if (Photo != DefaultPhoto && R && R->DeviceUsable()) {
    int altBudget = 2;
    int altMode = Photo ? 1 : 0;   /* the non-base mode: base photo -> osm overlay, base osm -> photo */
    for (Node &nd : Nodes) {
      if (altBudget == 0) break;
      if (nd.touch != Pass || nd.slot < 0 || nd.alt != 0) continue;
      int r = fb_stream_pyramid(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, altMode, TS, Scratch.data());
      if (r > 0) {
        nd.alt = R->UploadTilePhoto(nd.slot, Scratch.data(), TS, nd.z) ? 1 : -1;
        nd.altPass = Pass;   /* 2-phase on the overlay axis: drawable a later pass */
        altBudget--;
      } else if (r < 0) {
        nd.alt = -1;   /* no overlay on the server here — keep the base for good */
      }
    }
  }

  /* No re-emit: Descend already built DrawSlots from tiles ready THIS pass, and newly uploaded ones
   * enter next pass — one frame of latency, invisible. */
  if (R) R->SetDrawList(DrawSlots.data(), (int)DrawSlots.size());

  PublishUnits();

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
    free(nd.verts);
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
    FBLog::Debug("world", "fbworld", {{"leaves", Leaves}, {"drawn", DrawnReady}, {"pending", Pending},
                                      {"evicted", (int)Evicted}, {"vramMB", vramMB}, {"nodes", (int)Nodes.size()},
                                      {"lights", LightsResident}, {"buildsPerMin", buildsMin},
                                      {"evictPerMin", evictMin}, {"built", (int)Built}});
  }
}

} // namespace FlightBox::World
