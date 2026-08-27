#include "TilePool.h"

#include <numbers>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "Capacity.h"
#include "ChunkMesh.h"
#include "Delivery.h"
#include "Heap.h"
#include "Log.h"
#include "SourceSet.h"
#include "StackProbe.h"
#include "TerrainTiles.h"
#include "Transport.h"

namespace outshine::Ground {

namespace {

constexpr int kPollMs = 1;
constexpr int kPollAttempts = 30000;

thread_local double tFetchBlockedMs = 0.0;
thread_local bool tCarries = false;

uint64_t MeshKey(int z, uint32_t x, uint32_t y) {
  return ((uint64_t)1 << 62) | ((uint64_t)(z & 31) << 56)
       | ((uint64_t)(x & 0xFFFFFFFu) << 28) | (uint64_t)(y & 0xFFFFFFFu);
}
uint64_t DagKey(int id) { return ((uint64_t)2 << 62) | (uint64_t)(uint32_t)id; }
uint64_t RequestKey(const std::string &key) {
  uint64_t h = 1469598103934665603ull;
  for (char c : key) h = (h ^ (uint64_t)(uint8_t)c) * 1099511628211ull;
  return ((uint64_t)3 << 62) | (h & 0x3FFFFFFFFFFFFFFFull);
}

template <int A> int SeamAt(const float *v) { return v[A] < 0.0f ? 1 : 0; }
int (*const kSeam[8])(const float *) = {SeamAt<0>, SeamAt<1>, SeamAt<2>, SeamAt<3>,
                                        SeamAt<4>, SeamAt<5>, SeamAt<6>, SeamAt<7>};

}

TilePool::TilePool(const Config &config, Data::SourceSet &sources, Data::Transport &transport)
    : Sources_(sources),
      Wire_(transport),
      OriginLatDeg_(config.OriginLatDeg),
      OriginLonDeg_(config.OriginLonDeg),
      ByteBudget_(config.ByteBudget),
      DemCacheTiles_(config.DemCacheTiles),
      PollAttempts_(config.PollAttempts),
      CarrierCount_(config.Carriers),
      FocusLatDeg_(config.OriginLatDeg),
      FocusLonDeg_(config.OriginLonDeg) {
  const int n = config.Threads > 0 ? config.Threads : 1;
  ContextBytes_ = std::vector<std::atomic<size_t>>((size_t)n);
  Threads_.reserve((size_t)n);
  for (int i = 0; i < n; i++) Threads_.emplace_back([this, i] { Work(i); });
  const int carriers = CarrierCount_ > 0 ? CarrierCount_ : 2;
  Carriers_.reserve((size_t)carriers);
  for (int i = 0; i < carriers; i++) Carriers_.emplace_back([this] { Carry(); });
  Log::Info("world", "tilepool",
            {{"threads", n}, {"inFlightCap", n}, {"byteBudgetMB", (double)ByteBudget_ / 1048576.0},
             {"demCacheTilesPerThread", DemCacheTiles_}});
}

TilePool::~TilePool() {
  {
    std::lock_guard<std::mutex> lock(QueueMutex_);
    Stopping_ = true;
  }
  Wake_.notify_all();
  for (std::thread &t : Threads_) t.join();
  for (std::thread &t : Carriers_) t.join();
  const Ledger &l = Ledger_;
  const double meshes = l.MeshTiles > 0 ? (double)l.MeshTiles : 1.0;
  const double fetches = l.Fetches > 0 ? (double)l.Fetches : 1.0;
  Log::Debug("world", "tilepool_closed",
             {{"meshTiles", l.MeshTiles}, {"meshAbsent", l.MeshAbsent},
              {"meshRefused", l.MeshRefused},
              {"meshCpuMs", l.MeshCpuMs}, {"meshCpuMsPerTile", l.MeshCpuMs / meshes},
              {"dags", l.Dags}, {"dagMs", l.DagMs},
              {"fetches", l.Fetches}, {"fetchAbsent", l.FetchAbsent},
              {"fetchRefused", l.FetchRefused},
              {"fetchGaveUp", l.FetchGaveUp}, {"fetchMs", l.FetchMs},
              {"fetchMsPerGet", l.FetchMs / fetches},
              {"fetchBlockedMs", l.FetchBlockedMs}, {"fetchOnCompute", (int)l.FetchOnCompute},
              {"retryWaitMs", l.FetchBlockedMs - l.FetchMs}, {"fetchedMB", l.FetchedMB},
              {"byteCacheMB", (double)ByteCacheBytes() / 1048576.0},
              {"byteCacheEntries", (int)Cache_.size()}, {"evictions", l.Evictions}});
}

void TilePool::Focus(double latDeg, double lonDeg) {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  FocusLatDeg_ = latDeg;
  FocusLonDeg_ = lonDeg;
}

double TilePool::TileDistance(int z, uint32_t x, uint32_t y) const {
  const double n = std::ldexp(1.0, z);
  const double cx = (FocusLonDeg_ + 180.0) / 360.0 * n;
  const double lat = FocusLatDeg_ * std::numbers::pi / 180.0;
  const double cy = (1.0 - std::asinh(std::tan(lat)) / std::numbers::pi) * 0.5 * n;
  const double dx = (double)x + 0.5 - cx, dy = (double)y + 0.5 - cy;
  return dx * dx + dy * dy;
}

TilePool::Ledger TilePool::Counters() const {
  Ledger out;
  {
    std::lock_guard<std::mutex> lock(LedgerMutex_);
    out = Ledger_;
  }
  std::lock_guard<std::mutex> queue(QueueMutex_);
  out.Posts = Posts_;
  out.Repeats = Repeats_;
  out.QueueDepth = (long long)Queue_.size();
  return out;
}

size_t TilePool::ByteCacheBytes() const {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  size_t bytes = CapacityBytes(Cache_);
  for (const CacheEntry &e : Cache_) bytes += e.Key.capacity() + CapacityBytes(e.Data);
  return bytes;
}

size_t TilePool::DemCacheBytes() const {
  size_t bytes = CapacityBytes(ContextBytes_);
  for (const std::atomic<size_t> &slot : ContextBytes_) bytes += slot.load(std::memory_order_relaxed);
  return bytes;
}

size_t TilePool::SchedulerBytes() const {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  size_t bytes = CapacityBytes(Queue_);
  for (const Job &j : Queue_) bytes += (j.Ask ? j.Ask->Key().capacity() : 0) + CapacityBytes(j.Soup);
  bytes += TreeNodeBytes<uint64_t>(Posted_.size());
  bytes += TreeNodeBytes<std::pair<const uint64_t, Result>>(Done_.size());
  for (const std::pair<const uint64_t, Result> &d : Done_)
    bytes += CapacityBytes(d.second.Build.Verts) + CapacityBytes(d.second.Build.Idx) +
             CapacityBytes(d.second.Build.Clusters);
  return bytes;
}

TilePool::Reply TilePool::Lookup(const std::string &key, Landing *out) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  for (CacheEntry &e : Cache_) {
    if (e.Key != key) continue;
    e.Used = ++CacheClock_;
    if (e.Absent) return Reply::Absent;
    out->Bytes.assign(e.Data.begin(), e.Data.end());
    out->At = e.At;
    return Reply::Ready;
  }
  return Reply::Pending;
}

void TilePool::Remember(const std::string &key, const uint8_t *data, size_t len,
                        const Data::Address &at, bool absent) {
  std::lock_guard<std::mutex> lock(CacheMutex_);
  for (CacheEntry &e : Cache_)
    if (e.Key == key) return;
  long evicted = 0;
  while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_) {
    size_t victim = 0;
    for (size_t i = 1; i < Cache_.size(); i++)
      if (Cache_[i].Used < Cache_[victim].Used) victim = i;
    CacheBytes_ -= Cache_[victim].Data.size();
    Cache_.erase(Cache_.begin() + (long)victim);
    evicted++;
  }
  if (evicted > 0) {
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.Evictions += evicted;
  }
  CacheEntry e;
  e.Key = key;
  e.At = at;
  e.Absent = absent;
  e.Used = ++CacheClock_;
  if (!absent && len > 0) e.Data.assign(data, data + len);
  CacheBytes_ += e.Data.size();
  Cache_.push_back(std::move(e));
}

TilePool::Reply TilePool::FetchInto(const Data::Request &request, Landing *out) {
  if (!tCarries) {
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.FetchOnCompute++;
  }
  const std::string key = request.Key();
  Data::SourceSet::Query query = Sources_.Ask(request);
  Reply reply = Reply::Pending;
  const auto entered = std::chrono::steady_clock::now();
  double pollMs = 0.0;
  const int attempts = PollAttempts_ > 0 ? PollAttempts_ : kPollAttempts;
  for (int attempt = 0; attempt < attempts && reply == Reply::Pending; attempt++) {
    const auto t0 = std::chrono::steady_clock::now();
    Data::Delivery answer = Sources_.Collect(query, Wire_);
    pollMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    Data::Delivery::Answer taken;
    if (answer.TryTake(&taken)) {
      out->Bytes.assign(taken.Bytes.begin(), taken.Bytes.end());
      out->At = taken.At;
      Remember(key, out->Bytes.data(), out->Bytes.size(), taken.At, false);
      reply = Reply::Ready;
      break;
    }
    switch (answer.Where()) {
      case Data::Delivery::State::Pending:
        std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
        break;
      case Data::Delivery::State::Vacant:

        Remember(key, nullptr, 0, request.Where(), true);
        reply = Reply::Absent;
        break;
      case Data::Delivery::State::Undeclared:

        Log::Error("world", "tile_undeclared", {{"request", key}});
        reply = Reply::Undeclared;
        break;
      case Data::Delivery::State::Refused:
        Log::Error("world", "tile_refused", {{"request", key}});
        reply = Reply::Refused;
        break;
      case Data::Delivery::State::Delivered:
        break;
    }
  }
  if (reply == Reply::Pending) Sources_.Abandon(query, Wire_);
  const double blockedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - entered).count();
  tFetchBlockedMs += blockedMs;
  {
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.Fetches++;
    Ledger_.FetchMs += pollMs;
    Ledger_.FetchBlockedMs += blockedMs;
    if (reply == Reply::Ready) Ledger_.FetchedMB += (double)out->Bytes.size() / 1048576.0;
    if (reply == Reply::Absent) Ledger_.FetchAbsent++;

    if (reply == Reply::Refused || reply == Reply::Undeclared) Ledger_.FetchRefused++;
    if (reply == Reply::Pending) Ledger_.FetchGaveUp++;
  }
  if (reply != Reply::Ready) out->Bytes.clear();
  return reply;
}

TilePool::Reply TilePool::Bytes(const Data::Request &request, Landing *out) {
  const std::string key = request.Key();
  const Reply resident = Lookup(key, out);
  if (resident != Reply::Pending) return resident;
  Job job;
  job.Kind = Rank::Fetch;
  job.Key = RequestKey(key);
  job.Ask = request;
  Result result;
  const Reply posted = Poll(std::move(job), &result);
  if (posted == Reply::Pending) return Reply::Pending;

  if (posted == Reply::Undeclared) return Reply::Undeclared;
  return Lookup(key, out);
}

TilePool::Reply TilePool::BytesBlocking(const Data::Request &request, Landing *out) {
  const Reply resident = Lookup(request.Key(), out);
  if (resident != Reply::Pending) return resident;
  return FetchInto(request, out);
}

namespace {

class PoolTerrain : public TerrainSource {
 public:
  explicit PoolTerrain(TilePool &pool) : Pool_(pool) {}

  TerrainBytes Take(int z, uint32_t x, uint32_t y) override {
    const Data::Request request(Data::DataKind::Elevation, Data::Address::Tile(z, x, y));
    TilePool::Landing landing;
    switch (Pool_.BytesBlocking(request, &landing)) {
      case TilePool::Reply::Ready: return Answered(landing);
      case TilePool::Reply::Absent: return TerrainBytes::Nothing();

      case TilePool::Reply::Undeclared: return TerrainBytes::Nothing();
      case TilePool::Reply::Refused: return TerrainBytes::Wire();
      case TilePool::Reply::Pending: break;
    }
    return TerrainBytes::Waiting();
  }

 private:

  static TerrainBytes Answered(TilePool::Landing &landing) {
    int az = 0;
    uint32_t ax = 0, ay = 0;
    if (!landing.At.TryTile(&az, &ax, &ay)) return TerrainBytes::Wire();
    return TerrainBytes::From(az, ax, ay, std::move(landing.Bytes));
  }

 private:
  TilePool &Pool_;
};

}

namespace {

enum class Miss { None, Hole, Wait, Refused };

[[nodiscard]] Miss MissOf(TerrainMesh::State state) {
  switch (state) {
    case TerrainMesh::State::Built: return Miss::None;

    case TerrainMesh::State::NoTile: return Miss::Hole;
    case TerrainMesh::State::Deferred: return Miss::Wait;
    case TerrainMesh::State::SourceUndecodable:
    case TerrainMesh::State::SourceRefused:
    case TerrainMesh::State::FieldTooSmall:
    case TerrainMesh::State::StrideDoesNotDivide:
    case TerrainMesh::State::FrameUnusable: return Miss::Refused;
  }
  return Miss::Refused;
}

}

void TilePool::RunMesh(TerrainTiles &tiles, const Job &job, Result *out) {
  const TerrainMesh mesh = tiles.MeshOf(job.Z, job.X, job.Y);
  Chunk chunk = {};
  double origin[3] = {0.0, 0.0, 0.0};
  const char *stage = "source";
  Miss miss = MissOf(mesh.Where());
  if (miss == Miss::None &&
      (!ChunkBuildEcef(mesh, job.Z, job.X, job.Y, job.Grid, &chunk, origin) || chunk.nverts <= 0)) {
    miss = Miss::Refused;
    stage = "grid";
  }
  if (miss == Miss::None) {
    TileDagBuild((const float *)chunk.verts, chunk.nverts, chunk.gridverts, origin,
                 out->Build.Verts, out->Build.Idx, out->Build.Clusters);
    out->Build.ErrM = chunk.err;
    if (out->Build.Verts.empty() || out->Build.Idx.empty() || out->Build.Clusters.empty()) {
      miss = Miss::Refused;
      stage = "partition";
    } else {
      for (int a = 0; a < 3; a++) out->Build.OriginEcef[a] = origin[a];
    }
  }
  ChunkFree(&chunk);
  if (miss == Miss::None) {
    out->State = Reply::Ready;
    return;
  }
  if (miss == Miss::Refused) {
    Log::Warn("world", "tile_mesh_refused", {{"z", job.Z}, {"x", (int)job.X}, {"y", (int)job.Y},
                                             {"stage", stage}, {"rc", (int)mesh.Where()}});
    std::lock_guard<std::mutex> ledger(LedgerMutex_);
    Ledger_.MeshRefused++;
  }

  switch (miss) {
    case Miss::Hole: out->State = Reply::Absent; break;
    case Miss::Refused: out->State = Reply::Refused; break;
    case Miss::Wait:
    case Miss::None: out->State = Reply::Pending; break;
  }
}

void TilePool::RunDag(const Job &job, Result *out) {
  const uint32_t nverts = (uint32_t)(job.Soup.size() / kTileVertexFloats);
  if (nverts < 3) {
    out->State = Reply::Absent;
    return;
  }
  const float *soup = job.Soup.data();
  ClusterDag dag;
  ClusterDagOpts opts;
  if (job.SeamAttr >= 0 && job.SeamAttr < 8) opts.ClassOf = kSeam[job.SeamAttr];
  if (ClusterDagBuild(soup, nverts, 8, opts, &dag)) {
    out->Build.Verts = std::move(dag.Verts);
    out->Build.Idx = std::move(dag.Idx);
    out->Build.Clusters = std::move(dag.Clusters);
  } else {

    out->Build.Verts.assign(soup, soup + job.Soup.size());
    out->Build.Idx.resize(nverts);
    for (uint32_t i = 0; i < nverts; i++) out->Build.Idx[i] = i;
    DagCluster c{};
    c.Count = nverts;
    c.ParentErr = kDagRootErr;
    BoundingSphere(soup, nverts, 8, c.SelfCenter, &c.SelfRadius);
    out->Build.Clusters.push_back(c);
  }
  out->State = Reply::Ready;
}

void TilePool::Carry(void) {
  StackProbe::Enter(StackProbe::Purpose::Tile);
  tCarries = true;
  Landing scratch;
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(QueueMutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Carrying_.empty(); });
      if (Stopping_) { break; }
      job = std::move(Carrying_.front());
      Carrying_.erase(Carrying_.begin());
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    result.State = job.Ask ? FetchInto(*job.Ask, &scratch) : Reply::Refused;
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    {
      std::lock_guard<std::mutex> ledger(LedgerMutex_);
      Ledger_.FetchMs += spanMs;
      Ledger_.FetchBlockedMs += tFetchBlockedMs - blockedBefore;
    }
    {
      std::lock_guard<std::mutex> lock(QueueMutex_);
      if (result.State == Reply::Pending) {
        Posted_.erase(job.Key);
      } else if (Posted_.find(job.Key) != Posted_.end()) {
        Done_[job.Key] = std::move(result);
      }
      Landed_.notify_all();
    }
  }

}

void TilePool::Work(int slot) {
  StackProbe::Enter(StackProbe::Purpose::Tile);
  const EnuFrame frame = EnuFrame::At(OriginLatDeg_, OriginLonDeg_);

  if (frame.Where() != EnuFrame::State::Usable) {
    Log::Error("world", "tilepool_origin_too_polar", {{"lat", OriginLatDeg_}});
    std::abort();
  }
  PoolTerrain source(*this);
  TerrainTiles::Config config;
  config.DemCacheTiles = DemCacheTiles_;
  TerrainTiles tiles(source, frame, config);

  ContextBytes_[(size_t)slot].store(tiles.HeapBytes(), std::memory_order_relaxed);

  Landing scratch;
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(QueueMutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Queue_.empty(); });
      if (Stopping_) break;
      size_t best = 0;
      for (size_t i = 1; i < Queue_.size(); i++) {
        const Job &a = Queue_[i], &b = Queue_[best];
        if (a.Kind < b.Kind || (a.Kind == b.Kind && a.TileDist < b.TileDist)) best = i;
      }
      job = std::move(Queue_[best]);
      Queue_.erase(Queue_.begin() + (long)best);
    }
    Result result;
    const double blockedBefore = tFetchBlockedMs;
    const auto t0 = std::chrono::steady_clock::now();
    switch (job.Kind) {
      case Rank::Mesh:
        RunMesh(tiles, job, &result);
        break;
      case Rank::Dag:
        RunDag(job, &result);
        break;
      case Rank::Fetch:
        result.State = job.Ask ? FetchInto(*job.Ask, &scratch) : Reply::Refused;
        break;
    }
    const double spanMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    const double cpuMs = spanMs - (tFetchBlockedMs - blockedBefore);
    {
      std::lock_guard<std::mutex> ledger(LedgerMutex_);
      if (job.Kind == Rank::Mesh) {
        Ledger_.MeshTiles++;
        Ledger_.MeshCpuMs += cpuMs;
        if (result.State == Reply::Absent) Ledger_.MeshAbsent++;
      } else if (job.Kind == Rank::Dag) {
        Ledger_.Dags++;
        Ledger_.DagMs += cpuMs;
      }
    }
    StackProbe::Mark();
    ContextBytes_[(size_t)slot].store(tiles.HeapBytes(), std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(QueueMutex_);

      if (result.State == Reply::Pending) {
        Posted_.erase(job.Key);
        Landed_.notify_all();
      }

      else if (Posted_.find(job.Key) != Posted_.end()) {

        if (result.State == Reply::Absent || result.State == Reply::Undeclared)
          result.Build = TileBuild{};
        Done_[job.Key] = std::move(result);
        Landed_.notify_all();
      }
    }
  }
  ContextBytes_[(size_t)slot].store(0, std::memory_order_relaxed);
}

TilePool::Reply TilePool::Poll(Job &&job, Result *out) {
  std::unique_lock<std::mutex> lock(QueueMutex_);
  const auto done = Done_.find(job.Key);
  if (done != Done_.end()) {

    if (done->second.State == Reply::Absent || done->second.State == Reply::Undeclared) {
      out->State = done->second.State;
      return out->State;
    }
    *out = std::move(done->second);
    Done_.erase(done);
    Posted_.erase(job.Key);
    return out->State;
  }
  if (Posted_.insert(job.Key).second) {
    Posts_++;
    if (job.Kind == Rank::Mesh) job.TileDist = TileDistance(job.Z, job.X, job.Y);
    const bool carries = job.Kind == Rank::Fetch;
    (carries ? Carrying_ : Queue_).push_back(std::move(job));
    lock.unlock();
    Wake_.notify_all();
  } else {
    Repeats_++;
  }
  return Reply::Pending;
}

TilePool::Reply TilePool::Mesh(int z, uint32_t x, uint32_t y, int grid, TileBuild *out) {
  Job job;
  job.Kind = Rank::Mesh;
  job.Z = z;
  job.X = x;
  job.Y = y;
  job.Grid = grid;
  job.Key = MeshKey(z, x, y);
  Result result;
  const Reply state = Poll(std::move(job), &result);
  if (state == Reply::Ready) *out = std::move(result.Build);
  return state;
}

TilePool::Reply TilePool::MeshAwaited(int z, uint32_t x, uint32_t y, int grid,
                                      TileBuild *out) {
  const Reply asked = Mesh(z, x, y, grid, out);
  if (asked != Reply::Pending) { return asked; }
  const uint64_t key = MeshKey(z, x, y);
  {
    std::unique_lock<std::mutex> lock(QueueMutex_);
    Landed_.wait(lock, [&] {
      return Done_.find(key) != Done_.end() || Posted_.find(key) == Posted_.end();
    });
  }
  return Mesh(z, x, y, grid, out);
}

void TilePool::ForgetMesh(int z, uint32_t x, uint32_t y) {
  const uint64_t key = MeshKey(z, x, y);
  std::lock_guard<std::mutex> lock(QueueMutex_);
  Posted_.erase(key);
  Done_.erase(key);
}

bool TilePool::Known(uint64_t key) {
  std::lock_guard<std::mutex> lock(QueueMutex_);
  return Done_.find(key) != Done_.end() || Posted_.find(key) != Posted_.end();
}

TilePool::Reply TilePool::Dag(int id, const float *soup, int nverts, int seamAttr, TileBuild *out) {
  Job job;
  job.Kind = Rank::Dag;
  job.Key = DagKey(id);
  job.SeamAttr = seamAttr;

  if (!Known(job.Key)) job.Soup.assign(soup, soup + (size_t)nverts * kTileVertexFloats);
  Result result;
  const Reply state = Poll(std::move(job), &result);
  if (state == Reply::Ready) *out = std::move(result.Build);
  return state;
}

}
