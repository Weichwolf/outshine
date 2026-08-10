#include "World.h"
#include "Renderer.h"
#include "TerrainLoader.h"
#include "Camera.h"
#include "Capacity.h"
#include "Geodesy.h"
#include "Log.h"
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
 * tile refines as far as a rugged one at the same distance (equal albedo resolution by distance). */
static const double kEdgeTau = 384.0;   /* target max on-screen tile edge (px); lower = finer = more tiles */
/* Quads per tile edge, and it is the ZERO POINT of the whole LOD chain: the cluster DAG's level 0 IS
 * this mesh, so no tolerance below its own decimation error can ever be honoured. kEdgeTau/kGrid is
 * that error in pixels -- 3 px against the DAG's declared 1 px (render/ClusterDag.h). 128 is where
 * the price stops: 192 measured p95 21.4 ms against the 16.67 ms frame. */
static const int kGrid = 128;
static const double kCosView = 0.5;            /* frustum weight: <60deg off-axis -> full priority */
static const int kNodeCeil = 6000;             /* safety backstop on the working set */

/* Packed (z,x,y) node key: z<16, x/y<2^30 (z<=29). */
static inline uint64_t Key(int z, long x, long y) {
  return ((uint64_t)(z & 0xF) << 60) | ((uint64_t)(x & 0x3FFFFFFF) << 30) | (uint64_t)(y & 0x3FFFFFFF);
}
static std::unordered_map<uint64_t, int> gIndex;

World::World(double pixelFocalLength)
  : PixelFocal_(pixelFocalLength),
    R(nullptr), TS(512), ViewM(6000.0), Lat0(0), Lon0(0),
    Pass(0), Evicted(0), LastLog(0), Leaves(0), DrawnReady(0), Pending(0), TargetTot(0), TargetRdy(0),
    MeshVram(0) {}

World::~World() { if (Opened) fb_stream_close(); }

World::Pools World::HeapPools() const {
  Pools p;
  for (const Node &n : Nodes)
    p.TileNodes += CapacityBytes(n.verts) + CapacityBytes(n.idx) + CapacityBytes(n.clusters) +
                   CapacityBytes(n.albedo);
  p.TileNodes += CapacityBytes(Nodes) + CapacityBytes(DrawSlots) + CapacityBytes(WorkList) +
                 CapacityBytes(DrawnLeaves);
  p.Vectors = Vectors.HeapBytes();
  p.Buildings = Buildings.HeapBytes() + CapacityBytes(BuildingDagVerts) +
                CapacityBytes(BuildingDagIdx) + CapacityBytes(BuildingClusters) +
                CapacityBytes(BuildingSoup);
  p.Water = Water.HeapBytes() + CapacityBytes(WaterVerts);
  p.Class = Cls_.HeapBytes();
  return p;
}

size_t World::ByteCacheBytes() const { return (size_t)fb_stream_cache_bytes(); }

std::vector<ModuleMemory> World::WorkerMemory() const {
  std::vector<ModuleMemory> rows(kMaxTileWorkers);
  rows.resize((size_t)fb_stream_worker_memory(rows.data(), (int)rows.size()));
  return rows;
}

bool World::Open(Render::Renderer *renderer, const char *tilesBase, double lat, double lon,
                   double viewMeters, int albedoTS) {
  R = renderer;
  TS = albedoTS;
  ViewM = viewMeters;
  Lat0 = lat;
  Lon0 = lon;
  gIndex.clear();
  if (fb_stream_open(tilesBase, lat, lon, {kMaxZ, kGrid}) == 0) return false;
  Cls_.Open(lat, lon);
  Opened = true;
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
    MeshVram += (long)n.nverts * 32 + (long)n.nidx * 4;
    DrawSlots.push_back(n.slot);
    DrawnLeaves.push_back(idx);
  } else {
    Pending++;
  }
}

/* A pure function of camera and tile geometry, needing NO tile data — which is what lets the
 * boot/teleport request go straight to the final leaves, with no LOD ladder and no rebuild churn. */
bool World::WantSplit(int z, long x, long y, const double eye[3]) const {
  if (z >= kMaxZ || (int)Nodes.size() >= kNodeCeil) return false;
  double c[3]; Center(z, x, y, c);
  double dist = Dist(c, eye); if (dist < 1.0) dist = 1.0;
  return SpanM(z) * PixelFocal_ / dist > kEdgeTau;
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
 * level is ever built — only the geometry-target leaves. */
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
void World::CountTargets(int z, long x, long y, const double eye[3], const double fwd[3],
                         int &total, int &ready, int &inView) const {
  if (WantSplit(z, x, y, eye)) {
    bool anyV = false;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      CountTargets(z + 1, cx, cy, eye, fwd, total, ready, inView);
    }
    if (anyV) return;
  }
  total++;
  int idx = Find(z, x, y);
  if (idx >= 0 && Ready(Nodes[idx])) ready++;
  /* PUBLISHED, NOT ACTED ON. The target cut is the view RADIUS; how much of it the camera can
   * actually see is a different, smaller number, and the two are only comparable if both are
   * counted. The cone is `kCosView`, the same one AddWork prioritises by — wider than the real
   * frustum, so this is an upper bound on what a frustum-scoped cut would have to wait for. */
  double c[3];
  Center(z, x, y, c);
  const double d = Dist(c, eye);
  if (d < 1.0) { inView++; return; }
  const double dot = ((c[0] - eye[0]) * fwd[0] + (c[1] - eye[1]) * fwd[1] +
                      (c[2] - eye[2]) * fwd[2]) / d;
  if (dot > kCosView) inView++;
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
  TargetTot = TargetRdy = TargetView = 0;
  for (long ty = (long)ry - Rt; ty <= (long)ry + Rt; ty++)
    for (long tx = (long)rx - Rt; tx <= (long)rx + Rt; tx++) {
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) continue;
      if (Viable(kRootZ, tx, ty, eyeEcef)) {
        Descend(kRootZ, tx, ty, eyeEcef, fwdEcef);
        CountTargets(kRootZ, tx, ty, eyeEcef, fwdEcef, TargetTot, TargetRdy, TargetView);
      }
    }

  /* Budgeted, worst-first (nearest + in-frustum). The cap is in ITEMS and that is now enough, because
   * an item costs a memcpy: fetch, mesh and DAG all happen off this thread (world/TerrainLoader.h).
   * A wall-clock slice was measured HERE and rejected — it moved the cost rather than removing it,
   * p95 17.9 -> 25.8 ms with the same maximum, because the work is not divisible below one tile. */
  std::sort(WorkList.begin(), WorkList.end(), [](const Work &a, const Work &b) { return a.prio > b.prio; });
  int build = 2, upload = 6;
  MeshMs_ = AlbedoMs_ = UploadMs_ = BuildingMs_ = BuildingDecodeMs_ = 0.0;
  for (const Work &w : WorkList) {
    if (build == 0 && upload == 0) break;
    Node &nd = Nodes[w.idx];
    const double tMesh = Clock();
    if (!nd.haveMesh && build > 0) {
      float *v = nullptr;
      uint32_t *ix = nullptr;
      DagCluster *cl = nullptr;
      int nv = 0, ni = 0, ncl = 0;
      double o[3];
      float err = 0.f;
      /* The DAG arrived built (world/TerrainLoader.h): what this frame pays is the copy into the
       * node, not the simplifier. */
      if (fb_stream_build(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, kGrid, &v, &nv, &ix, &ni, &cl, &ncl,
                          o, &err)) {
        nd.verts.assign(v, v + (size_t)nv * 8);
        nd.idx.assign(ix, ix + ni);
        nd.clusters.assign(cl, cl + ncl);
        if (getenv("FB_DAGLOG")) {
          long per[16] = {0};
          int lv = 0;
          for (const DagCluster &c : nd.clusters) { per[c.Level] += c.Count / 3; if (c.Level > lv) lv = c.Level; }
          std::string t;
          for (int i = 0; i <= lv; i++) t += "L" + std::to_string(i) + "=" + std::to_string(per[i]) + " ";
          Log::Debug("world", "dag", {{"z", nd.z}, {"dagVerts", nv},
              {"clusters", ncl}, {"levels", t}});
        }
        free(v);
        free(ix);
        free(cl);
        nd.nverts = nv;
        nd.nidx = ni;
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
      nd.slot = R->UploadTile(nd.verts.data(), (uint32_t)nd.nverts, nd.idx.data(), (uint32_t)nd.nidx,
                              nd.clusters.data(), (int)nd.clusters.size(), nd.origin, anchor);
      if (nd.slot >= 0) {
        std::vector<float>().swap(nd.verts);
        std::vector<uint32_t>().swap(nd.idx);
        std::vector<DagCluster>().swap(nd.clusters);
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

  /* THE GROUND CLASS. It is a property of the PLACE, so the fragment and every CPU consumer read it
   * from the same bytes (world/ClassField.h). What happens here is streaming and bookkeeping — the
   * grid itself is laid down on ClassBuilder's thread and arrives whole. */
  const double tCls = Clock();
  Cls_.Update(camLat, camLon);
  if (R) {
    R->SetClassFrame(Cls_.EastEcef(), Cls_.NorthEcef(), Cls_.Cam());
    /* THE VERSION IS THE UPLOAD'S TRIGGER, not a dirty flag: what is on the device is named by the
     * structure it came from, so a missed or a doubled upload is a mismatch and not a guess. */
    const std::shared_ptr<const ClassStructure> cls = Cls_.Read();
    if (cls && cls->Version() != UploadedClassVersion_) {
      const double tu = Clock();
      R->WriteClassBuffer(cls->Words(), cls->Bytes());
      UploadedClassVersion_ = cls->Version();
      Log::Debug("world", "class_uploaded", {{"version", (double)cls->Version()},
          {"uploadMs", Clock() - tu}, {"streamMs", Cls_.LastStreamMs()},
          {"ingestMs", Cls_.LastIngestMs()}, {"bufferKB", cls->Bytes() / 1024.0}});
    }
  }
  ClassMs_ = Clock() - tCls;

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
    uint32_t *di = nullptr;
    DagCluster *dc = nullptr;
    int ndv = 0, ndi = 0, ndc = 0;
    /* Float 3 is uv.x, and a negative one tags the roof cap: an ATTRIBUTE seam, so the cap and its
     * wall keep two vertices but collapse as one point — otherwise the ring would read as a mesh
     * boundary and nothing would move. */
    if (fb_stream_dag(BuildingDagId, BuildingSoup.data(), (int)(BuildingSoup.size() / 8), 3,
                      &dv, &ndv, &di, &ndi, &dc, &ndc)) {
      const uint32_t vbase = (uint32_t)(BuildingDagVerts.size() / 8);
      const uint32_t ibase = (uint32_t)BuildingDagIdx.size();
      BuildingDagVerts.insert(BuildingDagVerts.end(), dv, dv + (size_t)ndv * 8);
      for (int i = 0; i < ndi; i++) BuildingDagIdx.push_back(di[i] + vbase);
      for (int i = 0; i < ndc; i++) {
        dc[i].First += ibase;
        BuildingClusters.push_back(dc[i]);
      }
      free(dv);
      free(di);
      free(dc);
      BuildingDagId = 0;
      std::vector<float>().swap(BuildingSoup);
      if (getenv("FB_DAGLOG")) {
        long per[16] = {0};
        int lv = 0;
        for (const DagCluster &c : BuildingClusters) { per[c.Level] += c.Count / 3; if (c.Level > lv) lv = c.Level; }
        float emin[16], emax[16];
        for (int i = 0; i < 16; i++) { emin[i] = 1e30f; emax[i] = 0.0f; }
        for (const DagCluster &c : BuildingClusters) {
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
                         BuildingDagIdx.data(), (uint32_t)BuildingDagIdx.size(),
                         BuildingClusters.data(), (int)BuildingClusters.size(), Buildings.Anchor());
    }
  }
  BuildingMs_ = Clock() - tBld;

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
                                      {"buildsPerMin", buildsMin},
                                      {"evictPerMin", evictMin}, {"built", (int)Built}});
  }
  UpdateMs_ = Clock() - tUpdate;
}

} // namespace outshine::World
