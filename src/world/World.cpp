#include "World.h"
#include "TerrainLoader.h"
#include "Camera.h"
#include "Capacity.h"
#include "Geodesy.h"
#include "Log.h"
#include "Units.h"
#include "TileGeodesy.h"
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

static const int kAnchorZ = 10;
static const int kGrace = 180;
static const double kEarthCirc = 40075016.686;

static const double kEdgeTau = 384.0;

static const int kGrid = 128;
static const double kCosView = 0.5;
static const int kNodeCeil = 6000;

static inline uint64_t Key(int z, long x, long y) {
  return ((uint64_t)(z & 0xF) << 60) | ((uint64_t)(x & 0x3FFFFFFF) << 30) | (uint64_t)(y & 0x3FFFFFFF);
}
World::World(double pixelFocalLength)
  : PixelFocal_(pixelFocalLength),
    ViewM(6000.0), Lat0(0), Lon0(0),
    Pass(0), Evicted(0), LastLog(0), Leaves(0), DrawnReady(0), Pending(0), TargetTot(0), TargetRdy(0),
    MeshVram(0) {}

World::~World() = default;

World::Pools World::HeapPools() const {
  Pools p;
  for (const Node &n : Nodes)
    p.TileNodes += CapacityBytes(n.verts) + CapacityBytes(n.idx) + CapacityBytes(n.clusters);
  p.TileNodes += CapacityBytes(Nodes) + CapacityBytes(DrawnHandles_) + CapacityBytes(WorkList) +
                 CapacityBytes(DrawnLeaves) + CapacityBytes(Uncollected_) + CapacityBytes(Retired_);
  p.Vectors = Vectors_.HeapBytes();
  p.Buildings = Buildings_.HeapBytes() + CapacityBytes(BuildingDagVerts) +
                CapacityBytes(BuildingDagIdx) + CapacityBytes(BuildingClusters) +
                CapacityBytes(BuildingSoup) + CapacityBytes(FootprintTileEnds_);
  p.Water = Water_.HeapBytes() + CapacityBytes(WaterVerts);
  p.Streets = Streets_.HeapBytes();
  p.Class = Cls_.HeapBytes();
  if (const TilePool *pool = Pool_.get()) {
    p.ByteCache = pool->ByteCacheBytes();
    p.DemCache = pool->DemCacheBytes();
    p.Scheduler = pool->SchedulerBytes();
  }
  return p;
}

bool World::Open(Data::SourceSet &sources, Data::Transport &transport, double lat, double lon,
                 double viewMeters) {
  ViewM = viewMeters;
  Lat0 = lat;
  Lon0 = lon;
  Index_.clear();
  Pool_ = std::make_unique<TilePool>(GroundPoolConfig(lat, lon), sources, transport);
  Ground_ = std::make_unique<GroundStream>(*Pool_, GroundSurface{kMaxZ, kGrid});
  Cls_.Open(lat, lon);

  double origin[3];
  GeoToEcef(lat, lon, 0.0, origin);
  Buildings_.AnchorAt(origin);
  Water_.AnchorAt(origin);
  Opened = true;
  return true;
}

double World::SpanM(int z) const {
  return kEarthCirc * std::cos(Lat0 * kPi / 180.0) / (double)(1L << z);
}

void World::Center(int z, long x, long y, double out[3]) const {
  Geo g = TileFracToGeo(z, (uint32_t)x, (uint32_t)y, 0.5, 0.5);
  g.AltM = 0.0;
  Ecef e = GeoToEcefWgs84(g);
  out[0] = e.X; out[1] = e.Y; out[2] = e.Z;
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
  if (nd.Mesh != MeshState::Held || nd.handle >= 0) return;
  nd.handle = handle;
  std::vector<float>().swap(nd.verts);
  std::vector<uint32_t>().swap(nd.idx);
  std::vector<DagCluster>().swap(nd.clusters);
  nd.readyPass = Pass;
  Built++;
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
  if (Nodes[idx].Mesh == MeshState::Held) { c[0] = Nodes[idx].origin[0]; c[1] = Nodes[idx].origin[1]; c[2] = Nodes[idx].origin[2]; }
  else Center(z, x, y, c);
  double dist = Dist(c, eye);
  if (dist < 1.0) dist = 1.0;
  double dir[3] = {(c[0] - eye[0]) / dist, (c[1] - eye[1]) / dist, (c[2] - eye[2]) / dist};
  double d = dir[0] * fwd[0] + dir[1] * fwd[1] + dir[2] * fwd[2];
  double weight = d > kCosView ? 1.0 : 0.05;
  WorkList.push_back(Work{idx, weight / dist});
}

void World::Emit(int idx) {
  Leaves++;
  Node &n = Nodes[idx];
  if (Ready(n)) {
    n.emitPass = Pass;
    DrawnReady++;
    MeshVram += (long)n.nverts * 32 + (long)n.nidx * 4;
    DrawnHandles_.push_back(n.handle);
    DrawnLeaves.push_back(idx);
  } else {
    Pending++;
  }
}

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

bool World::Splits(int z, long x, long y, const double eye[3]) const {
  if (!WantSplit(z, x, y, eye)) return false;
  const int idx = Find(z, x, y);
  return idx < 0 || !Nodes[idx].SplitRefused;
}

bool World::CanCover(int z, long x, long y, const double eye[3]) const {
  if (Splits(z, x, y, eye)) {
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

void World::RequestSubtree(int z, long x, long y, const double eye[3], const double fwd[3]) {
  int idx = Ensure(z, x, y);
  Nodes[idx].touch = Pass;
  if (Splits(z, x, y, eye)) {
    bool anyV = false;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      RequestSubtree(z + 1, cx, cy, eye, fwd);
    }
    if (anyV) return;
  }
  if (Wants(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);
}

void World::DrawChildren(int z, long x, long y, const double eye[3], const double fwd[3]) {
  for (int q = 0; q < 4; q++) {
    long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
    if (Viable(z + 1, cx, cy, eye)) Descend(z + 1, cx, cy, eye, fwd);
  }
}

int World::Descend(int z, long x, long y, const double eye[3], const double fwd[3]) {
  int idx = Ensure(z, x, y);
  Nodes[idx].touch = Pass;

  if (WantSplit(z, x, y, eye) && !Splits(z, x, y, eye)) {
    bool anyV = false;
    for (int q = 0; q < 4 && !anyV; q++)
      anyV = Viable(z + 1, x * 2 + (q & 1), y * 2 + (q >> 1), eye);
    if (anyV) {
      if (Wants(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);
      if (Ready(Nodes[idx])) { Emit(idx); return 1; }
      DrawChildren(z, x, y, eye, fwd);
      return 0;
    }
  }
  if (Splits(z, x, y, eye)) {
    bool anyV = false, allC = true;
    for (int q = 0; q < 4; q++) {
      long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
      if (!Viable(z + 1, cx, cy, eye)) continue;
      anyV = true;
      if (!CanCover(z + 1, cx, cy, eye)) allC = false;
    }
    if (anyV) {
      if (allC) {
        DrawChildren(z, x, y, eye, fwd);
        return 1;
      }
      if (Ready(Nodes[idx])) {
        for (int q = 0; q < 4; q++) {
          long cx = x * 2 + (q & 1), cy = y * 2 + (q >> 1);
          if (Viable(z + 1, cx, cy, eye)) RequestSubtree(z + 1, cx, cy, eye, fwd);
        }
        Emit(idx);
        return 1;
      }

      DrawChildren(z, x, y, eye, fwd);
      return 0;
    }
  }

  if (Nodes[idx].Mesh == MeshState::Vacant) return 0;

  if (Wants(Nodes[idx])) AddWork(idx, z, x, y, eye, fwd);
  Emit(idx);
  return Ready(Nodes[idx]) ? 1 : 0;
}

void World::CountTargets(int z, long x, long y, const double eye[3], const double fwd[3],
                         int &total, int &ready, int &inView) const {
  if (Splits(z, x, y, eye)) {
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
  if (idx >= 0 && Settled(Nodes[idx])) ready++;

  double c[3];
  Center(z, x, y, c);
  const double d = Dist(c, eye);
  if (d < 1.0) { inView++; return; }
  const double dot = ((c[0] - eye[0]) * fwd[0] + (c[1] - eye[1]) * fwd[1] +
                      (c[2] - eye[2]) * fwd[2]) / d;
  if (dot > kCosView) inView++;
}

void World::SurfaceAnchor(int z, long x, long y, double out[3]) const {
  const int za = z < kAnchorZ ? z : kAnchorZ;
  const int sh = z - za;
  Center(za, x >> sh, y >> sh, out);
}

void World::Update(double camLat, double camLon) {
  const double tUpdate = Clock();
  while (camLon > 180.0) camLon -= 360.0;
  while (camLon < -180.0) camLon += 360.0;
  Pool_->Camera(camLat, camLon);

  const double tCls = Clock();
  Cls_.Update(*Pool_, camLat, camLon);
  ClassMs_ = Clock() - tCls;

  const double tBld = Clock();
  BuildingDecodeMs_ = 0.0;
  Vectors_.Build(*Pool_, camLat, camLon, 1);
  const size_t hadWater = Water_.Surfaces().size() + Water_.Courses().size();
  if (Veg_) Water_.Ingest(*Ground_, Vectors_, *Veg_);
  if (Veg_) Streets_.Ingest(Vectors_, *Veg_);
  if (Water_.Surfaces().size() + Water_.Courses().size() != hadWater) WaterDirty_ = true;
  CutKerbs();
  if (Buildings_.Build(*Ground_, Vectors_, Span<const WayLine>(Kerbs_.data(), Kerbs_.size())) > 0 &&
      Buildings_.AddedCount() > 0) {
    BuildingVerts = (uint32_t)(Buildings_.Verts().size() / 8);
    BuildingDecodeMs_ = Clock() - tBld;
    FootprintTileEnds_.push_back((uint32_t)Buildings_.Verts().size());
  }
  BuildingMs_ = Clock() - tBld;
  UpdateMs_ = Clock() - tUpdate;
}

void World::CutKerbs() {
  const std::vector<double> &pts = Vectors_.Points();
  Kerbs_.clear();
  Kerbs_.reserve(Streets_.Ways().size());
  for (const StreetField::Way &w : Streets_.Ways()) {
    if (w.Form != StreetField::Shape::Ribbon || w.HalfWidthM <= 0.0f || w.PointCount < 2) continue;
    WayLine l;
    l.LatLon = Span<const double>(pts.data() + (size_t)w.FirstPoint * 2, (size_t)w.PointCount * 2);
    l.HalfWidthM = w.HalfWidthM;
    l.MinLat = l.MaxLat = l.LatLon[0];
    l.MinLon = l.MaxLon = l.LatLon[1];
    for (size_t k = 2; k + 1 < l.LatLon.Size(); k += 2) {
      l.MinLat = std::min(l.MinLat, l.LatLon[k]);
      l.MaxLat = std::max(l.MaxLat, l.LatLon[k]);
      l.MinLon = std::min(l.MinLon, l.LatLon[k + 1]);
      l.MaxLon = std::max(l.MaxLon, l.LatLon[k + 1]);
    }
    Kerbs_.push_back(l);
  }
}

void World::AdmitMesh(Node &nd, int &budget) {
  Adm_.Wanted++;
  if (budget <= 0) { Adm_.Capped++; return; }
  Adm_.Asked++;
  const double tMesh = Clock();
  TileBuild tile;

  switch (Pool_->Mesh(nd.z, (uint32_t)nd.x, (uint32_t)nd.y, kGrid, &tile)) {
    case TilePool::Reply::Ready:
      nd.verts = std::move(tile.Verts);
      nd.idx = std::move(tile.Idx);
      nd.clusters = std::move(tile.Clusters);
      nd.nverts = (int)(nd.verts.size() / 8);
      nd.nidx = (int)nd.idx.size();
      nd.err = tile.ErrM;
      for (int c = 0; c < 3; c++) nd.origin[c] = tile.OriginEcef[c];
      SurfaceAnchor(nd.z, nd.x, nd.y, nd.anchor);
      nd.Mesh = MeshState::Held;
      budget--;
      Adm_.Admitted++;
      break;

    case TilePool::Reply::Refused:
    case TilePool::Reply::Pending:
      Adm_.Waiting++;
      break;

    case TilePool::Reply::Undeclared:
    case TilePool::Reply::Absent: {
      nd.Mesh = MeshState::Vacant;
      const int up = nd.z > kRootZ ? Find(nd.z - 1, nd.x >> 1, nd.y >> 1) : -1;
      if (up >= 0) Nodes[up].SplitRefused = true;
      Adm_.Absent++;
      break;
    }
  }
  MeshMs_ += Clock() - tMesh;
}

void World::NoteBand(bool inBand, double latDeg, double lonDeg) {
  if (inBand == EyeInBand_) return;
  EyeInBand_ = inBand;
  if (inBand)
    Log::Info("world", "eye_entered_mercator_band", {{"latDeg", latDeg}, {"lonDeg", lonDeg}});
  else
    Log::Error("world", "eye_left_mercator_band",
               {{"latDeg", latDeg}, {"lonDeg", lonDeg},
                {"limitDeg", kMercatorLatMaxDeg}});
}

void World::RootRing(const Eye &eye, uint32_t rx, uint32_t ry) {
  const long Rt = (long)std::ceil(ViewM / SpanM(kRootZ)) + 1;
  const long n = 1L << kRootZ;
  for (long ty = (long)ry - Rt; ty <= (long)ry + Rt; ty++)
    for (long tx = (long)rx - Rt; tx <= (long)rx + Rt; tx++) {
      if (tx < 0 || ty < 0 || tx >= n || ty >= n) continue;
      if (!Viable(kRootZ, tx, ty, eye.PosEcef)) continue;
      Descend(kRootZ, tx, ty, eye.PosEcef, eye.FwdEcef);
      CountTargets(kRootZ, tx, ty, eye.PosEcef, eye.FwdEcef, TargetTot, TargetRdy, TargetView);
    }
}

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

  TargetTot = TargetRdy = TargetView = 0;
  uint32_t rx = 0, ry = 0;
  const bool inBand =
      TileIndex::Of(Geo{camLon, eye.LatDeg, 0.0}, kRootZ).TryXy(&rx, &ry);
  NoteBand(inBand, eye.LatDeg, camLon);
  if (inBand) RootRing(eye, rx, ry);

  std::sort(WorkList.begin(), WorkList.end(), [](const Work &a, const Work &b) { return a.prio > b.prio; });
  int build = kMeshBuildsPerPass;
  MeshMs_ = 0.0;
  for (const Work &w : WorkList) {
    Node &nd = Nodes[w.idx];
    if (nd.Mesh == MeshState::Wanted) AdmitMesh(nd, build);
    if (nd.Mesh == MeshState::Held && nd.handle < 0) {
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

    const TilePool::Reply built = Pool_->Dag(
        BuildingDagId, BuildingSoup.data(), (int)(BuildingSoup.size() / 8), 3, &ladder);
    if (built == TilePool::Reply::Ready) {
      const uint32_t vbase = (uint32_t)(BuildingDagVerts.size() / 8);
      const uint32_t ibase = (uint32_t)BuildingDagIdx.size();
      BuildingDagVerts.insert(BuildingDagVerts.end(), ladder.Verts.begin(), ladder.Verts.end());
      for (uint32_t i : ladder.Idx) BuildingDagIdx.push_back(i + vbase);
      for (DagCluster c : ladder.Clusters) {
        c.First += ibase;
        BuildingClusters.push_back(c);
      }
      BuildingSeq_++;
      if (getenv("OUTSHINE_DAGLOG")) {
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

    if (built == TilePool::Reply::Absent)
      Log::Warn("world", "building_block_refused", {{"block", (int)DagDone_},
          {"soupVerts", (int)(BuildingSoup.size() / 8)}});
    if (built != TilePool::Reply::Pending) {
      BuildingDagId = 0;
      DagDone_++;
      std::vector<float>().swap(BuildingSoup);
    }
    BuildingDagMs_ = Clock() - tDag;
  }

  for (size_t i = 0; i < Nodes.size();) {
    Node &nd = Nodes[i];
    if (nd.touch == Pass) { nd.stale = 0; i++; continue; }
    if (++nd.stale <= kGrace) { i++; continue; }
    if (nd.handle >= 0) Retired_.push_back(nd.handle);

    if (nd.Mesh != MeshState::Held)
      if (TilePool *pool = Pool_.get()) pool->ForgetMesh(nd.z, (uint32_t)nd.x, (uint32_t)nd.y);
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
    double dtMin = (nowMs - LastLog) / 60000.0;
    LastLog = nowMs;
    double buildsMin = dtMin > 0 ? (Built - PrevBuilt) / dtMin : 0.0;
    double evictMin = dtMin > 0 ? (Evicted - PrevEvicted) / dtMin : 0.0;
    PrevBuilt = Built; PrevEvicted = Evicted;
    Log::Debug("world", "fbworld", {{"leaves", Leaves}, {"drawn", DrawnReady}, {"pending", Pending},
                                      {"evicted", Evicted}, {"meshVramMB", (double)MeshVram / 1.0e6},
                                      {"nodes", (int)Nodes.size()},
                                      {"buildsPerMin", buildsMin},
                                      {"evictPerMin", evictMin}, {"built", Built}});
  }
  RefineMs_ = Clock() - tRefine;
}

World::Await World::SimWaiting() const {
  if (Vectors_.PendingTiles() != 0) return Await::Vectors;
  if (!Cls_.Complete()) return Await::Class;
  return Await::Nothing;
}

World::Await World::Waiting() const {
  if (TargetTot <= 0 || TargetRdy != TargetTot) return Await::TargetCut;
  if (BuildingDagId != 0) return Await::BuildingDag;
  if (DagDone_ != FootprintTileEnds_.size()) return Await::BuildingBlocks;
  return SimWaiting();
}

const char *AwaitName(World::Await await) {
  switch (await) {
    case World::Await::Nothing: return "nothing";
    case World::Await::TargetCut: return "targetCut";
    case World::Await::BuildingDag: return "buildingDag";
    case World::Await::BuildingBlocks: return "buildingBlocks";
    case World::Await::Vectors: return "vectors";
    case World::Await::Class: return "class";
  }
  return "nothing";
}

}
