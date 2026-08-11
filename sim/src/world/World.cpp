#include "World.h"
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
 * that error in pixels -- 3 px against the DAG's declared 1 px (core/ClusterDag.h). 128 is where
 * the price stops: 192 measured p95 21.4 ms against the 16.67 ms frame. */
static const int kGrid = 128;
static const double kCosView = 0.5;            /* frustum weight: <60deg off-axis -> full priority */
static const int kNodeCeil = 6000;             /* safety backstop on the working set */

/* Packed (z,x,y) node key: z<16, x/y<2^30 (z<=29). */
static inline uint64_t Key(int z, long x, long y) {
  return ((uint64_t)(z & 0xF) << 60) | ((uint64_t)(x & 0x3FFFFFFF) << 30) | (uint64_t)(y & 0x3FFFFFFF);
}
World::World(double pixelFocalLength)
  : PixelFocal_(pixelFocalLength),
    TS(512), ViewM(6000.0), Lat0(0), Lon0(0),
    Pass(0), Evicted(0), LastLog(0), Leaves(0), DrawnReady(0), Pending(0), TargetTot(0), TargetRdy(0),
    MeshVram(0) {}

World::~World() { if (Opened) fb_stream_close(); }

World::Pools World::HeapPools() const {
  Pools p;
  for (const Node &n : Nodes)
    p.TileNodes += CapacityBytes(n.verts) + CapacityBytes(n.idx) + CapacityBytes(n.clusters) +
                   CapacityBytes(n.albedo);
  p.TileNodes += CapacityBytes(Nodes) + CapacityBytes(DrawnHandles_) + CapacityBytes(WorkList) +
                 CapacityBytes(DrawnLeaves) + CapacityBytes(Uncollected_) + CapacityBytes(Retired_);
  p.Vectors = Vectors_.HeapBytes();
  p.Buildings = Buildings_.HeapBytes() + CapacityBytes(BuildingDagVerts) +
                CapacityBytes(BuildingDagIdx) + CapacityBytes(BuildingClusters) +
                CapacityBytes(BuildingSoup) + CapacityBytes(FootprintTileEnds_);
  p.Water = Water_.HeapBytes() + CapacityBytes(WaterVerts);
  p.Class = Cls_.HeapBytes();
  return p;
}

size_t World::ByteCacheBytes() const {
  const TilePool *pool = fb_tile_pool();
  return pool ? pool->ByteCacheBytes() : 0;
}

bool World::Open(const char *tilesBase, double lat, double lon, double viewMeters, int albedoTS) {
  TS = albedoTS;
  ViewM = viewMeters;
  Lat0 = lat;
  Lon0 = lon;
  Index_.clear();
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
  auto it = Index_.find(k);
  if (it != Index_.end()) return it->second;
  Node nd{};
  nd.z = z; nd.x = x; nd.y = y; nd.handle = -1;
  int idx = (int)Nodes.size();
  Nodes.push_back(std::move(nd));
  Index_[k] = idx;
  return idx;
}

void World::Collect(uint64_t id, int handle) {
  auto it = Index_.find(id);
  if (it == Index_.end() || handle < 0) return;
  Node &nd = Nodes[it->second];
  if (!nd.haveMesh || nd.handle >= 0) return;
  nd.handle = handle;
  std::vector<float>().swap(nd.verts);
  std::vector<uint32_t>().swap(nd.idx);
  std::vector<DagCluster>().swap(nd.clusters);
  nd.readyPass = Pass;   /* 2-phase: drawable next pass, once the collector's upload is submitted */
  Built++;   /* build completion (thrash: builds/min -> ~0 in a converged loiter, climbs if evict-rebuild) */
}

std::vector<int> World::TakeRetired() {
  std::vector<int> out;
  out.swap(Retired_);
  return out;
}

World::WaterSurface World::Water() const {
  WaterSurface s;
  if (WaterVerts.empty()) return s;
  s.Verts = WaterVerts.data();
  s.VertCount = (uint32_t)(WaterVerts.size() / 6);
  s.AnchorEcef = Water_.Anchor();
  s.Seq = WaterSeq_;
  return s;
}

World::BuildingSurface World::Buildings() const {
  BuildingSurface s;
  if (BuildingDagVerts.empty()) return s;
  s.Verts = BuildingDagVerts.data();
  s.VertCount = (uint32_t)(BuildingDagVerts.size() / 8);
  s.Idx = BuildingDagIdx.data();
  s.IdxCount = (uint32_t)BuildingDagIdx.size();
  s.Clusters = BuildingClusters.data();
  s.ClusterCount = (int)BuildingClusters.size();
  s.AnchorEcef = Buildings_.Anchor();
  s.Seq = BuildingSeq_;
  return s;
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
  if (Ready(n)) {   /* collected and one pass elapsed — safe to draw */
    n.emitPass = Pass;   /* drawn this pass -> a mode switch may keep showing it (old mode) until the overlay lands */
    DrawnReady++;
    MeshVram += (long)n.nverts * 32 + (long)n.nidx * 4;
    DrawnHandles_.push_back(n.handle);
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
  auto it = Index_.find(Key(z, x, y));
  return it == Index_.end() ? -1 : it->second;
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
  if (!Taken(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);   /* TARGET leaf */
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
  if (!Taken(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);
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

/* THE SIMULATION PASS. Every statement here is about a place: which vectors have landed, what the
 * class grid says, where water stands and which footprint sits on the ground. No camera, no device,
 * and nothing that only a picture needs. */
void World::Update(double camLat, double camLon) {
  const double tUpdate = Clock();
  while (camLon > 180.0) camLon -= 360.0;   /* normalize lon before tile queries (dateline) */
  while (camLon < -180.0) camLon += 360.0;
  fb_tile_pool()->Camera(camLat, camLon);   /* the queue drains nearest-first from here */

  /* THE GROUND CLASS. It is a property of the PLACE, so the fragment and every CPU consumer read it
   * from the same bytes (world/ClassField.h). What happens here is streaming and bookkeeping — the
   * grid itself is laid down on ClassBuilder's thread and arrives whole. */
  const double tCls = Clock();
  Cls_.Update(camLat, camLon);
  ClassMs_ = Clock() - tCls;

  /* OSM buildings: the 3x3 z14 block around the camera, one tile per pass, built once and kept. The
   * vector tile is the SAME source the albedo raster is baked from, so a footprint and its grey patch
   * cannot disagree about where a house is. */
  const double tBld = Clock();
  BuildingDecodeMs_ = 0.0;
  Vectors_.Build(camLat, camLon, 1);
  const size_t hadWater = Water_.Surfaces().size() + Water_.Courses().size();
  if (Veg_) Water_.Ingest(Vectors_, *Veg_);
  if (Water_.Surfaces().size() + Water_.Courses().size() != hadWater) WaterDirty_ = true;
  if (Buildings_.Build(Vectors_) > 0 && Buildings_.AddedCount() > 0) {
    BuildingVerts = (uint32_t)(Buildings_.Verts().size() / 8);
    BuildingDecodeMs_ = Clock() - tBld;
    FootprintTileEnds_.push_back((uint32_t)Buildings_.Verts().size());
  }
  BuildingMs_ = Clock() - tBld;
  UpdateMs_ = Clock() - tUpdate;
}

/* THE PICTURE PASS. The LOD cut against one eye, the meshes it needs, and the two surfaces that are
 * geometry rather than place. Nothing is pushed anywhere: what comes out is Uncollected(), Drawn(),
 * TakeRetired(), Water() and Buildings(). */
void World::Refine(const Eye &eye, double nowMs) {
  const double tRefine = Clock();
  Pass++;
  DrawnHandles_.clear();
  DrawnLeaves.clear();
  WorkList.clear();
  Uncollected_.clear();
  Leaves = DrawnReady = Pending = 0;
  MeshVram = 0;
  double camLon = eye.LonDeg;
  while (camLon > 180.0) camLon -= 360.0;
  while (camLon < -180.0) camLon += 360.0;

  /* Root ring: zROOT tiles whose centre is within the view radius. */
  uint32_t rx = 0, ry = 0;
  osmmesh_geo_to_tile(camLon, eye.LatDeg, (uint8_t)kRootZ, &rx, &ry);
  double span = SpanM(kRootZ);
  long Rt = (long)std::ceil(ViewM / span) + 1;
  long n = 1L << kRootZ;
  TargetTot = TargetRdy = TargetView = 0;
  for (long ty = (long)ry - Rt; ty <= (long)ry + Rt; ty++)
    for (long tx = (long)rx - Rt; tx <= (long)rx + Rt; tx++) {
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) continue;
      if (Viable(kRootZ, tx, ty, eye.PosEcef)) {
        Descend(kRootZ, tx, ty, eye.PosEcef, eye.FwdEcef);
        CountTargets(kRootZ, tx, ty, eye.PosEcef, eye.FwdEcef, TargetTot, TargetRdy, TargetView);
      }
    }

  /* Budgeted, worst-first (nearest + in-frustum). The cap is in ITEMS and that is now enough, because
   * an item costs a memcpy: fetch, mesh and DAG all happen off this thread (world/TerrainLoader.h).
   * A wall-clock slice was measured HERE and rejected — it moved the cost rather than removing it,
   * p95 17.9 -> 25.8 ms with the same maximum, because the work is not divisible below one tile. */
  std::sort(WorkList.begin(), WorkList.end(), [](const Work &a, const Work &b) { return a.prio > b.prio; });
  int build = 2;
  MeshMs_ = 0.0;
  for (const Work &w : WorkList) {
    Node &nd = Nodes[w.idx];
    if (!nd.haveMesh && build > 0) {
      const double tMesh = Clock();
      TileBuild tile;
      /* The DAG arrived built (world/TilePool.h): what this frame pays is the move into the node,
       * not the simplifier. */
      if (fb_tile_pool()->Mesh(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, kGrid, &tile) ==
          TilePool::Reply::Ready) {
        nd.verts = std::move(tile.Verts);
        nd.idx = std::move(tile.Idx);
        nd.clusters = std::move(tile.Clusters);
        nd.nverts = (int)(nd.verts.size() / 8);
        nd.nidx = (int)nd.idx.size();
        nd.err = tile.ErrM;
        for (int c = 0; c < 3; c++) nd.origin[c] = tile.OriginEcef[c];
        SurfaceAnchor(nd.z, nd.x, nd.y, nd.anchor);
        nd.haveMesh = 1;
        build--;
      }
      MeshMs_ += Clock() - tMesh;
    }
    if (nd.haveMesh && nd.handle < 0) {
      TileMesh m;
      m.Id = Key(nd.z, nd.x, nd.y);
      m.Verts = nd.verts.data();
      m.VertCount = (uint32_t)nd.nverts;
      m.Idx = nd.idx.data();
      m.IdxCount = (uint32_t)nd.nidx;
      m.Clusters = nd.clusters.data();
      m.ClusterCount = (int)nd.clusters.size();
      for (int c = 0; c < 3; c++) { m.OriginEcef[c] = nd.origin[c]; m.AnchorEcef[c] = nd.anchor[c]; }
      Uncollected_.push_back(m);
    }
  }

  /* The water surface is a mesh and its level is not: the level came from the shore while the vectors
   * were ingested, and this is only the ribbon over it. */
  if (WaterDirty_) {
    WaterDirty_ = false;
    Water_.Tessellate(Vectors_, WaterVerts);
    WaterSeq_++;
    Log::Debug("world", "water", {{"surfaces", (int)Water_.Surfaces().size()},
                                  {"courses", (int)Water_.Courses().size()},
                                  {"tris", (int)(WaterVerts.size() / 18)},
                                  {"noGround", (int)Water_.NoGroundCount()},
                                  {"outliers", (int)Water_.OutlierCount()}});
  }

  /* THE DAG IS BUILT OVER THE NEW TILE ALONE, in the tile pool, and appended when it lands. Over the
   * whole block it is superlinear — 265 ms at 51 456 verts, 480 ms at 58 368, measured — and even one
   * dense tile is 33.0 ms of a 50.9 ms frame natively (walkbench 150 m/s). Nothing is lost by
   * splitting it: the DAG's crack-free guarantee is about SHARED EDGES, and two buildings in two
   * tiles share none. */
  const double tDag = Clock();
  BuildingDagMs_ = 0.0;
  if (BuildingDagId == 0 && DagDone_ < FootprintTileEnds_.size()) {
    const uint32_t first = DagDone_ ? FootprintTileEnds_[DagDone_ - 1] : 0;
    const uint32_t end = FootprintTileEnds_[DagDone_];
    const std::vector<float> &soup = Buildings_.Verts();
    BuildingSoup.assign(soup.begin() + first, soup.begin() + end);
    BuildingDagId = ++BuildingDagSeq;
  }
  if (BuildingDagId != 0) {
    TileBuild ladder;
    /* Float 3 is uv.x, and a negative one tags the roof cap: an ATTRIBUTE seam, so the cap and its
     * wall keep two vertices but collapse as one point — otherwise the ring would read as a mesh
     * boundary and nothing would move. */
    if (fb_tile_pool()->Dag(BuildingDagId, BuildingSoup.data(), (int)(BuildingSoup.size() / 8), 3,
                            &ladder) == TilePool::Reply::Ready) {
      const uint32_t vbase = (uint32_t)(BuildingDagVerts.size() / 8);
      const uint32_t ibase = (uint32_t)BuildingDagIdx.size();
      BuildingDagVerts.insert(BuildingDagVerts.end(), ladder.Verts.begin(), ladder.Verts.end());
      for (uint32_t i : ladder.Idx) BuildingDagIdx.push_back(i + vbase);
      for (DagCluster c : ladder.Clusters) {
        c.First += ibase;
        BuildingClusters.push_back(c);
      }
      BuildingDagId = 0;
      DagDone_++;
      BuildingSeq_++;
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
    }
    BuildingDagMs_ = Clock() - tDag;
  }

  /* Grace-period eviction; swap-pop, so Index_ must stay in sync. */
  for (size_t i = 0; i < Nodes.size();) {
    Node &nd = Nodes[i];
    if (nd.touch == Pass) { nd.stale = 0; i++; continue; }
    if (++nd.stale <= kGrace) { i++; continue; }
    if (nd.handle >= 0) Retired_.push_back(nd.handle);
    Index_.erase(Key(nd.z, nd.x, nd.y));
    size_t last = Nodes.size() - 1;
    if (i != last) {
      Nodes[i] = std::move(Nodes[last]);
      Index_[Key(Nodes[i].z, Nodes[i].x, Nodes[i].y)] = (int)i;
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
  RefineMs_ = Clock() - tRefine;
}

} // namespace outshine::World
