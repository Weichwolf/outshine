#include "FBWorld.h"
#include "FBRenderer.h"
#include "fb_terrain.h"
#include "fb_mips.h"        /* fb_pyramid_bytes — the albedo scratch now holds a whole mip pyramid */
#include "geo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace FlightBox {

static const int kRootZ = 8;
static const int kMaxZ = 14;
static const int kGrace = 180;                 /* passes unasked before eviction (lru.h hysteresis) */
static const double kEarthCirc = 40075016.686;
static const double kPi = 3.14159265358979323846;
/* LOD is PURELY DISTANCE-BASED (2026-07-23, user directive): split when a tile's projected EDGE LENGTH
 * (ground span / distance, FOV-normalised to a 720-px/60deg viewport) exceeds kEdgeTau pixels. Height
 * variance is deliberately NOT in the decision — a flat near tile must refine to the same level a rugged
 * one would at the same distance (equal albedo resolution by distance). kSseK = H / (2 tan(fov/2)) is the
 * pixel focal length; a tile of span S at distance d projects to S*kSseK/d px. Leaf tiles land between
 * kEdgeTau/2 and kEdgeTau px on screen, so near=fine, far=coarse, monotonic in distance. (This also
 * unsticks the old flat-terrain leaves=2-3 case, where err~0 refused every split.) */
static const double kSseK = 720.0 / (2.0 * 0.57735026919);   /* H / (2 tan(fov/2)) */
static const double kEdgeTau = 384.0;   /* target max on-screen tile edge (px); lower = finer = more tiles */
static const double kCosView = 0.5;            /* frustum weight: <60deg off-axis -> full priority */
static const int kNodeCeil = 6000;             /* safety backstop on the working set */

/* Packed (z,x,y) key for O(1) node lookup. z<16, x/y<2^30 (z<=29). */
static inline uint64_t Key(int z, long x, long y) {
  return ((uint64_t)(z & 0xF) << 60) | ((uint64_t)(x & 0x3FFFFFFF) << 30) | (uint64_t)(y & 0x3FFFFFFF);
}
static std::unordered_map<uint64_t, int> gIndex;

FBWorld::FBWorld()
  : R(nullptr), Photo(false), DefaultPhoto(false), Grid(32), TS(512), ViewM(6000.0), Lat0(0), Lon0(0),
    Pass(0), Evicted(0), LastLog(0), Leaves(0), DrawnReady(0), Pending(0), TargetTot(0), TargetRdy(0), MeshVram(0),
    NightLights(false), Anchor{0, 0, 0}, LightsResident(0) {}

/* Night-light appearance (cosmetic LUT, like the OSM style palette): per-class colour, world radius
 * (m) and a brightness weight; the shader gets colour * (intensity/255) * brightness, additive. */
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

bool FBWorld::Open(FBRenderer *renderer, const char *tilesBase, double lat, double lon, int grid,
                   double viewMeters, int albedoTS) {
  R = renderer;
  Grid = grid;
  TS = albedoTS;
  ViewM = viewMeters;
  Lat0 = lat;
  Lon0 = lon;
  Scratch.resize((size_t)fb_pyramid_bytes(TS));   /* holds a whole finished mip pyramid, not one level */
  gIndex.clear();
  if (fb_stream_open(tilesBase, lat, lon, kRootZ) == 0) return false;
  fb_stream_set_base(DefaultPhoto ? 1 : 0);        /* worker priority: base tiles before the overlay */
  /* Light-position anchor = the field origin ECEF (alt 0). Sprite positions are stored relative to it
   * so the renderer holds float offsets (< view radius) and subtracts (eye - anchor) per frame. */
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

/* Pure viability — map bounds AND within the view radius. A child that fails EITHER only prevents the
 * parent's split (the parent stays a drawn leaf); it never voids coverage. No side effects. */
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

/* Decode /t/lights for one drawn leaf into node.lightInst: each record's tile-local (x,y) -> geo ->
 * ECEF at the tile-centre ground height + a small lift, minus the anchor. Class -> colour/radius LUT,
 * intensity -> brightness. lightState: 1 = decoded (buffer may be empty for a dark tile), -1 pending. */
void FBWorld::BuildLights(int idx) {
  Node &n = Nodes[idx];
  if (!n.haveMesh) { n.lightState = -1; return; }   /* need origin height first; retry a later pass */
  if ((int)LightBytes.size() < 65540) LightBytes.resize(65540);
  int nb = fb_stream_lights(n.z, (uint32_t)n.x, (uint32_t)n.y, LightBytes.data(), (int)LightBytes.size());
  if (nb < 4) { n.lightState = -1; return; }         /* pending (0) or too big — retry next pass */
  const uint8_t *p = LightBytes.data();
  int count = (int)p[0] | ((int)p[1] << 8);
  n.lightInst.clear();
  /* Tile-centre ground ASL from the mesh origin (ECEF -> geodetic alt). One height per tile is plenty
   * for point lights viewed from altitude (a z14 tile is < 2 km across). */
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

/* Geometry-only refine test: does (z,x,y) project larger than kEdgeTau? Pure function of camera + tile
 * geometry (distance-LOD) — so the TARGET cut is known instantly, without any tile data. That is what
 * lets the boot/teleport request go straight to the final leaves (no LOD ladder / rebuild churn) and
 * removes the old all-four-children-in-view gate (each viable child refines independently → the
 * leaves=2-3 stall is gone; out-of-ViewM quadrants are simply not covered, by design). */
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

/* Pure: can this subtree cover its ground area with tiles READY this pass? True if every viable child
 * can cover (refined), OR this node itself is resident (a coarser tile holds the whole area). */
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

/* Cascade the REQUEST to the target leaves — geometry picks the depth, no data needed. Intermediate
 * nodes are only touched (kept alive as refine-holds); only the final target leaves get AddWork'd. */
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

/* Direct-to-target draw traversal (returns 1 = this node's area is fully covered by emitted tiles). At
 * a split node: if all viable children can cover -> draw the refined level. Else HOLD with this node if
 * resident (the previous, coarser target leaf) while the deeper targets stream in — cascading the
 * request meanwhile. If self is NOT resident (boot/teleport), emit whatever ready descendants exist
 * (partial); the remaining hole is covered by a resident ancestor higher up, or by the LOADING SCREEN.
 * No intermediate LOD levels are ever built — only the geometry-target leaves. */
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
      /* self not resident (boot/teleport): emit ready descendants, rest is a hole (loading screen) */
      for (int q = 0; q < 4; q++) {
        long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
        if (Viable(z + 1, cx, cy, eye)) Descend(z + 1, cx, cy, eye, fwd);
      }
      return 0;
    }
  }
  /* target leaf (or a tile with no viable children — fully beyond ViewM): request + emit. Covered (in
   * mode) only if ReadyMode — a base-resident but overlay-pending leaf still emits (never a hole/blank;
   * FBRenderer holds the old mode) but reports NOT mode-covered so an ancestor keeps holding. */
  if (!Uploaded(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);
  Emit(idx);
  return CoversInMode(Nodes[idx]) ? 1 : 0;
}

/* Walk the geometry target cut (no side effects), counting total leaves and GPU-ready ones — the
 * boot/teleport LoadProgress that gates the loading screen + sim start. */
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

  /* Root ring: zROOT tiles whose centre is within the view radius of the camera. */
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

  /* Budgeted work, worst-first (nearest + in-frustum). lru.h bake-budget: bound per-frame cost. */
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
      /* Base albedo pyramid = the boot-default source (eager). A photo base with no Esri coverage (a
       * 204 hole) falls back to OSM so the tile can always draw its base. `r` = pyramid bytes. */
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

  /* Lazy OVERLAY: when the viewed mode differs from the eager base, fetch the OTHER mode's albedo for
   * on-screen tiles (budgeted), then cache it. Symmetric — base OSM -> photo overlay, base photo ->
   * OSM overlay. A server 204 marks the node -1 (give up; the tile keeps drawing its base). */
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

  /* Re-emit is not needed: Descend already built DrawSlots from tiles that were ready THIS pass. Newly
   * uploaded tiles enter the draw list next pass — one frame's latency, invisible. */
  if (R) R->SetDrawList(DrawSlots.data(), (int)DrawSlots.size());

  /* Night lights (LOWEST priority — after terrain mesh/albedo/overlay): stream a few undecoded drawn
   * leaves per pass, then hand the renderer the concatenated sprites of every decoded drawn leaf,
   * capped at kLightBudget (nearest-first: DrawnLeaves is in descent order, coarse ring outward). */
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

  /* Grace-period eviction (swap-pop; keep gIndex in sync). */
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
    printf("[fbworld] leaves=%d drawn=%d pending=%d evicted=%ld vramMB=%.1f nodes=%d lights=%d | builds/min=%.0f evict/min=%.0f (built=%ld)\n",
           Leaves, DrawnReady, Pending, Evicted, vramMB, (int)Nodes.size(), LightsResident, buildsMin, evictMin, Built);
    fflush(stdout);   /* progress must show live in piped/background native runs */
  }
}

} // namespace FlightBox
